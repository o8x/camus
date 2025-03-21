#pragma once

#include <optional>
#include <ostream>
#include <sstream>
#include <vector>

namespace camus {
    struct article {
        bool ready;
        std::string uuid;
        std::string date;
        std::string filename;
        std::string out_filename;
        std::string short_path;
        std::string display_name;
        std::vector<std::string> content;

        std::string to_string() const {
            std::stringstream ss;

            ss << "{" << std::endl
                << "  ready: " << (ready ? "true" : "false") << "," << std::endl
                << "  uuid: " << uuid << "," << std::endl
                << "  date: " << date << "," << std::endl
                << "  filename: " << filename << "," << std::endl
                << "  short_path: " << short_path << "," << std::endl
                << "  display_name: " << display_name << "," << std::endl
                << "  out_filename: " << out_filename << "," << std::endl
                << "}";

            return ss.str();
        }
    };

    std::optional<article> parse_article_params(const std::string& path);

    // 生产主页
    void generate_index_page(const article& index);

    article get_index_template_from_file(const std::string& filename);
    article get_page_template_from_file(const std::string& filename);

    // 生成文章
    void generate_article_page(const article& tpl, const article& article);

    // 生成全部
    void generate_by_directory();
}
