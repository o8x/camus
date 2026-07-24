#include "writer.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

#include <build/build.h>

#include "common/error/error.h"
#include "common/filesystem/filesystem.h"
#include "common/functions/functions.h"
#include "common/logging/logging.h"
#include "common/net/net.h"
#include "common/render/render.h"
#include "common/str/str.h"
#include "inja/inja.hpp"
#include "yaml_config.h"

namespace camus
{
	void writer::run_only_live(const std::function<void()> &func) const
	{
		if (cmd_.dryrun) {
			return;
		}

		func();
	}

	void writer::add_write(const std::filesystem::path &path, std::string_view content)
	{
		if (cmd_.dryrun) {
			return;
		}

		if (pending_writes_.contains(path)) {
			logging::fatal("output file already queued dest={}", path.string());
		}

		pending_writes_[path] = content;
	}

	void writer::flush_writes()
	{
		for (const auto &[path, content] : pending_writes_) {
			std::filesystem::create_directories(path.parent_path());
			filesystem::write_file(path, content);
		}

		pending_writes_.clear();
	}

		void writer::emit_article()
	{
		resolve_directory_routes();
		parse_article_frontmatter();
		prepare_output_dir();

		auto articles = collect_articles();
		fill_article_nav(articles);

		const std::filesystem::path out_dir = std::filesystem::relative(conf_.camus().dest_dir(), cmd_.workdir);
		auto toc_items = build_toc_items();

		article_stats all_stats{};
		all_stats.min_time = std::numeric_limits<time_t>::max();
		std::unordered_map<std::string, article_stats> stats;
		stats["all"] = all_stats;

		for (auto *article : articles) {
			render_single_article(*article, toc_items, stats, out_dir);
		}

		render_stats_page(stats, toc_items);
		finalize_directory_properties();
	}

	void writer::resolve_directory_routes()
	{
		catalog::traverse_catalog_tree(catalog_, [&](catalog::catalog_node &node, int) {
			if (!node.is_directory()) {
				return;
			}

			node.path_prefix = conf_.camus().deploy.path_prefix;
			node.property.external_path = node.path;
			node.property.display_name = node.path.filename();

			if (const auto route = conf_.match_route(node.path)) {
				node.property.display_name = route->title;
				node.property.subtitle = route->subtitle;
				node.property.description = route->description;

				if (const std::filesystem::path tr_path = route->match_transfer(node.path); !tr_path.empty()) {
					node.property.external_path = tr_path;
				}
			}

			for (auto &c : node.children) {
				c.property.external_path = node.property.external_path;
			}
		});
	}

	void writer::parse_article_frontmatter()
	{
		catalog::traverse_catalog_tree(catalog_, [&](catalog::catalog_node &node, int) {
			if (node.path.extension() != ".md") {
				return;
			}

			node.path_prefix = conf_.camus().deploy.path_prefix;

			std::map<std::string, std::string> params;
			for (int i = 0; i < node.contents.size(); ++i) {
				const std::string line = strings::trim_space(node.contents[i]);
				if (i > 1 && line == "---") {
					node.contents.assign(node.contents.begin() + i, node.contents.end());
					break;
				}

				if (i == 0) {
					continue;
				}

				const auto [left, right] = strings::split_pair(line, ":", true);
				if (left.empty() && right.empty()) {
					logging::fatal("failed to parse params file={} line={}({})", node.path.string(), i, line);
				}

				params.emplace(left, right);
			}

			if (params["visibility"] == "hidden") {
				node.property.visibility = catalog::hidden;
				return;
			}

			bool hidden_flag = false;
			for (auto it = node.contents.begin(); it != node.contents.end();) {
				if (*it == "--hidden-section-start") {
					hidden_flag = true;
					it = node.contents.erase(it);
					continue;
				}

				if (*it == "--hidden-section-end") {
					hidden_flag = false;
					it = node.contents.erase(it);
					continue;
				}

				if (hidden_flag) {
					it = node.contents.erase(it);
				} else {
					++it;
				}
			}

			assert(!node.contents.empty());

			if (params.contains("tags")) {
				try {
					nlohmann::json::parse(params["tags"]).get_to(node.property.tags);
				} catch (const std::exception &e) {
					logging::fatal("parse article tags failed name={} error={}", node.path.string(), e.what());
				}
			}

			node.property.write_time = functions::datetime_to_unix(params["date"]);
			if (node.property.write_time <= 0) {
				node.property.write_time = 0;
				struct stat fileStat{};
				if (stat((conf_.camus().source_dir / node.path).c_str(), &fileStat) == 0) {
					node.property.write_time = fileStat.st_mtime;
				} else {
					logging::warn("failed to stat {}", node.path.string());
				}
			}

			node.property.visibility =
				params["visibility"] == "hidden_in_toc" ? catalog::hidden_in_toc : catalog::open;

			if (conf_.camus().output.filename_case == "uuid") {
				node.property.display_name = strings::uuid_v4();
			} else {
				node.property.display_name = params["display-name"];
			}

			if (node.property.display_name.empty()) {
				node.property.display_name = node.path.stem();
			}

			node.property.display_name = strings::replace(
				conf_.render_var(node.property.display_name),
				std::map<std::string, std::string>{
					{"/", " "},
					{"\\", " "},
					{"'", " "},
					{"\"", " "},
				}
			);

			if (const std::string alias = params["short-path"]; !alias.empty()) {
				const uint64_t hash = strings::make_hash(node.path);

				if (alias == "auto") {
					node.property.short_path = std::format("{:x}", hash % (UINT16_MAX * 2));
				} else {
					node.property.short_path = alias;
				}
			}
		});
	}

	void writer::prepare_output_dir() const
	{
		run_only_live([&]() {
			filesystem::empty_path(conf_.camus().output.dest_dir, true);
			filesystem::empty_path(conf_.camus().dest_dir(), true);
		});
	}

	std::vector<catalog::catalog_node *> writer::collect_articles()
	{
		std::vector<catalog::catalog_node *> articles;
		catalog::traverse_catalog_tree(catalog_, [&](catalog::catalog_node &node, int) {
			if (node.path.extension() == ".md" && node.property.visibility != catalog::hidden) {
				articles.push_back(&node);
			}
		});

		std::ranges::sort(articles, [](const catalog::catalog_node *a, const catalog::catalog_node *b) -> bool {
			return a->property.write_time > b->property.write_time;
		});

		return articles;
	}

	void writer::fill_article_nav(std::vector<catalog::catalog_node *> &articles) const
	{
		for (int i = 0; i < articles.size(); ++i) {
			auto &node = *articles[i];

			if (i + 1 < articles.size()) {
				node.render_data_.nav.next_path =
					filesystem::clean_path(articles[i + 1]->link_url(), conf_.camus().deploy.path_prefix);
				node.render_data_.nav.next_title = articles[i + 1]->property.display_name;
			}

			if (i > 0) {
				node.render_data_.nav.prev_path =
					filesystem::clean_path(articles[i - 1]->link_url(), conf_.camus().deploy.path_prefix);
				node.render_data_.nav.prev_title = articles[i - 1]->property.display_name;
			}
		}
	}

	std::vector<nlohmann::json> writer::build_toc_items() const
	{
		std::vector<nlohmann::json> items;
		for (const auto &child : catalog_.children) {
			if (child.property.visibility != catalog::hidden) {
				items.push_back(nlohmann::json(child));
			}
		}

		std::ranges::sort(items, [](const nlohmann::json &a, const nlohmann::json &b) -> bool {
			if (const bool a_dir = a.value("is_dir", false); a_dir != b.value("is_dir", false)) {
				return a_dir;
			}

			return a.value("write_time", 0) > b.value("write_time", 0);
		});

		return items;
	}

	void writer::accumulate_article_stats(
		const catalog::catalog_node &node,
		const size_t word_count,
		const double read_mins,
		std::unordered_map<std::string, article_stats> &stats
	)
	{
		auto &all = stats["all"];
		all.max_time = std::max(all.max_time, node.property.write_time);
		all.min_time = std::min(all.min_time, node.property.write_time);
		all.articles++;
		all.words += word_count;
		all.read_mins += read_mins;
		all.tags.insert(node.property.tags.begin(), node.property.tags.end());

		if (uint16_t year = 1970 + node.property.write_time / (365 * 86400); year > 1996 && year < 2200) {
			const std::string year_name = std::to_string(year);
			auto &yr = stats[year_name];
			if (yr.min_time == 0) {
				yr.min_time = std::numeric_limits<time_t>::max();
			}

			if (node.property.write_time > 0) {
				yr.max_time = std::max(yr.max_time, node.property.write_time);
				yr.min_time = std::min(yr.min_time, node.property.write_time);
			}

			yr.articles++;
			yr.words += word_count;
			yr.read_mins += read_mins;
			yr.tags.insert(node.property.tags.begin(), node.property.tags.end());
		}
	}

	void writer::render_single_article(
		catalog::catalog_node &node,
		const std::vector<nlohmann::json> &toc_items,
		std::unordered_map<std::string, article_stats> &stats,
		const std::filesystem::path &out_dir
	)
	{
		if (node.link_url().empty()) {
			logging::fatal(
				"make article failed file={} name={} dest={}{}",
				node.path.string(),
				node.property.display_name,
				out_dir.string(),
				node.link_url().string()
			);
		}

		std::string markdown = strings::string_join(node.contents, "\n");
		for (const auto &[k, v] : conf_.camus().envs) {
			markdown = strings::replace(markdown, "{{ env." + k + " }}", v);
			markdown = strings::replace(markdown, "{{env." + k + "}}", v);
		}

		const size_t word_count = strings::estimate_effective_length(markdown);
		const double read_mins = strings::estimate_reading_minutes(markdown);

		accumulate_article_stats(node, word_count, read_mins, stats);

		const std::string html = render::inja_render(
			inja_,
			render::render_markdown(
				markdown.data(),
				conf_.camus().render.markdown_engine,
				conf_.camus().render.markdown_options
			),
			conf_.json()
		);

		node.render_data_.stats.word_count = word_count;
		node.render_data_.stats.read_min = read_mins;
		node.render_data_.page.contents = html;

		nlohmann::json j = conf_.json();
		j.update(nlohmann::json(node));
		j["toc"] = toc_items;

		j["page"]["title"] = node.property.display_name;
		j["page"]["write_time"] = node.property.write_time;
		j["page"]["description"] = "";
		j["page"]["contents"] = html;
		j["stats"]["word_count"] = word_count;
		j["stats"]["read_min"] = read_mins;
		j["nav"] = node.render_data_.nav;

		logging::debug(
			"make article words={} name={} dest={}{}",
			word_count,
			node.path.string(),
			out_dir.string(),
			node.link_url().string()
		);
		assert(!node.contents.empty());

		const std::string contents = strings::replace(
			render::inja_render(inja_, conf_.camus().get_theme(config::CAMUS_THEME_TYPE_PAGE), j),
			std::map<std::string, std::string>{
				{" 00:00:00", ""},
				{"<img ", R"(<img width="100%")"},
				{"<hr>", ""},
				{"<hr/>", ""},
				{"<hr />", ""},
			},
			true
		);

		for (const auto &f : std::set{node.link_url(), node.real_url()}) {
			add_write(out_dir / filesystem::clean_path(f, "./"), contents);
		}
	}

	void writer::render_stats_page(
		const std::unordered_map<std::string, article_stats> &stats,
		const std::vector<nlohmann::json> &toc_items
	)
	{
		nlohmann::json j = conf_.json();
		j["toc"] = toc_items;
		j["stats"] = stats;

		const std::string contents =
			render::inja_render(inja_, conf_.camus().get_theme(config::CAMUS_THEME_TYPE_STATS), j);
		add_write(conf_.camus().dest_dir() / "stats.html", contents);
	}

	void writer::finalize_directory_properties()
	{
		catalog::traverse_catalog_tree(catalog_, [&](catalog::catalog_node &node, int) {
			if (!node.is_directory()) {
				return;
			}

			for (const auto &c : node.children) {
				node.property.write_time = std::max(node.property.write_time, c.property.write_time);
			}

			node.property.subtitle =
				node.property.subtitle.empty() ? conf_.camus().site.subtitle : node.property.subtitle;
			node.property.description =
				node.property.description.empty() ? conf_.camus().site.description : node.property.description;
			node.property.display_name =
				node.property.display_name.empty() ? conf_.camus().site.title : node.property.display_name;
		});

		catalog_.remove_children_if([](const catalog::catalog_node &a) -> bool {
			if (a.is_directory()) {
				return false;
			}

			return a.property.invalid();
		});
	}

	void writer::emit_friends()
	{
		std::vector<nlohmann::json> toc_items;
		for (const auto &child : catalog_.children) {
			if (child.property.visibility == catalog::hidden) {
				continue;
			}
			toc_items.push_back(nlohmann::json(child));
		}

		std::ranges::sort(toc_items, [](const nlohmann::json &a, const nlohmann::json &b) -> bool {
			if (const bool a_dir = a.value("is_dir", false); a_dir != b.value("is_dir", false)) {
				return a_dir;
			}

			return a.value("write_time", 0) > b.value("write_time", 0);
		});

		nlohmann::json json = conf_.json();
		json["toc"] = toc_items;
		json["friends"] = conf_.camus().friends;

		add_write(
			filesystem::clean_path("friends.html", conf_.camus().dest_dir()),
			render::inja_render(inja_, conf_.camus().get_theme(config::CAMUS_THEME_TYPE_FRIENDS), json)
		);
	}

	void writer::emit_toc()
	{
		if (cmd_.dryrun) {
			return;
		}

		if (conf_.camus().render.html_engine == "default") {
			struct dir_ctx {
				const catalog::catalog_node *parent = nullptr;
				std::vector<const catalog::catalog_node *> children;
			};

			std::map<const catalog::catalog_node *, dir_ctx> dirs;

			catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &node, int) {
				if (!node.is_directory()) {
					return;
				}

				auto &[parent, children] = dirs[&node];
				for (const auto &child : node.children) {
					if (child.is_directory()) {
						dirs[&child].parent = &node;
					}
					children.push_back(&child);
				}
			});

			nlohmann::json toc_full;
			toc_full["config"] = conf_.json();

			for (const auto &[dir_node, ctx] : dirs) {
				nlohmann::json toc_item;

				if (ctx.parent) {
					toc_item["parent"]["url"] = ctx.parent->link_url();
					if (toc_item["parent"]["label"] = ctx.parent->property.display_name;
						toc_item["parent"]["url"] == "/") {
						toc_item["parent"]["label"] = "/";
					}
				} else {
					toc_item["parent"]["url"] = "";
					toc_item["parent"]["label"] = "";
				}

				std::vector<nlohmann::json> items;
				for (const auto *child : ctx.children) {
					items.push_back(nlohmann::json(*child));
				}

				std::ranges::sort(items, [](const nlohmann::json &a, const nlohmann::json &b) -> bool {
					if (const bool a_dir = a.value("is_dir", false); a_dir != b.value("is_dir", false)) {
						return a_dir;
					}

					return a.value("write_time", 0) > b.value("write_time", 0);
				});

				toc_item["items"] = items;
				toc_item["current"] = *dir_node;

				toc_full["catalog"].push_back(toc_item);
				toc_item.update(conf_.json());

				add_write(
					filesystem::clean_path(
						std::format("{}/index.html", dir_node->real_url().string()),
						conf_.camus().dest_dir()
					),
					render::inja_render(inja_, conf_.camus().get_theme(config::CAMUS_THEME_TYPE_HOME), toc_item)
				);
			}

			if (conf_.camus().output.meta) {
				add_write(
					filesystem::clean_path("meta.json", conf_.camus().dest_dir()),
					toc_full.dump(4)
				);
			}

			return;
		}

		logging::fatal("html engine '{}' not supported", conf_.camus().render.html_engine);
	}

	void writer::emit_sitemap()
	{
		if (!conf_.camus().output.sitemap) {
			return;
		}

		std::vector<std::string> items;
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &node, int) {
			if (node.is_directory()) {
				return;
			}

			const std::string loc = std::format(
				"<url><loc>{}</loc><lastmod>{}</lastmod></url>",
				std::format("{}{}", conf_.camus().deploy.full_prefix(), node.link_url().string()),
				functions::format_time_t(node.property.write_time, "%Y-%m-%d")
			);

			items.push_back(loc);
		});

		logging::debug(std::format("make sitemap url={}/sitemap.xml", conf_.camus().deploy.full_prefix()));

		add_write(
			conf_.camus().dest_dir() / "sitemap.xml",
			std::format(
				R"(<?xml version="1.0" encoding="UTF-8"?><urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">{}</urlset>)",
				strings::string_join(items, "\n\t")
			)
		);
	}

	void writer::emit_assets()
	{
		const std::string assets_dir = conf_.camus().assets_dir;
		if (!std::filesystem::exists(assets_dir)) {
			return;
		}

		if (cmd_.dryrun) {
			return;
		}

		std::filesystem::create_directories(conf_.camus().dest_dir());

		// 整体拷贝资源文件夹
		if (std::filesystem::is_empty(assets_dir)) {
			logging::debug(std::format("skip empty assets directory: {}", assets_dir));
			return;
		}

		// 资源文件夹拷贝到目标目录
		const std::filesystem::path dest_dir = conf_.camus().dest_dir() / "assets";
		std::filesystem::copy(
			filesystem::clean_path(std::format("{}/assets", CAMUS_DIR), "./"),
			dest_dir,
			std::filesystem::copy_options::recursive
		);

		std::filesystem::copy(
			assets_dir,
			dest_dir,
			std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing
		);

		// 替换资源文件中的模板
		size_t count = 0;
		const nlohmann::json json = conf_.json();
		for (const auto &entry : std::filesystem::recursive_directory_iterator(dest_dir)) {
			if (!entry.is_regular_file()) {
				continue;
			}

			++count;
			if (const std::filesystem::path ext = entry.path().extension();
				ext == ".css" || ext == ".js" || ext == ".txt" || ext == ".json") {
				filesystem::write_file(
					entry.path(),
					render::inja_render(inja_, filesystem::read_file(entry.path()), json)
				);
			}
		}

		// 默认会将资源文件都提升到输出文件夹中，再删除原文件
		std::filesystem::copy(
			dest_dir,
			conf_.camus().dest_dir(),
			std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing
		);

		filesystem::empty_path(dest_dir, true);

		logging::debug(
			std::format(
				"assets: promote copy {}/... to {}/ files={}",
				std::filesystem::relative(assets_dir, cmd_.workdir).string(),
				std::filesystem::relative(dest_dir, cmd_.workdir).string(),
				count
			)
		);
	}

	writer::writer(const cmdline &cmd) : conf_(CAMUS_CONF_NAME), cmd_(cmd)
	{
		if (cmd_.workdir = filesystem::path_abs(cmd_.workdir); !std::filesystem::exists(cmd_.workdir)) {
			error::panic("work directory not exists name={}", cmd_.workdir.string());
		}

		inja_.add_callback("format_datetime", 1, [](const inja::Arguments &args) {
			const int unix_ts = args.at(0)->get<int>();
			return functions::format_time_t(unix_ts);
		});

		inja_.add_callback("format_date", 1, [](const inja::Arguments &args) {
			const int unix_ts = args.at(0)->get<int>();
			return functions::format_time_t(unix_ts, "%Y-%m-%d");
		});

		inja_.add_callback("with_prefix", 1, [&](const inja::Arguments &args) {
			const std::string name = args.at(0)->get<std::string>();
			return filesystem::clean_path(name, conf_.camus().deploy.path_prefix);
		});

		inja_.add_callback("with_cdn", 1, [&](const inja::Arguments &args) {
			const std::string name = args.at(0)->get<std::string>();

			// 退化为 with_prefix
			if (!conf_.camus().envs.contains("with_cdn")) {
				return filesystem::clean_path(name, conf_.camus().deploy.path_prefix);
			}

			return std::format(
				"{}/{}",
				strings::trim_right(conf_.camus().envs.at("cdn_accelerate"), "/"),
				strings::trim_left(filesystem::clean_path(name), "/")
			);
		});
	}

	void writer::watch()
	{
		constexpr uint16_t server_port = 3001;
		const std::string server_addr = net::get_local_addr();
		assert(!server_addr.empty());

		std::thread server_thread([&]() {
			watch_server_.set_mount_point("/", conf_.camus().dest_dir());
			watch_server_.listen(server_addr, server_port);
		});

		const std::filesystem::path watch_path = cmd_.workdir;
		logging::info("watch directory: {}", conf_.camus().dest_dir().string());
		logging::info("watch server listen on http://{}:{}", server_addr, server_port);

		while (true) {
			if (quit_) {
				break;
			}

			if (!std::filesystem::exists(watch_path)) {
				logging::fatal("watch directory not exists name={}", watch_path.string());
				continue;
			}

			std::stringstream buffer;
			for (const auto &entry : std::filesystem::recursive_directory_iterator(watch_path)) {
				if (const auto ext = entry.path().extension();
					ext == ".md" || ext == ".yaml" || ext == ".json" ||
					entry.path().string().find("/theme/") != std::string::npos) {
					buffer << filesystem::read_file(entry.path());
				}
			}

			// 如果文件 hash 不同，就重新编译一遍
			static uint64_t current_hash = strings::make_hash(buffer.str());
			if (const uint64_t hash = strings::make_hash(buffer.str()); current_hash != hash) {
				current_hash = hash;
				std::ignore = build();
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}

		watch_server_.stop();
		if (server_thread.joinable()) {
			server_thread.join();
		}
	}

	void writer::inspect()
	{
		cmd_.dryrun = true;
		conf_.reload();

		uint16_t max_length = 0;
		for (const auto &[k, v] : conf_.map()) {
			if (k.length() > max_length) {
				max_length = k.length();
			}
		}

		std::cout << std::endl << strings::coloring_yellow("Configure: ") << std::endl;
		for (const auto &[k, v] : conf_.map()) {
			std::cout << "    " << std::left << std::setw(max_length) << k << " : " << strings::coloring_green(v)
					  << std::endl
					  << std::flush;
		}

		if (catalog_ = catalog::build_catalog_tree(conf_.camus().source_dir); catalog_.empty()) {
			logging::warn("No articles found");
		}

		logging::set_level(logging::DEBUG_LEVEL);
		std::cout << std::endl << strings::coloring_yellow("Dry Build: ") << std::endl;
		build();
		logging::set_level(logging::DEBUG_LEVEL);

		max_length = 0;
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &item, const int depth) {
			if (const size_t w = strings::get_display_width(item.path); w > max_length) {
				max_length = w;
			}
		});

		std::cout << std::endl << strings::coloring_yellow("Catalog: ") << std::endl;
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &item, const int depth) {
			const int spaces = (depth == 0 ? 0 : depth - 1) * 4;

			std::string path = strings::padding_left(item.path, max_length);
			std::string name = item.link_url();
			if (item.link_url().parent_path() != "/") {
				path = strings::coloring_bright_green(path);
				name = strings::coloring_green(item.link_url());
			}

			std::cout << std::format(
							 "{}{}{} -> {}",
							 item.is_directory() ? strings::coloring_bright_green("[D] ") : "    ",
							 std::string(spaces, ' '),
							 path,
							 strings::coloring_simple(name)
						 )
					  << std::endl
					  << std::flush;
		});
	}

	int writer::build()
	{
		if (cmd_.dryrun) {
			logging::info("camus running on dryrun mode");
		}

		conf_.reload();

		std::filesystem::current_path(cmd_.workdir);

		if (catalog_ = catalog::build_catalog_tree(conf_.camus().source_dir); catalog_.empty()) {
			logging::warn("No articles found");
			return 1;
		}

		emit_article();
		emit_toc();
		emit_friends();
		emit_sitemap();
		emit_assets();

		flush_writes();

		logging::info("build complete articles={} output={}", catalog_.size(), conf_.camus().dest_dir().string());
		return 0;
	}
} // namespace camus
