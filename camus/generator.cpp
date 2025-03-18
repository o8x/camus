#include "generator.h"

#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include "log.h"
#include "utils.h"
#include <maddy/parser.h>

namespace camus {
    std::optional<article> parse_article_params(const std::string &path, const std::string &out_directory) {
        std::ifstream file(path);

        if (!file.is_open()) {
            throw std::runtime_error("Could not open file " + path);
        }

        article article{
            .uuid = util::uuid_v4(),
            .date = util::get_now_time("%Y-%m-%d %H:%M:%S"),
            .filename = path
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
                auto set_string_parma = [&](const std::string &name, std::string &out) {
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

        std::filesystem::path name = article.short_path;
        if (name.empty()) {
            name = article.uuid;
        }

        article.out_filename = out_directory + "/" + name.string() + ".html";
        return article;
    }

    article get_article_template(const std::string &filename) {
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
    void generate_article_page(const article &tpl, const article &article) {
        std::stringstream input;
        input << util::join(article.content, "\n");

        std::shared_ptr<maddy::ParserConfig> config = std::make_shared<maddy::ParserConfig>();
        config->enabledParsers |= maddy::types::ALL;

        std::shared_ptr<maddy::Parser> parser = std::make_shared<maddy::Parser>(config);
        std::string htmlOutput = parser->Parse(input);

        log::info(htmlOutput);
    }

    void generate_index_page(const article &index) {
        log::info("generate index page ...");

        std::filesystem::copy_file(index.filename, index.out_filename);
    }

    void generate_by_directory(const std::string &read_directory, const std::string &out_directory) {
        article index_page;
        article page_template;
        std::vector<article> pages;
        for (const auto &entry: std::filesystem::directory_iterator(read_directory)) {
            if (!entry.is_regular_file()) {
                throw std::runtime_error("File is not a regular file " + entry.path().string());
            }

            if (entry.path().filename() == "article_template.html") {
                page_template = get_article_template(entry.path().string());
                continue;
            }

            if (entry.path().filename() == "index_template.html") {
                index_page = article{
                    .ready = true,
                    .uuid = util::uuid_v4(),
                    .date = "",
                    .filename = entry.path().string(),
                    .short_path = "index",
                    .display_name = "主页",
                    .out_filename = out_directory + "/index.html",
                };
                continue;
            }

            std::optional<article> article = parse_article_params(entry.path().string(), out_directory);
            pages.push_back(article.value());
        }

        // 生成 index
        generate_index_page(index_page);

        // 生成文章
        for (const article &page: pages) {
            if (!page.ready) {
                log::info("ignore article: " + page.display_name);
                continue;
            }

            log::info("generate article page '" + page.out_filename + "': " + page.display_name);
            generate_article_page(page_template, page);
        }
    }
}
