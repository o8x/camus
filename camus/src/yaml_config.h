#pragma once

#include <expected>
#include <map>
#include <string>
#include <vector>

#include "common/filesystem/filesystem.h"
#include "nlohmann/json.hpp"

namespace camus::config
{
	struct route_transfer_conf {
		std::filesystem::path source;
		std::filesystem::path target;
	};

	struct route_conf {
		std::string path;
		std::string title;
		std::string subtitle;
		std::string description;
		std::vector<route_transfer_conf> transfers;

		[[nodiscard]] std::filesystem::path match_transfer(const std::filesystem::path &path) const
		{
			for (const auto &[source, target] : transfers) {
				if (filesystem::path_equal(path, source, true)) {
					return target;
				}
			}

			return {};
		}
	};

	struct render_conf {
		std::string static_engine;
		std::string engine;
		int options;
	};

	struct camus_conf {
		bool sitemap;
		std::filesystem::path source_dir = "";
		std::filesystem::path output_dir = "";
		std::filesystem::path assets_dir = "";
		std::string domain_name;
		std::string theme_home;
		std::string theme_page;
		std::string theme_stats;
		std::string filename_case = "keep";
		std::string toc_format = "json";
		// 站点属性
		route_conf site;
		// 渲染
		render_conf render;
	};

	class yaml_config
	{
		std::map<std::string, std::string> flattened_map;
		camus_conf camus_;
		std::vector<route_conf> routes_;

		const std::string filename_;

		void supplement_other_vars();

	  public:
		explicit yaml_config(const std::string &filename);
		void reload();

		[[nodiscard]] camus_conf camus() const;
		[[nodiscard]] nlohmann::json json() const;
		[[nodiscard]] std::map<std::string, std::string> map() const;
		[[nodiscard]] std::string render_var(const std::string &data) const;
		[[nodiscard]] std::expected<route_conf, std::string> match_route(const std::filesystem::path &path) const;
	};
} // namespace camus::config
