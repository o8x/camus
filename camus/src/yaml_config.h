#pragma once

#include <expected>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "common/filesystem/filesystem.h"
#include "nlohmann/json.hpp"

namespace camus::config
{
	struct friend_link_conf {
		std::string title;
		std::string desc;
		std::string link_url;
		std::string image_url;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(friend_link_conf, title, desc, link_url, image_url);
	};

	struct route_transfer_conf {
		std::filesystem::path source;
		std::filesystem::path target;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(route_transfer_conf, source, target);
	};

	struct route_conf {
		std::string path;
		std::string title;
		std::string subtitle;
		std::string description;
		std::vector<route_transfer_conf> transfers;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(route_conf, path, title, subtitle, description, transfers);

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

	enum camus_theme_type : uint8_t {
		CAMUS_THEME_TYPE_HOME,
		CAMUS_THEME_TYPE_PAGE,
		CAMUS_THEME_TYPE_STATS,
		CAMUS_THEME_TYPE_FRIENDS,
	};

	struct render_conf {
		std::string theme_name = "default";
		std::string html_engine;
		std::string markdown_engine;
		int markdown_options;
		// 主题数据
		std::unordered_map<camus_theme_type, std::string> theme;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(render_conf, theme_name, markdown_engine, html_engine, markdown_engine);
	};

	struct output_conf {
		bool meta;
		bool sitemap;
		std::string filename_case = "keep";
		std::filesystem::path dest_dir = "";

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(output_conf, meta, sitemap, filename_case, dest_dir);
	};

	struct deploy_conf {
		std::string protocol;
		std::string domain_name;
		std::string path_prefix;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(deploy_conf, protocol, domain_name, path_prefix);

		std::string full_prefix()
		{
			return std::format("{}{}{}", protocol, domain_name, path_prefix);
		}
	};

	struct camus_conf {
		std::filesystem::path source_dir = "";
		std::filesystem::path assets_dir = "";
		// 站点属性
		route_conf site;
		// 输出
		output_conf output;
		// 部署配置
		deploy_conf deploy;
		// 渲染
		render_conf render;
		// 友情链接
		std::vector<friend_link_conf> friends;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(camus_conf, source_dir, assets_dir, site, output, deploy, render, friends);

		[[nodiscard]] std::filesystem::path dest_dir() const
		{
			return output.dest_dir;
		}

		std::string get_theme(const camus_theme_type &theme)
		{
			return render.theme.at(theme);
		}
	};

	class yaml_config
	{
		nlohmann::json public_config_;

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
