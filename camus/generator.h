#pragma once

#include <optional>
#include <ostream>
#include <sstream>
#include <filesystem>
#include <vector>

#include "utils.h"

namespace camus {
    struct article {
        bool ready;
        std::string uuid;
        time_t create_time;
        std::string filename;
        std::string full_filename;
        std::string out_filename;
        std::string short_path;
        std::string url;
        std::string display_name;
        std::vector<std::string> content;

        std::string join_content() const {
            return util::join(content, "\n");
        }

        std::string to_string() const {
            std::stringstream ss;

            ss << "{" << std::endl
                << "  ready: " << (ready ? "true" : "false") << "," << std::endl
                << "  uuid: " << uuid << "," << std::endl
                << "  date: " << util::format_time_t(create_time) << "," << std::endl
                << "  filename: " << filename << "," << std::endl
                << "  full_filename: " << full_filename << "," << std::endl
                << "  short_path: " << short_path << "," << std::endl
                << "  url: " << url << "," << std::endl
                << "  display_name: " << display_name << "," << std::endl
                << "  out_filename: " << out_filename << "," << std::endl
                << "}";

            return ss.str();
        }

        bool operator<(const article& other) const {
            return create_time > other.create_time;
        }
    };

    std::optional<article> parse_article_params(const std::filesystem::path& path);

    // 生产主页
    void generate_index_page(const article& index, std::vector<article> pages);

    article get_index_template_from_file(const std::string& filename);
    article get_page_template_from_file(const std::string& filename);

    // 生成文章
    bool generate_article_page(const article& tpl, const article& article);

    // 生成全部
    void generate_by_directory();
}
