#include "writer.h"

#include <cassert>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

#include <build/build.h>
#include <unistd.h>

#include "common/error/error.h"
#include "common/filesystem/filesystem.h"
#include "common/functions/functions.h"
#include "common/logging/logging.h"
#include "common/net/net.h"
#include "common/render/render.h"
#include "common/str/str.h"
#include "inja/inja.hpp"
#include "yaml_config.h"

namespace
{
	bool command_exists(const std::string &name)
	{
		if (name.empty()) {
			return false;
		}

		if (name.contains('/')) {
			return access(name.c_str(), X_OK) == 0;
		}

		const char *env = std::getenv("PATH");
		if (env == nullptr) {
			return false;
		}

		for (const auto &dir : strings::split(env, ':')) {
			if (dir.empty()) {
				continue;
			}

			if (const std::filesystem::path candidate = std::filesystem::path(dir) / name;
				access(candidate.c_str(), X_OK) == 0) {
				return true;
			}
		}

		return false;
	}

	std::string find_command(const std::vector<std::string> &names)
	{
		for (const auto &name : names) {
			if (command_exists(name)) {
				return name;
			}
		}

		return "";
	}

	std::string resolve_pdf_engine(const std::string &engine)
	{
		const std::string name = strings::to_lower(strings::trim_space(engine));

		if (name.contains('/')) {
			return command_exists(name) ? name : "";
		}

		if (name == "chrome" || name == "chromium" || name == "google-chrome") {
#ifdef __APPLE__
			if (const std::string path = find_command(
					{"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
					 "/Applications/Chromium.app/Contents/MacOS/Chromium"}
				);
				!path.empty()) {
				return path;
			}
#endif
			return find_command({"google-chrome", "google-chrome-stable", "chromium", "chromium-browser"});
		}

		if (name == "edge" || name == "msedge" || name == "microsoft-edge") {
#ifdef __APPLE__
			if (const std::string path =
					find_command({"/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"});
				!path.empty()) {
				return path;
			}
#endif
			return find_command({"microsoft-edge", "microsoft-edge-stable", "microsoft-edge-beta"});
		}

		return "";
	}

	std::string path_to_file_url(const std::filesystem::path &path)
	{
		std::string url = "file://";
		const std::string value = path.string();
		for (const auto &part : strings::split(value, '/')) {
			if (!part.empty()) {
				url += "/" + strings::url_encode(part);
			}
		}

		return url.empty() ? "file:///" : url;
	}

	std::string shell_quote(const std::string &value)
	{
		return "'" + strings::replace(value, "'", "'\\''") + "'";
	}

	std::string html_escape(const std::string &value)
	{
		std::string out;
		out.reserve(value.size());
		for (const char c : value) {
			switch (c) {
			case '&':
				out += "&amp;";
				break;
			case '<':
				out += "&lt;";
				break;
			case '>':
				out += "&gt;";
				break;
			case '"':
				out += "&quot;";
				break;
			case '\'':
				out += "&#39;";
				break;
			default:
				out += c;
			}
		}

		return out;
	}

	constexpr std::string_view pdf_html_style = R"(
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
@page { size: A4; margin: 20mm 18mm; }
html { font-size: 16px; }
body {
    font-family: -apple-system, "Helvetica Neue", "PingFang SC", "Microsoft YaHei", Arial, sans-serif;
    color: #262626;
    line-height: 1.7;
    -webkit-print-color-adjust: exact;
    print-color-adjust: exact;
}
main { max-width: 100%; }
h1, h2, h3, h4, h5, h6 { color: #161616; line-height: 1.35; margin: 1.6em 0 0.6em; page-break-after: avoid; }
h1 { font-size: 1.75rem; }
h2 { font-size: 1.4rem; }
h3 { font-size: 1.15rem; }
p { margin: 0.7em 0; }
a { color: #0f62fe; text-decoration: none; }
blockquote { margin: 1em 0; padding: 0.6em 1em; border-left: 3px solid #c6c6c6; background: #f4f4f4; color: #525252; }
ul, ol { margin: 0.7em 0 0.7em 1.5em; }
li { margin: 0.25em 0; }
code { font-family: "IBM Plex Mono", "SFMono-Regular", Consolas, monospace; font-size: 0.88em; background: #f4f4f4; padding: 0.1em 0.3em; }
pre { margin: 1em 0; padding: 1em; background: #262626; color: #f4f4f4; overflow-x: auto; page-break-inside: avoid; }
pre code { background: transparent; color: inherit; padding: 0; }
img { max-width: 100%; height: auto; }
table { width: 100%; border-collapse: collapse; margin: 1em 0; font-size: 0.9rem; page-break-inside: avoid; }
th, td { padding: 0.4em 0.6em; border-bottom: 1px solid #e0e0e0; text-align: left; }
th { background: #f4f4f4; font-weight: 600; }
hr { border: none; border-top: 1px solid #e0e0e0; margin: 1.5em 0; }
.pdf-cover, .pdf-title-page, .pdf-toc { page-break-after: always; break-after: page; }
.pdf-cover { display: flex; flex-direction: column; justify-content: center; min-height: 235mm; text-align: center; }
.pdf-cover__eyebrow { font-size: 0.85rem; letter-spacing: 0.35em; text-transform: uppercase; color: #8d8d8d; margin-bottom: 2.2em; }
.pdf-cover__title { font-size: 2.8rem; line-height: 1.25; margin: 0 0 0.5em; }
.pdf-cover__subtitle { font-size: 1.2rem; color: #525252; margin: 0 auto 1.6em; max-width: 80%; }
.pdf-cover__desc { font-size: 0.95rem; color: #6f6f6f; margin: 0 auto; max-width: 72%; }
.pdf-cover__meta { margin-top: 3em; font-size: 0.9rem; color: #8d8d8d; letter-spacing: 0.12em; }
.pdf-title-page { display: flex; flex-direction: column; justify-content: center; min-height: 235mm; }
.pdf-title-page__inner { margin: 0 auto; max-width: 78%; }
.pdf-title-page__title { font-size: 2.2rem; margin: 0 0 0.4em; }
.pdf-title-page__subtitle { font-size: 1.05rem; color: #525252; margin: 0 0 2em; }
.pdf-title-page__rule { border: none; border-top: 2px solid #161616; width: 4.5em; margin: 0 0 2em; }
.pdf-meta { display: grid; grid-template-columns: auto 1fr; gap: 0.55em 1.6em; font-size: 0.92rem; }
.pdf-meta dt { color: #8d8d8d; }
.pdf-meta dd { color: #262626; }
.pdf-toc__heading { font-size: 1.9rem; margin: 0 0 1.4em; padding-bottom: 0.4em; border-bottom: 2px solid #161616; }
.pdf-toc__list { list-style: none; margin: 0; padding: 0; }
.pdf-toc__item { margin: 0; padding: 0; border-bottom: 1px dotted #d0d0d0; }
.pdf-toc__link { display: flex; align-items: baseline; gap: 1em; padding: 0.55em 0; color: #262626; }
.pdf-toc__num { flex: none; width: 2.4em; color: #8d8d8d; font-variant-numeric: tabular-nums; }
.pdf-toc__title { flex: 1; }
.pdf-toc__tags { flex: none; color: #8d8d8d; font-size: 0.82rem; }
.pdf-intro { page-break-after: always; break-after: page; }
.pdf-article { page-break-before: always; break-before: page; }
)";
} // namespace

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

			node.property.visibility = params["visibility"] == "hidden_in_toc" ? catalog::hidden_in_toc : catalog::open;

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
			filesystem::empty_path(conf_.camus().dest_dir(), true);
			filesystem::empty_path(conf_.camus().pdf_dir(), true);
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
		const catalog::catalog_node &node, const size_t word_count, const double read_mins,
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
		catalog::catalog_node &node, const std::vector<nlohmann::json> &toc_items,
		std::unordered_map<std::string, article_stats> &stats, const std::filesystem::path &out_dir
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
		const std::unordered_map<std::string, article_stats> &stats, const std::vector<nlohmann::json> &toc_items
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
				if (toc_item["parent"]["label"] = ctx.parent->property.display_name; toc_item["parent"]["url"] == "/") {
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
			add_write(filesystem::clean_path("meta.json", conf_.camus().dest_dir()), toc_full.dump(4));
		}
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
		if (cmd_.dryrun) {
			return;
		}

		const std::filesystem::path embedded_dir = filesystem::clean_path(std::format("{}/assets", CAMUS_DIR), "./");
		const std::filesystem::path user_dir = conf_.camus().assets_dir;

		const bool has_embedded =
			std::filesystem::is_directory(embedded_dir) && !std::filesystem::is_empty(embedded_dir);
		const bool has_user =
			!user_dir.empty() && std::filesystem::is_directory(user_dir) && !std::filesystem::is_empty(user_dir);

		if (!has_embedded && !has_user) {
			logging::debug("skip assets: no embedded or user assets found");
			return;
		}

		std::filesystem::create_directories(conf_.camus().dest_dir());

		// 资源文件夹拷贝到目标目录，用户资源整体覆盖内嵌资源
		const std::filesystem::path dest_dir = conf_.camus().dest_dir() / "assets";
		if (has_embedded) {
			std::filesystem::copy(
				embedded_dir,
				dest_dir,
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing
			);
		}

		if (has_user) {
			std::filesystem::copy(
				user_dir,
				dest_dir,
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing
			);
		}

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
				"assets: promote copy embedded={} user={} files={}",
				has_embedded ? std::filesystem::relative(embedded_dir, cmd_.workdir).string() : "-",
				has_user ? std::filesystem::relative(user_dir, cmd_.workdir).string() : "-",
				count
			)
		);
	}

	void writer::emit_pdf()
	{
		if (conf_.camus().render.pdf_engine.empty()) {
			return;
		}

		if (cmd_.dryrun) {
			return;
		}

		const std::string engine = resolve_pdf_engine(conf_.camus().render.pdf_engine);
		if (engine.empty()) {
			logging::warn("pdf engine not found, skip pdf generation name={}", conf_.camus().render.pdf_engine);
			return;
		}

		struct pdf_article {
			const catalog::catalog_node *node;
			std::string markdown;
		};

		std::vector<pdf_article> articles;
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &node, int) {
			if (node.path.extension() != ".md" || node.property.visibility == catalog::hidden) {
				return;
			}

			std::string markdown = strings::string_join(node.contents, "\n");

			articles.push_back({&node, std::move(markdown)});
		});

		if (articles.empty()) {
			logging::debug("skip pdf generation: no articles");
			return;
		}

		const std::string &site_title = conf_.camus().site.title;
		const std::string cover_title = html_escape(site_title.empty() ? "Camus" : site_title);
		const std::string cover_subtitle = html_escape(conf_.camus().site.subtitle);
		const std::string cover_desc = html_escape(conf_.camus().site.description);
		const std::string generated_at = functions::format_time_t(std::time(nullptr), "%Y-%m-%d");
		const std::string domain = html_escape(conf_.camus().deploy.domain_name);

		std::string markdown = std::format("# {}\n\n{}\n", site_title, conf_.camus().site.description);
		std::string toc_items;
		std::string body;

		body += "<section class=\"pdf-intro\">\n";
		body += render::render_markdown(
			markdown.c_str(),
			conf_.camus().render.markdown_engine,
			conf_.camus().render.markdown_options
		);
		body += "\n</section>\n";

		int index = 0;
		for (const auto &article : articles) {
			++index;

			const std::string label = article.node->property.display_name;
			markdown += std::format("\n\n---\n\n## {}\n\n{}", label, article.markdown);

			std::string tags;
			for (const auto &tag : article.node->property.tags) {
				if (!tags.empty()) {
					tags += " · ";
				}

				tags += html_escape(tag);
			}

			toc_items += std::format(
				"<li class=\"pdf-toc__item\"><a class=\"pdf-toc__link\" href=\"#pdf-article-{}\">"
				"<span class=\"pdf-toc__num\">{:02}</span><span class=\"pdf-toc__title\">{}</span>"
				"<span class=\"pdf-toc__tags\">{}</span></a></li>\n",
				index,
				index,
				html_escape(label),
				tags
			);

			const std::string article_markdown = std::format("## {}\n\n{}", label, article.markdown);
			body += std::format(
				"<section id=\"pdf-article-{}\" class=\"pdf-article\">\n{}\n</section>\n",
				index,
				render::render_markdown(
					article_markdown.c_str(),
					conf_.camus().render.markdown_engine,
					conf_.camus().render.markdown_options
				)
			);
		}

		const std::string front = std::format(
			"<section class=\"pdf-cover\">\n<div class=\"pdf-cover__eyebrow\">Camus</div>\n"
			"<h1 class=\"pdf-cover__title\">{}</h1>\n<p class=\"pdf-cover__subtitle\">{}</p>\n"
			"<p class=\"pdf-cover__desc\">{}</p>\n<div class=\"pdf-cover__meta\">{}</div>\n</section>\n"
			"<section class=\"pdf-title-page\">\n<div class=\"pdf-title-page__inner\">\n"
			"<h1 class=\"pdf-title-page__title\">{}</h1>\n<p class=\"pdf-title-page__subtitle\">{}</p>\n"
			"<hr class=\"pdf-title-page__rule\">\n<dl class=\"pdf-meta\">\n"
			"<dt>文章数</dt><dd>{}</dd>\n<dt>生成日期</dt><dd>{}</dd>\n"
			"<dt>站点</dt><dd>{}</dd>\n<dt>版本</dt><dd>v{}</dd>\n</dl>\n</div>\n</section>\n"
			"<section class=\"pdf-toc\">\n<h2 class=\"pdf-toc__heading\">目录</h2>\n"
			"<ol class=\"pdf-toc__list\">\n{}</ol>\n</section>\n",
			cover_title,
			cover_subtitle,
			cover_desc,
			generated_at,
			cover_title,
			cover_subtitle,
			articles.size(),
			generated_at,
			domain,
			PROJECT_VERSION,
			toc_items
		);

		const std::filesystem::path pdf_dir = conf_.camus().pdf_dir();
		const std::filesystem::path markdown_path = pdf_dir / "index.md";
		const std::filesystem::path html_path = pdf_dir / "index.html";
		std::filesystem::path pdf_path = pdf_dir / "index.pdf";

		std::filesystem::create_directories(pdf_dir);
		filesystem::write_file(markdown_path, markdown);

		const std::string html = std::format(
			"<!DOCTYPE html>\n<html lang=\"{}\">\n<head>\n<meta charset=\"utf-8\">\n"
			"<title>{}</title>\n<style>{}</style>\n</head>\n<body>\n{}\n<main>\n{}\n</main>\n</body>\n</html>\n",
			site_title.empty() ? "en" : "zh-CN",
			cover_title,
			pdf_html_style,
			front,
			body
		);

		filesystem::write_file(html_path, html);

		std::vector<std::string> args;
		bool has_headless = false;
		bool has_print_to_pdf = false;
		bool has_user_data_dir = false;

		for (const auto &option : conf_.camus().render.pdf_options) {
			if (option.empty()) {
				continue;
			}

			if (option.starts_with("--headless")) {
				has_headless = true;
			}

			if (option.starts_with("--print-to-pdf")) {
				if (has_print_to_pdf) {
					continue;
				}

				has_print_to_pdf = true;

				if (const auto pos = option.find('='); pos != std::string::npos) {
					std::string value = option.substr(pos + 1);
					if (!value.empty()) {
						value = strings::replace(value, "{{dest_dir}}", pdf_dir.string());
						value = strings::replace(value, "{{ dest_dir }}", pdf_dir.string());

						std::filesystem::path path(value);
						if (path.is_relative()) {
							path = pdf_dir / path;
						}

						pdf_path = path.lexically_normal();
					}
				}
				continue;
			}

			if (option.starts_with("--user-data-dir")) {
				has_user_data_dir = true;
			}

			args.push_back(option);
		}

		if (!has_headless) {
			args.insert(args.begin(), "--headless");
		}

		if (const std::filesystem::path parent = pdf_path.parent_path(); !parent.empty()) {
			std::filesystem::create_directories(parent);
		}

		args.push_back("--print-to-pdf=" + pdf_path.string());

		const std::filesystem::path profile_dir =
			std::filesystem::temp_directory_path() / std::format("camus-pdf-{}", getpid());
		std::error_code ec;
		std::filesystem::remove_all(profile_dir, ec);
		std::filesystem::create_directories(profile_dir, ec);

		if (!has_user_data_dir) {
			args.push_back("--user-data-dir=" + profile_dir.string());
		}

		args.push_back(path_to_file_url(html_path));

		std::string command = shell_quote(engine);
		for (const auto &arg : args) {
			command += " " + shell_quote(arg);
		}

		logging::debug("pdf command: {}", command);
		const int status = std::system(command.c_str());

		std::filesystem::remove_all(profile_dir, ec);

		if (status != 0) {
			logging::warn("pdf generation command failed status={}", status);
			return;
		}

		if (!std::filesystem::exists(pdf_path) || std::filesystem::file_size(pdf_path, ec) == 0) {
			logging::warn("pdf generation failed, output not found name={}", pdf_path.string());
			return;
		}

		logging::debug("make pdf dest={} engine={}", pdf_path.string(), engine);
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
			const auto deploy = conf_.camus().deploy;

			return std::format(
				"{}{}{}",
				deploy.protocol,
				deploy.domain_name,
				filesystem::clean_path(name, deploy.path_prefix)
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

			const std::string dist_prefix = conf_.camus().dest_dir().parent_path().string() + "/";
			std::stringstream buffer;
			for (auto it = std::filesystem::recursive_directory_iterator(watch_path);
				 it != std::filesystem::recursive_directory_iterator();
				 ++it) {
				const auto &entry = *it;
				if (entry.path().string().starts_with(dist_prefix)) {
					if (entry.is_directory()) {
						it.disable_recursion_pending();
					}
					continue;
				}

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
		emit_pdf();

		logging::info("build complete articles={} output={}", catalog_.size(), conf_.camus().dest_dir().string());
		return 0;
	}
} // namespace camus
