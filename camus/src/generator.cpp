#include "generator.h"

#include <fstream>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

#include "config.h"
#include "common/error/error.h"
#include "common/filesystem/filesystem.h"
#include "common/functions/functions.h"
#include "common/logging/logging.h"
#include "common/markdown/markdown.h"

namespace camus
{
	std::optional<article> parse_article_params(const std::filesystem::path &path)
	{
		std::ifstream file(path);

		if (!file.is_open()) {
			return std::nullopt;
		}

		article article{
			.uuid = functions::uuid_v4(),
			.create_time = time(nullptr),
			.filename = path.filename().string(),
			.visibility = article::open,
			.full_filename = path.string(),
			.hidden_lines = 0,
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
						const std::vector<std::string> res = functions::split(line, name);
						out = functions::trim_space(res[1]);
					}
				};

				std::string datetime;
				std::string visibility;
				// 解析参数
				set_string_parma("date: ", datetime);
				set_string_parma("short-path: ", article.short_path);
				set_string_parma("display-name: ", article.display_name);
				set_string_parma("visibility: ", visibility);

				if (visibility == "hidden") {
					article.visibility = article::hidden;
				}

				if (visibility == "hidden-in-toc") {
					article.visibility = article::hidden_in_toc;
				}

				if (!datetime.empty()) {
					article.create_time = functions::datetime_to_unix(functions::trim_space(datetime));
				}

				continue;
			}

			if (hidden_flag) {
				article.hidden_lines++;
			}

			if (functions::trim_space(line) == "--hidden-section-start") {
				hidden_flag = true;
				continue;
			}

			if (functions::trim_space(line) == "--hidden-section-end") {
				hidden_flag = false;
				continue;
			}

			article.content.push_back(line);
		}

		file.close();

		std::string name = article.short_path;
		if (name.empty()) {
			if (ini::all().filename_type == "original_filename") {
				name = article.filename;
				functions::replace_all(name, ".md", "");

				article.url = functions::url_encode(name) + ".html";
			} else {
				name = article.uuid;
				article.url = name + ".html";
			}
		} else {
			article.url = name + ".html";
		}

		article.out_filename = ini::all().out_directory + "/" + name + ".html";
		return article;
	}

	article get_index_theme_from_file(const std::string &filename)
	{
		article home_theme = get_page_theme_from_file(filename);
		home_theme.visibility = article::open;
		home_theme.uuid = functions::uuid_v4();
		home_theme.create_time = time(nullptr);
		home_theme.filename = filename;
		home_theme.short_path = "index";
		home_theme.display_name = "Home Page";
		home_theme.out_filename = ini::all().out_directory + "/index.html";

		return home_theme;
	}

	article get_page_theme_from_file(const std::string &filename)
	{
		std::ifstream file(filename);

		if (!file.is_open()) {
			error::panic("could not open theme name={}", filename);
		}

		article data{
			.filename = filename,
		};

		std::string line;
		while (getline(file, line)) {
			data.content.push_back(line);
		}

		return data;
	}

	/**
	 * 页面介绍 {{page.description}}
	 * 主标题 {{site.title}}
	 * 页面标题 {{page.title}}
	 * 页面内容 {{page.content}}
	 */
	bool generate_article_page(const article &tpl, const article &article)
	{
		std::string hidden_flag;
		if (article.hidden_lines > 0) {
			hidden_flag = std::format(", hidden: {} line", article.hidden_lines);
		}

		logging::info(std::format("generating: {}", article.out_filename, hidden_flag));

		std::string markdown = functions::string_join(article.content, "\n");
		auto [length, to_html] = markdown::markdown_to_html(markdown.data(), ini::all().markdown_engine);

		std::string t = tpl.join_content();
		std::string date_str = functions::format_time_t(article.create_time);
		std::string html_content(to_html, length); // 固定长度，避免读到脏内存

		free(to_html);

		// 替换参数
		ini::fill(t);
		functions::replace_all(date_str, " 00:00:00", "");	// 去掉无用的时间部分
		functions::replace_all(html_content, "<hr />", ""); // 去掉自动生成的 hr 标记
		functions::replace_all(t, "{{page.title}}", article.display_name);
		functions::replace_all(t, "{{page.date}}", date_str);
		functions::replace_all(t, "{{page.description}}", "");
		functions::replace_all(t, "{{page.content}}", html_content);
		functions::replace_all(t, "{{page.info}}", article.to_json());

		// 写入文件
		std::ofstream writer(article.out_filename, std::ios::out);
		if (!writer.is_open()) {
			return false;
		}

		writer << t;
		writer.flush();
		writer.close();

		return true;
	}

	void generate_index_page(const article &index, const std::string &toc)
	{
		logging::info(std::format("generating: {}", index.out_filename));

		std::string content = index.join_content();
		functions::replace_all(content, "{{posts-item-json}}", toc);

		ini::fill(content);
		filesystem::write_file(index.out_filename, content);
		filesystem::write_file(std::format("{}/toc.json", ini::all().out_directory), toc);
	}

	std::string generate_directory_json(std::vector<article> &pages)
	{
		// 生成目录
		std::vector<std::string> items;
		for (article &it : pages) {
			// 不在目录中生成
			if (it.visibility == article::hidden || it.visibility == article::hidden_in_toc) {
				continue;
			}

			functions::replace_all(it.display_name, "\"", "'");
			items.push_back(it.to_json());
		}

		const std::string items_string = functions::string_join(items, ",\n");
		return "[" + items_string + "]";
	}

	void generate_by_directory()
	{
		const std::string &read_directory = ini::all().posts_directory;
		const article page_theme = get_page_theme_from_file(ini::all().page_template);
		const article home_theme = get_index_theme_from_file(ini::all().home_template);

		std::vector<article> pages;
		for (const auto &entry : std::filesystem::directory_iterator(read_directory)) {
			const std::string full_filename = entry.path().string();
			const std::string filename = entry.path().filename().string();
			if (!entry.is_regular_file()) {
				error::panic("file is not a regular file={}", full_filename);
			}

			// 只处理 md
			if (entry.path().extension() != ".md") {
				continue;
			}

			std::optional<article> article = parse_article_params(full_filename);
			if (!article.has_value()) {
				logging::info("parse params failed, skip: " + full_filename);
				continue;
			}

			article.value().filename = filename;
			pages.push_back(article.value());
		}

		// 按日期排序
		std::sort(pages.begin(), pages.end());

		// 生成 index
		const std::string toc = generate_directory_json(pages);
		generate_index_page(home_theme, toc);

		// 生成文章
		for (article &page : pages) {
			if (!page.is_visible()) {
				logging::info("skip article: " + page.display_name);
				continue;
			}

			generate_article_page(page_theme, page);
		}

		// 整体拷贝资源文件夹
		if (std::filesystem::exists(ini::all().assets_directory)) {
			if (std::filesystem::is_empty(ini::all().assets_directory)) {
				logging::info(std::format("skip empty assets directory: {}", ini::all().assets_directory));
			} else {
				// 默认会将资源文件都提升到输出文件夹中
				std::filesystem::copy(
					ini::all().assets_directory,
					ini::all().out_directory,
					std::filesystem::copy_options::recursive
				);

				size_t count = 0;
				for (const auto &entry : std::filesystem::recursive_directory_iterator(ini::all().assets_directory)) {
					if (entry.is_regular_file()) {
						++count;
					}
				}

				logging::info(
					std::format(
						"assets: promote copy {}/* to {}/* files={}",
						ini::all().assets_directory,
						ini::all().out_directory,
						count
					)
				);
			}
		}

		if (ini::all().sitemap) {
			std::vector<std::string> items;

			for (const auto &it : pages) {
				std::string url = std::format("{}/{}", ini::all().site_homepage, it.url);
				functions::replace_all(url, "//", "/");

				std::string loc = std::format(
					"<url><loc>{}</loc><lastmod>{}</lastmod></url>",
					url,
					functions::format_time_t(it.create_time, "%Y-%m-%d")
				);

				items.push_back(loc);
			}

			const std::string filename = std::format("{}/sitemap.xml", ini::all().out_directory);

			logging::info(
				std::format("generating: sitemap file={} url={}/sitemap.xml", filename, ini::all().site_homepage)
			);

			filesystem::write_file(
				filename,
				std::format(
					R"(<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">{}</urlset>)",
					functions::string_join(items, "\n\t")
				)
			);
		}
	}
} // namespace camus
