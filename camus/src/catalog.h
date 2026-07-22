#pragma once

#include <functional>
#include <iostream>
#include <vector>

#include "common/filesystem/filesystem.h"
#include "common/functions/functions.h"
#include "common/str/str.h"
#include "nlohmann/json.hpp"

namespace camus::catalog
{
	enum catalog_visibility : uint8_t { open = 1, hidden = 2, hidden_in_toc = 3 };

	struct catalog_property {
		// 创建时间
		time_t write_time = 0;
		// 可见性
		catalog_visibility visibility;
		// 对外路径，处理 toc 之后的公开 URL，只保存路径
		std::filesystem::path external_path;
		// 文件的显示名称
		std::string display_name;
		// 短路径
		std::string short_path;
		// 副标题
		std::string subtitle;
		// 描述
		std::string description;
		// 标签
		std::vector<std::string> tags;

		[[nodiscard]] bool invalid() const
		{
			return display_name.empty();
		}
	};

	// 相对于 workdir 的内存目录树
	// path 为目录 property 无意义
	// path 为文件 name	   是文件名
	struct catalog_node {
		// 路径
		std::filesystem::path path;
		// 路径前缀，和主配置文件中 deploy.path_prefix 等同
		std::filesystem::path path_prefix;
		// 内容
		std::vector<std::string> contents;
		// 参数
		catalog_property property;
		// 递归子目录
		std::vector<catalog_node> children;

		friend bool operator<(const catalog_node &a, const catalog_node &b)
		{
			if (a.is_directory() || b.is_directory()) {
				return false;
			}

			return a.property.write_time > b.property.write_time;
		}

		template <
			typename BasicJsonType,
			nlohmann::detail::enable_if_t<nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>
		friend void to_json(BasicJsonType &json, const catalog_node &data)
		{
			json["title"] = data.property.display_name;
			json["write_time"] = data.property.write_time;
			json["path"] = filesystem::clean_path(data.real_url(), data.path_prefix);
			json["is_dir"] = data.is_directory();
			json["link_path"] = "";

			if (!data.property.short_path.empty()) {
				json["link_path"] = data.link_url();
			}

			if (data.is_directory()) {
				json["desc"] = data.property.description;
				json["subtitle"] = data.property.subtitle;
			} else {
				json["tags"] = data.property.tags;
			}
		}

		[[nodiscard]] std::string serialize_json() const;

		void remove_children_if(const std::function<bool(const catalog_node &)> &predicate);

		[[nodiscard]] std::filesystem::path link_url() const
		{
			if (!property.short_path.empty()) {
				return filesystem::clean_path("/s/" + property.short_path + ".html");
			}

			return real_url();
		}

		[[nodiscard]] std::filesystem::path real_url() const
		{
			if (is_directory()) {
				return filesystem::clean_path(property.external_path);
			}

			if (property.invalid()) {
				return "";
			}

			return filesystem::clean_path(property.external_path.string() + "/" + property.display_name + ".html");
		}

		[[nodiscard]] bool is_directory() const
		{
			return !children.empty();
		}

		[[nodiscard]] bool empty() const
		{
			return !contents.empty();
		}

		[[nodiscard]] uint16_t size();
	};

	// 遍历
	void
	traverse_catalog_tree(catalog_node &node, const std::function<void(catalog_node &, int depth)> &fn, int depth = 0);
	// 从根目录开始构建相对目录树
	catalog_node build_catalog_tree(const std::filesystem::path &path, const std::filesystem::path &relative_path = "");
} // namespace camus::catalog
