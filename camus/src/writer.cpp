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
#include "common/markdown/markdown.h"
#include "common/net/net.h"
#include "common/str/str.h"
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

	void writer::emit_article()
	{
		// 解析目录链接
		catalog::traverse_catalog_tree(catalog_, [&](catalog::catalog_node &node, int) {
			if (!node.is_directory()) {
				return;
			}

			// 对外目录默认和自身同级
			node.property.external_path = node.path;

			if (const auto route = conf_.match_route(node.path)) {
				node.property.display_name = route->title;
				node.property.subtitle = route->subtitle;
				node.property.description = route->description;

				// 替换公开路径
				if (const std::filesystem::path tr_path = route->match_transfer(node.path); !tr_path.empty()) {
					node.property.external_path = tr_path;
				}
			}

			// 覆盖子级
			for (auto &c : node.children) {
				c.property.external_path = node.property.external_path;
			}
		});

		// 解析文件
		catalog::traverse_catalog_tree(catalog_, [&](catalog::catalog_node &node, int) {
			if (node.path.extension() != ".md") {
				return;
			}

			// 文件的内容
			std::map<std::string, std::string> params;
			for (int i = 0; i < node.contents.size(); ++i) {
				const std::string line = strings::trim_space(node.contents[i]);
				if (i > 1 && line == "---") {
					// 去掉前面的行
					node.contents.assign(node.contents.begin() + i, node.contents.end());
					break;
				}

				// 第1行是 ---
				if (i == 0) {
					continue;
				}

				const auto [left, right] = strings::split_pair(line, ":", true);
				if (left.empty() && right.empty()) {
					logging::fatal("failed to parse params file={} line={}({})", node.path.string(), i, line);
				}

				params.emplace(left, right);
			}

			// 忽略隐藏的文件
			if (params["visibility"] == "hidden") {
				return;
			}

			// 删除隐藏内容
			bool hidden_flag = false;
			for (auto it = node.contents.begin(); it != node.contents.end();) {
				if (*it == "--hidden-section-start") {
					hidden_flag = true;
				}

				if (hidden_flag) {
					it = node.contents.erase(it);
				} else {
					++it;
				}

				if (*it == "--hidden-section-end") {
					break;
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
				// 取文件创建时间
				struct stat fileStat{};
				if (stat((conf_.camus().source_dir / node.path).c_str(), &fileStat) == 0) {
					node.property.write_time = fileStat.st_mtime;
				} else {
					logging::warn("failed to stat {}", node.path.string());
				}
			}

			node.property.visibility = params["visibility"] == "hidden_in_toc" ? catalog::hidden_in_toc : catalog::open;
			if (conf_.camus().filename_case == "uuid") {
				node.property.display_name = strings::uuid_v4();
			} else {
				node.property.display_name = params["display-name"];
			}

			if (node.property.display_name.empty()) {
				node.property.display_name = node.path.stem();
			}

			// 所有的快捷路径都指向一个目录
			if (const std::string alias = params["short-path"]; !alias.empty()) {
				const uint64_t hash = strings::make_hash(node.path);

				if (alias == "auto") {
					node.property.short_path = std::format("{:x}", hash % (UINT16_MAX * 2));
				} else {
					node.property.short_path = alias;
				}
			}

			// 目前只支持 md 文件，都会自动编译 html，因此文件移动无意义
			// node.property.external_path

			// 合并替换，保留一个元素
			const std::string text = strings::string_join(node.contents, "\n");
			node.contents.clear();
			node.contents.push_back(
				strings::replace(
					conf_.render_var(conf_.camus().theme_page),
					std::map<std::string, std::string>{
						{"{{page.content}}",
						 markdown::render_markdown(
							 text.data(),
							 conf_.camus().render.engine,
							 conf_.camus().render.options
						 )},
						{"{{page.title}}", strings::replace(node.property.display_name, "\"", "'")},
						{"{{page.date}}", params["date"]},
						{"{{page.tags}}", params["tags"]},
						{"{{page.description}}", ""},
						{" 00:00:00", ""},
						{"<hr />", ""},
					}
				)
			);
		});

		run_only_live([&]() {
			// 清理输出文件夹
			filesystem::empty_path(conf_.camus().output_dir, true);
			// 写入 Index.html
			filesystem::write_file(conf_.camus().output_dir / "index.html", conf_.render_var(conf_.camus().theme_home));
		});

		// 写入其他文件
		const std::filesystem::path out_dir = std::filesystem::relative(conf_.camus().output_dir, cmd_.workdir);
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &node, int) {
			if (node.path.extension() != ".md") {
				return;
			}

			assert(!node.link_url().empty());

			const std::filesystem::path output_filename = out_dir / filesystem::clean_path(node.link_url(), "./");

			logging::debug(
				"make article name={} dest={}{}",
				node.path.string(),
				out_dir.string(),
				node.link_url().string()
			);
			assert(!node.contents.empty());

			run_only_live([&]() {
				if (std::filesystem::exists(output_filename)) {
					logging::fatal(
						"article already exist name={} dest={}{}",
						node.path.string(),
						out_dir.string(),
						node.link_url().string()
					);
				}

				// 空转模式支持
				std::filesystem::create_directories(output_filename.parent_path());
				filesystem::write_file(output_filename, node.contents[0]);
			});
		});

		// 填充文件夹属性
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

		// 删除无用的元素
		catalog_.remove_children_if([](const catalog::catalog_node &a) -> bool {
			if (a.is_directory()) {
				return false;
			}

			return a.property.invalid();
		});
	}

	void writer::emit_toc()
	{
		if (cmd_.dryrun) {
			return;
		}

		// 生成平铺的目录树
		std::vector<catalog::catalog_node> toc;
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &node, int) { toc.push_back(node); });

		const nlohmann::json json = toc;
		const std::string toc_json = json.dump(4);
		if (const std::string format = conf_.camus().toc_format; format == "all" || format == "json") {
			filesystem::write_file(conf_.camus().output_dir / "toc.json", toc_json);
		} else if (format == "all" || format == "javascript") {
			filesystem::write_file(
				conf_.camus().output_dir / "toc.json",
				std::format(R"(const toc_json = {})", toc_json)
			);
		}
	}

	void writer::emit_sitemap()
	{
		if (!conf_.camus().sitemap) {
			return;
		}

		std::vector<std::string> items;
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &node, int) {
			if (node.is_directory()) {
				return;
			}

			const std::string loc = std::format(
				"<url><loc>{}</loc><lastmod>{}</lastmod></url>",
				std::format("https://{}{}", conf_.camus().domain_name, node.link_url().string()),
				functions::format_time_t(node.property.write_time, "%Y-%m-%d")
			);

			items.push_back(loc);
		});

		logging::debug(std::format("make sitemap url=https://{}/sitemap.xml", conf_.camus().domain_name));

		run_only_live([&]() {
			filesystem::write_file(
				conf_.camus().output_dir / "sitemap.xml",
				std::format(
					R"(<?xml version="1.0" encoding="UTF-8"?><urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">{}</urlset>)",
					strings::string_join(items, "\n\t")
				)
			);
		});
	}

	void writer::emit_assets() const
	{
		const std::string assets_dir = conf_.camus().assets_dir;
		if (!std::filesystem::exists(assets_dir)) {
			return;
		}

		if (cmd_.dryrun) {
			return;
		}

		// 整体拷贝资源文件夹
		if (std::filesystem::is_empty(assets_dir)) {
			logging::debug(std::format("skip empty assets directory: {}", assets_dir));
			return;
		}

		// 默认会将资源文件都提升到输出文件夹中
		std::filesystem::copy(assets_dir, conf_.camus().output_dir, std::filesystem::copy_options::recursive);

		size_t count = 0;
		for (const auto &entry : std::filesystem::recursive_directory_iterator(assets_dir)) {
			if (entry.is_regular_file()) {
				++count;
			}
		}

		logging::debug(
			std::format(
				"assets: promote copy {}/... to {}/ files={}",
				std::filesystem::relative(assets_dir, cmd_.workdir).string(),
				std::filesystem::relative(conf_.camus().output_dir, cmd_.workdir).string(),
				count
			)
		);
	}

	writer::writer(const cmdline &cmd) : conf_(CAMUS_CONF_NAME), cmd_(cmd)
	{
		if (cmd_.workdir = filesystem::path_abs(cmd_.workdir); !std::filesystem::exists(cmd_.workdir)) {
			error::panic("work directory not exists name={}", cmd_.workdir.string());
		}
	}

	void writer::watch()
	{
		constexpr uint16_t server_port = 3001;
		const std::string server_addr = net::get_local_addr();
		assert(!server_addr.empty());

		std::thread server_thread([&]() {
			watch_server_.set_mount_point("/", conf_.camus().output_dir);
			watch_server_.listen(server_addr, server_port);
		});

		logging::info("watch directory: {}", conf_.camus().source_dir.string());
		logging::info("watch server listen on http://{}:{}", server_addr, server_port);

		while (true) {
			if (quit_) {
				break;
			}

			if (!std::filesystem::exists(conf_.camus().source_dir)) {
				logging::fatal("watch directory not exists name={}", conf_.camus().source_dir.string());
				continue;
			}

			std::stringstream buffer;
			for (const auto &entry : std::filesystem::recursive_directory_iterator(conf_.camus().source_dir)) {
				if (entry.path().extension() == ".md") {
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

		build();
		logging::set_level(logging::INFO_LEVEL);

		uint16_t max_length = 0;
		for (const auto &[k, v] : conf_.map()) {
			if (k.length() > max_length) {
				max_length = k.length();
			}
		}

		max_length *= 1.2;

		std::cout << "File System: " << std::endl;
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &item, const int depth) {
			const int spaces = (depth == 0 ? 0 : depth - 1) * 4;
			std::string path = item.path.string();
			path.resize(max_length - spaces, ' ');

			std::cout << std::format(
							 "{} {}{} -> {}",
							 item.children.empty() ? "   " : "[D]",
							 std::string(spaces, ' '),
							 path,
							 strings::coloring_green(item.link_url())
						 )
					  << std::endl
					  << std::flush;
		});

		std::cout << "Configure: " << std::endl;
		for (const auto &[k, v] : conf_.map()) {
			std::cout << "    " << std::left << std::setw(max_length) << k << ": " << strings::coloring_green(v)
					  << std::endl
					  << std::flush;
		}
	}

	int writer::build()
	{
		if (cmd_.dryrun) {
			logging::info("camus running on dryrun mode");
		}

		std::filesystem::current_path(cmd_.workdir);

		if (catalog_ = catalog::build_catalog_tree(conf_.camus().source_dir); catalog_.empty()) {
			logging::warn("No articles found");
			return 1;
		}

		emit_article();
		emit_toc();
		emit_sitemap();
		emit_assets();

		logging::info("build complete articles={} output={}", catalog_.size(), conf_.camus().output_dir.string());
		return 0;
	}
} // namespace camus
