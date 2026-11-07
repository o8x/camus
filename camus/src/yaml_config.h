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
		std::string cover_image;
		std::vector<route_transfer_conf> transfers;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(route_conf, path, title, subtitle, description, cover_image, transfers);

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

	inline constexpr const char *pdf_page_size_default = "A4";
	inline constexpr const char *pdf_page_margin_default = "20mm 18mm";
	inline constexpr const char *pdf_page_font_size_default = "10pt";

	struct pdf_page_conf {
		std::string size = pdf_page_size_default;
		std::string margin = pdf_page_margin_default;
		// 正文字号，仅支持 pt；其它字号由模板基于该值按比例计算
		std::string font_size = pdf_page_font_size_default;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(pdf_page_conf, size, margin, font_size);
	};

	enum pdf_sort_mode : uint8_t {
		PDF_SORT_FILE_FIRST = 0,
		PDF_SORT_DIR_FIRST = 1,
		PDF_SORT_MIXED = 2,
	};

	inline void to_json(nlohmann::json &json, const pdf_sort_mode &mode)
	{
		switch (mode) {
		case PDF_SORT_DIR_FIRST:
			json = "dir_first";
			break;
		case PDF_SORT_MIXED:
			json = "mixed";
			break;
		case PDF_SORT_FILE_FIRST:
		default:
			json = "file_first";
			break;
		}
	}

	inline void from_json(const nlohmann::json &json, pdf_sort_mode &mode)
	{
		const std::string value = json.is_string() ? json.get<std::string>() : "";
		if (value == "dir_first") {
			mode = PDF_SORT_DIR_FIRST;
		} else if (value == "mixed") {
			mode = PDF_SORT_MIXED;
		} else {
			mode = PDF_SORT_FILE_FIRST;
		}
	}

	struct pdf_conf {
		std::string engine;
		std::vector<std::string> build;
		pdf_page_conf page;
		pdf_sort_mode sort = PDF_SORT_FILE_FIRST;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(pdf_conf, engine, build, page, sort);
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
		int markdown_options = 0;
		// 主题数据
		std::unordered_map<camus_theme_type, std::string> theme;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(render_conf, theme_name, html_engine, markdown_engine);
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
		// PDF 引擎、参数与排序
		pdf_conf pdf;
		// 启动时自动更新策略：auto（下载并安装）/ check-only（仅检测）/ disable
		std::string auto_check_update = "auto";
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
