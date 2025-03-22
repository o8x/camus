#include "generator.h"

#include <cmark.h>
#include <fstream>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

#include "log.h"
#include "utils.h"
#include "config.h"

namespace camus {
    std::optional<article> parse_article_params(const std::filesystem::path& path) {
        std::ifstream file(path);

        if (!file.is_open()) {
            return std::nullopt;
        }

        article article{
            .ready = true, // 默认已经就绪
            .uuid = util::uuid_v4(),
            .date = util::get_now_time("%Y-%m-%d %H:%M:%S"),
            .filename = path.filename().string(),
            .full_filename = path.string(),
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
        while (getline(file, line)) {
            if (line == "---") {
                params_count++;
            }

            // 当参数出现两次时，开始处理 markdown
            if (params_count < 2) {
                auto set_string_parma = [&](const std::string& name, std::string& out) {
                    if (line.find(name) != std::string::npos) {
                        const std::vector<std::string> res = util::split(line, name);
                        out = util::trim_space(res[1]);
                    }
                };

                // 解析参数
                set_string_parma("date: ", article.date);
                set_string_parma("short_path: ", article.short_path);
                set_string_parma("display-name: ", article.display_name);

                if (line.find("ready: ") != std::string::npos) {
                    std::vector<std::string> res = util::split(line, "ready: ");
                    article.ready = res[1].find("true") != std::string::npos;
                }

                continue;
            }

            article.content.push_back(line);
        }

        file.close();

        std::string name = article.short_path;
        if (name.empty()) {
            if (ini::all().filename_type == "original_filename") {
                name = article.filename;
                util::replace_all(name, ".md", "");

                article.url = util::url_encode(name) + ".html";
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

    article get_index_template_from_file(const std::string& filename) {
        article home_template = get_page_template_from_file(filename);
        home_template.ready = true;
        home_template.uuid = util::uuid_v4();
        home_template.date = util::get_now_time("%Y-%m-%d");
        home_template.filename = filename;
        home_template.short_path = "index";
        home_template.display_name = "Home Page";
        home_template.out_filename = ini::all().out_directory + "/index.html";

        return home_template;
    }

    article get_page_template_from_file(const std::string& filename) {
        std::ifstream file(filename);

        if (!file.is_open()) {
            throw std::runtime_error("Could not open file " + filename);
        }

        article data{
            .date = "",
            .filename = filename,
        };

        std::string line;
        while (getline(file, line)) {
            data.content.push_back(line);
        }

        return data;
    }

    /**
     * 页面介绍 {{page-description}}
     * 主标题 {{main-title}}
     * 页面标题 {{page-title}}
     * 页面内容 {{page-content}}
     */
    bool generate_article_page(const article& tpl, const article& article) {
        log::info(std::format("generating article page: {}", article.out_filename));

        const std::string markdown = util::join(article.content, "\n");
        char* html = cmark_markdown_to_html(markdown.c_str(), markdown.length(), CMARK_OPT_DEFAULT);

        // 替换参数
        std::string t = tpl.join_content();

        ini::fill(t);
        util::replace_all(t, "{{page-title}}", article.display_name);
        util::replace_all(t, "{{page-date}}", article.date);
        util::replace_all(t, "{{page-description}}", "");
        util::replace_all(t, "{{page-content}}", html);

        // 写入文件
        std::ofstream writer(article.out_filename, std::ios::out);
        if (!writer.is_open()) {
            return false;
        }

        writer << t;
        writer.close();

        free(html);

        return true;
    }

    void generate_index_page(const article& index, std::vector<article> pages) {
        log::info("generating index page ...");

        std::string content = index.join_content();

        // 生成目录
        // name date link
        struct directory_item {
            std::string name;
            std::string date;
            std::string link;
        };


        std::vector<std::string> items;
        for (article& it : pages) {
            if (!it.ready) {
                continue;
            }

            util::replace_all(it.display_name, "\"", "'");

            items.push_back(std::format(
                R"({{"name":"{}", "date":"{}", "link":"{}"}})",
                it.display_name, it.date, it.url
            ));
        }

        const std::string items_string = util::join(items, ",\n");
        util::replace_all(content, "{{posts-item-json}}", "[" + items_string + "]");

        ini::fill(content);
        util::write_file(index.out_filename, content);
    }

    void generate_by_directory() {
        const std::string& read_directory = ini::all().posts_directory;
        const article page_template = get_page_template_from_file(ini::all().page_template_file);
        const article home_template = get_index_template_from_file(ini::all().home_template_file);

        std::vector<article> pages;
        for (const auto& entry : std::filesystem::directory_iterator(read_directory)) {
            const std::string full_filename = entry.path().string();
            const std::string filename = entry.path().filename().string();
            if (!entry.is_regular_file()) {
                throw std::runtime_error("File is not a regular file " + full_filename);
            }

            if (entry.path().filename().string().find(".html") != std::string::npos) {
                continue;
            }

            std::optional<article> article = parse_article_params(full_filename);
            if (!article.has_value()) {
                log::info("parse params failed, ignore: " + full_filename);
                continue;
            }

            article.value().filename = filename;
            pages.push_back(article.value());
        }

        // 生成 index
        generate_index_page(home_template, pages);

        // 生成文章
        for (const article& page : pages) {
            if (!page.ready) {
                log::info("ignore article: " + page.display_name);
                continue;
            }

            generate_article_page(page_template, page);
        }
    }
}
