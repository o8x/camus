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
		std::string html_engine = "default";
		std::string markdown_engine = "cmark-gfm";
		std::string pdf_engine;
		std::vector<std::string> pdf_options;
		int markdown_options = 0;
		// 主题数据
		std::unordered_map<camus_theme_type, std::string> theme;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(render_conf, theme_name, html_engine, markdown_engine, pdf_engine, pdf_options);
	};

	struct output_conf {
		bool meta = true;
		bool sitemap = true;
		std::string filename_case = "keep";
		std::filesystem::path dest_dir = "dist/html";

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(output_conf, meta, sitemap, filename_case, dest_dir);
	};

	struct public_url_conf {
		std::string url;
		std::string protocol;
		std::string domain_name;
		std::string path_prefix;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(public_url_conf, url, protocol, domain_name, path_prefix);

		[[nodiscard]] std::string full_prefix() const
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
		// 对外访问地址（自动拆解为 protocol/domain_name/path_prefix）
		public_url_conf public_url;
		// CDN 地址，为空时 with_cdn 原样返回
		std::string cdn_url;
		// 渲染
		render_conf render;
		// 友情链接
		std::vector<friend_link_conf> friends;

		[[nodiscard]] std::filesystem::path dest_dir() const
		{
			return output.dest_dir;
		}

		[[nodiscard]] std::filesystem::path pdf_dir() const
		{
			return output.dest_dir.parent_path() / "pdf";
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
		// 展平后的搬运规则（source 已解析为完整源目录路径）
		std::vector<route_transfer_conf> transfers_;

		const std::string filename_;

		// 监听模式：本地访问时 public_url / cdn_url 无效，解析后清空
		bool watch_mode_ = false;

		void supplement_other_vars();

	  public:
		explicit yaml_config(const std::string &filename);
		void set_watch_mode(bool enabled);
		void reload();

		[[nodiscard]] camus_conf camus() const;
		[[nodiscard]] nlohmann::json json() const;
		[[nodiscard]] std::map<std::string, std::string> map() const;
		[[nodiscard]] std::string render_var(const std::string &data) const;
		[[nodiscard]] std::expected<route_conf, std::string> match_route(const std::filesystem::path &path) const;
		// 在所有路由的 transfers 中查找匹配 path 的搬运目标（支持子目录 source）
		[[nodiscard]] std::filesystem::path match_transfer(const std::filesystem::path &path) const;
	};
} // namespace camus::config
