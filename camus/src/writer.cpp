#include "writer.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string>
#include <vector>

#include "common/error/error.h"
#include "common/filesystem/filesystem.h"
#include "common/functions/functions.h"
#include "common/logging/logging.h"
#include "common/markdown/markdown.h"
#include "common/str/str.h"
#include "config.h"

struct article {
	enum visibility { open, hidden, hidden_in_toc };

	std::string uuid;
	time_t create_time;
	visibility visibility;
	std::string source_full_filename;
	std::string source_filename;
	std::string output_filename;
	uint32_t hidden_lines;
	std::string url;
	std::string display_name;
	std::vector<std::string> content;
	camus::site_toc_item toc;

	std::string link() const
	{
		return filesystem::clean_path(toc.url_path() + "/" + url);
	}

	std::string output_full_filename() const
	{
		return filesystem::clean_path(output_filename, toc.output_dir());
	}

	std::string join_content() const
	{
		return strings::string_join(content, "\n");
	}

	bool is_visible() const
	{
		return visibility != hidden;
	}

	bool is_toc_visible() const
	{
		return visibility != hidden && visibility != hidden_in_toc;
	}

	std::string to_json() const
	{
		std::ostringstream oss;

		oss << "{";
		if (camus::conf_loader::camus().filename_case == "uuid") {
			oss << R"("uuid": ")" << uuid << "\",";
		}

		oss << R"("create_time": )" << create_time << ",";
		oss << R"("url": ")" << url << "\",";
		oss << R"("display_name": ")" << display_name << "\"";
		oss << "}";

		return oss.str();
	}

	bool operator<(const article &other) const
	{
		return create_time > other.create_time;
	}
};

struct toc_item : camus::site_toc_item {
	time_t create_time;
	std::vector<article> articles;
};

template <> struct YAML::convert<article> {
	static Node encode(const article &a)
	{
		Node node;

		node["url"] = a.url;
		node["create_time"] = a.create_time;
		node["display_name"] = a.display_name;

		return node;
	}
}; // namespace YAML

template <> struct YAML::convert<toc_item> {
	static Node encode(const toc_item &a)
	{
		Node node;

		node["title"] = a.title;
		node["path"] = a.path;
		node["description"] = a.description;
		node["create_time"] = a.create_time;
		node["articles"] = a.articles;

		return node;
	}
}; // namespace YAML

namespace camus
{
	std::optional<article>
	parse_article(const std::filesystem::path &path, const std::filesystem::directory_entry &entry)
	{
		std::ifstream file(entry.path().string());
		if (!file.is_open()) {
			return std::nullopt;
		}

		article article{
			.uuid = strings::uuid_v4(),
			.create_time = time(nullptr),
			.visibility = article::open,
			.source_full_filename = entry.path().string(),
			.source_filename = entry.path().filename().string(),
			.hidden_lines = 0,
			.toc = conf_loader::site().find_toc(path.string().empty() ? "./" : path)
		};

		/**
		 *  ---
		 *   name: run-cmake
		 *   display-name: 创建和运行 CMake 项目
		 *   date: 2025-03-16
		 *   ready: true
		 *   ---
		 */
		std::string line;
		uint32_t params_count = 0;
		bool hidden_flag = false;
		while (getline(file, line)) {
			if (line == "---") {
				params_count++;
			}

			// 当参数出现两次时，开始处理 markdown
			if (params_count < 2 || line == "---") {
				auto set_string_parma = [&](const std::string &name, std::string &out) {
					if (out.empty() && line.find(name) != std::string::npos) {
						const std::vector<std::string> res = strings::split(line, name);
						out = strings::trim_space(res[1]);
					}
				};

				std::string datetime;
				std::string visibility;
				// 解析参数
				set_string_parma("date: ", datetime);
				set_string_parma("display-name: ", article.display_name);
				set_string_parma("visibility: ", visibility);

				if (visibility == "hidden") {
					article.visibility = article::hidden;
				}

				if (visibility == "hidden-in-toc") {
					article.visibility = article::hidden_in_toc;
				}

				if (!datetime.empty()) {
					article.create_time = functions::datetime_to_unix(strings::trim_space(datetime));
				}

				continue;
			}

			if (hidden_flag) {
				article.hidden_lines++;
			}

			if (strings::trim_space(line) == "--hidden-section-start") {
				hidden_flag = true;
				continue;
			}

			if (strings::trim_space(line) == "--hidden-section-end") {
				hidden_flag = false;
				continue;
			}

			article.content.push_back(line);
		}

		file.close();

		std::string name = article.uuid;
		article.url = name + ".html";

		if (conf_loader::camus().filename_case == "keep") {
			name = strings::replace(article.source_filename, ".md", "");
			article.url = strings::url_encode(name) + ".html";
		}

		article.display_name = strings::replace(article.display_name, "\"", "'");
		article.output_filename = std::format("{}.html", name);
		return article;
	}

	/**
	 * 页面介绍 {{page.description}}
	 * 主标题 {{site.title}}
	 * 页面标题 {{page.title}}
	 * 页面内容 {{page.content}}
	 */
	void write_articles(const std::vector<article> &pages)
	{
		std::string tpl;
		filesystem::with_current_dir(conf_loader::camus().work_dir, [&](const std::filesystem::path &) {
			tpl = filesystem::read_file(conf_loader::camus().theme_page);
		});

		assert(!tpl.empty());

		// 生成文章
		for (const auto &article : pages) {
			if (!article.is_visible()) {
				continue;
			}

			std::string hidden_flag;
			if (article.hidden_lines > 0) {
				hidden_flag = std::format("hidden={}line", article.hidden_lines);
			}

			logging::info("make article name={} link={} {}", article.output_filename, article.link(), hidden_flag);

			std::string markdown = strings::string_join(article.content, "\n");
			auto [length, to_html] = markdown::markdown_to_html(markdown.data());

			// 去掉无用的时间部分
			std::string date_str = strings::replace(functions::format_time_t(article.create_time), " 00:00:00", "");
			// 固定长度，避免读到脏内存
			std::string html_content =
				strings::replace(std::string(to_html, length), "<hr />", ""); // 去掉自动生成的 hr 标记
			free(to_html);

			std::string content = tpl;
			content = strings::replace(content, "{{page.title}}", article.display_name);
			content = strings::replace(content, "{{page.date}}", date_str);
			content = strings::replace(content, "{{page.description}}", "");
			content = strings::replace(content, "{{page.content}}", html_content);
			content = strings::replace(content, "{{page.info}}", article.to_json());
			content = conf_loader::render_var(content);

			std::filesystem::create_directories(article.toc.output_dir());
			const int n = filesystem::write_file(article.output_full_filename(), content);
			assert(n > 0);
		}
	}

	std::string toc_to_json(const std::vector<toc_item> &toc)
	{
		const auto root = YAML::Node(toc); // 显式转换
		YAML::Emitter out;
		out << YAML::DoubleQuoted << YAML::Flow; // JSON 样式
		out << root;
		return out.c_str();
	}

	void build_home_page(const std::vector<article> &pages)
	{
		std::string tpl;
		filesystem::with_current_dir(conf_loader::camus().work_dir, [&](const std::filesystem::path &) {
			tpl = filesystem::read_file(conf_loader::camus().theme_home);
		});

		filesystem::write_file("index.html", conf_loader::render_var(tpl));

		// 获取目录
		std::map<std::string, site_toc_item> tocs;
		for (const article &p : pages) {
			if (!p.is_toc_visible()) {
				continue;
			}

			tocs.emplace(p.toc.dir_name, p.toc);
		}

		std::vector<toc_item> toc_list;
		for (const auto &val : tocs | std::views::values) {
			toc_item item;
			item.dir_name = val.title;
			item.path = val.path;
			item.title = val.title;
			item.description = val.description;
			item.create_time = time(nullptr);

			for (const article &p : pages) {
				// 遍历该目录的内容
				if (!p.is_toc_visible() || val != p.toc) {
					continue;
				}

				// 获取最小时间
				if (p.create_time < item.create_time) {
					item.create_time = p.create_time;
				}

				item.articles.push_back(p);
			}

			// 按时间降序对文章排序
			std::ranges::sort(item.articles, [](const article &a, const article &b) {
				return a.create_time > b.create_time;
			});

			toc_list.push_back(item);
		}

		// 按时间降序对目录排序
		std::ranges::sort(toc_list, [](const toc_item &a, const toc_item &b) { return a.create_time > b.create_time; });

		const std::string toc_format = conf_loader::camus().toc_format;
		logging::info("make table of contents format={}", toc_format);

		if (toc_format == "all" || toc_format == "json") {
			filesystem::write_file("toc.json", toc_to_json(toc_list));
		}

		if (toc_format == "all" || toc_format == "javascript") {
			filesystem::write_file("toc.js", std::format(R"(const toc_json = {})", toc_to_json(toc_list)));
		}
	}

	void build_sitemap(const std::vector<article> &pages)
	{
		if (!conf_loader::camus().sitemap) {
			return;
		}

		std::vector<std::string> items;

		for (const auto &it : pages) {
			std::string url = std::format("{}/{}", conf_loader::site().url, it.url);
			url = strings::replace(url, "//", "/");

			std::string loc = std::format(
				"<url><loc>{}</loc><lastmod>{}</lastmod></url>",
				url,
				functions::format_time_t(it.create_time, "%Y-%m-%d")
			);

			items.push_back(loc);
		}

		logging::info(std::format("make sitemap url={}/sitemap.xml", conf_loader::site().url));

		filesystem::write_file(
			"sitemap.xml",
			std::format(
				R"(<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">{}</urlset>)",
				strings::string_join(items, "\n\t")
			)
		);
	}

	void copy_assets()
	{
		const std::string assets_dir = conf_loader::camus().assets_dir;
		if (!std::filesystem::exists(assets_dir)) {
			return;
		}

		// 整体拷贝资源文件夹
		if (std::filesystem::is_empty(assets_dir)) {
			logging::info(std::format("skip empty assets directory: {}", assets_dir));
		} else {
			// 默认会将资源文件都提升到输出文件夹中
			std::filesystem::copy(
				assets_dir,
				conf_loader::camus().output_dir,
				std::filesystem::copy_options::recursive
			);

			size_t count = 0;
			for (const auto &entry : std::filesystem::recursive_directory_iterator(assets_dir)) {
				if (entry.is_regular_file()) {
					++count;
				}
			}

			logging::info(
				std::format(
					"assets: promote copy {}/* to {}/* files={}",
					assets_dir,
					conf_loader::camus().output_dir,
					count
				)
			);
		}
	}

	int writer::generate() const
	{
		std::filesystem::current_path(work_dir_);

		conf_loader::get().parse_yaml("camus.yaml");
		const std::string read_dir = conf_loader::camus().source_dir;

		std::vector<article> pages;
		for (const auto &entry : std::filesystem::recursive_directory_iterator(read_dir)) {
			// 只处理 md
			if (entry.path().extension() != ".md") {
				continue;
			}

			const std::filesystem::path clean_path =
				filesystem::clean_path_prefix(entry.path(), conf_loader::camus().source_dir);
			std::optional<article> article = parse_article(clean_path.parent_path(), entry);
			if (!article.has_value()) {
				logging::info("parse params failed, skip: " + entry.path().string());
				continue;
			}

			pages.push_back(article.value());
		}

		std::filesystem::remove_all(conf_loader::camus().output_dir);
		std::filesystem::create_directory(conf_loader::camus().output_dir);
		filesystem::with_current_dir(conf_loader::camus().output_dir, [&](const std::filesystem::path &) {
			build_home_page(pages);
			write_articles(pages);
			build_sitemap(pages);
		});

		// 拷贝额外的导入内容
		copy_assets();
		return 0;
	}
} // namespace camus
