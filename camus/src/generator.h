#pragma once

#include "config.h"

#include <optional>
#include <sstream>
#include <filesystem>
#include <vector>

#include "common/functions/functions.h"

namespace camus
{
	struct article {
		enum visibility { open, hidden, hidden_in_toc };

		std::string uuid;
		time_t create_time;
		std::string filename;
		visibility visibility;
		std::string full_filename;
		uint32_t hidden_lines;
		std::string out_filename;
		std::string short_path;
		std::string url;
		std::string display_name;
		std::vector<std::string> content;

		std::string join_content() const
		{
			return functions::string_join(content, "\n");
		}

		bool is_visible() const
		{
			return visibility != hidden;
		}

		std::string to_json() const
		{
			std::ostringstream oss;

			oss << "{";
			if (conf_loader::camus().filename_case == "uuid") {
				oss << R"("uuid": ")" << uuid << "\",";
			}

			oss << R"("create_time": )" << create_time << ",";
			oss << R"("short_path": ")" << short_path << "\",";
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

	std::optional<article> parse_article_params(const std::filesystem::path &path);

	// 生产主页
	void generate_toc_home(const std::vector<article> &pages);

	article get_index_theme_from_file(const std::string &filename);
	article get_page_theme_from_file(const std::string &filename);

	// 生成文章
	bool generate_article_page(const article &article);

	// 生成全部
	void generate_from_directory(const std::string &read_dir, const std::string &write_dir);
} // namespace camus
