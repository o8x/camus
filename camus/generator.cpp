#include "generator.h"

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
            .create_time = time(nullptr),
            .filename = path.filename().string(),
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
                auto set_string_parma = [&](const std::string& name, std::string& out) {
                    if (out.empty() && line.find(name) != std::string::npos) {
                        const std::vector<std::string> res = util::split(line, name);
                        out = util::trim_space(res[1]);
                    }
                };

                std::string datetime;
                // 解析参数
                set_string_parma("date: ", datetime);
                set_string_parma("short-path: ", article.short_path);
                set_string_parma("display-name: ", article.display_name);

                if (!datetime.empty()) {
                    article.create_time = util::datetime_to_unix(util::trim_space(datetime));
                }

                if (line.find("ready: ") != std::string::npos) {
                    std::vector<std::string> res = util::split(line, "ready: ");
                    article.ready = res[1].find("true") != std::string::npos;
                }

                continue;
            }

            if (hidden_flag) {
                article.hidden_lines++;
            }

            if (util::trim_space(line) == "--hidden-section-start") {
                hidden_flag = true;
                continue;
            }

            if (util::trim_space(line) == "--hidden-section-end") {
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
        home_template.create_time = time(nullptr);
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
        std::string hidden_flag;
        if (article.hidden_lines > 0) {
            hidden_flag = std::format(", hidden: {} line", article.hidden_lines);
        }

        log::info(std::format(
            "generating: {}", article.out_filename, hidden_flag
        ));

        std::string markdown = util::join(article.content, "\n");
        auto [length , to_html] = util::markdown_to_html(markdown.data(), ini::all().markdown_engine == "cmark");

        std::string t = tpl.join_content();
        std::string date_str = util::format_time_t(article.create_time);
        std::string html_content(to_html, length); // 固定长度，避免读到脏内存

        free(to_html);

        // 替换参数
        ini::fill(t);
        util::replace_all(date_str, " 00:00:00", ""); // 去掉无用的时间部分
        util::replace_all(html_content, "<hr />", ""); // 去掉自动生成的 hr 标记
        util::replace_all(t, "{{page-title}}", article.display_name);
        util::replace_all(t, "{{page-date}}", date_str);
        util::replace_all(t, "{{page-description}}", "");
        util::replace_all(t, "{{page-content}}", html_content);

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

    void generate_index_page(const article& index, std::vector<article> pages) {
        log::info(std::format("generating: {}", index.out_filename));

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
                it.display_name, util::format_time_t(it.create_time, "%Y-%m-%d"), it.url
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

        // 按日期排序
        std::sort(pages.begin(), pages.end());

        // 生成 index
        generate_index_page(home_template, pages);

        // 生成文章
        for (const article page : pages) {
            if (!page.ready) {
                log::info("ignore article: " + page.display_name);
                continue;
            }

            generate_article_page(page_template, page);
        }
    }
}
