#include "catalog.h"

#include <algorithm>

#include <build/build.h>

#include "common/str/str.h"
#include "yaml_config.h"

namespace camus::catalog
{
	catalog_node build_catalog_tree(const std::filesystem::path &path, const std::filesystem::path &relative_path)
	{
		std::filesystem::path root = relative_path;
		if (root.empty()) {
			root = path;
		}

		catalog_node node;
		node.directory = true;
		if (node.path = std::filesystem::relative(path, root); node.path == ".") {
			node.path = "/";
		}

		if (!std::filesystem::is_directory(path)) {
			return node;
		}

		for (const auto &entry : std::filesystem::directory_iterator(path)) {
			// 必须先判断符号链接，is_directory/is_regular_file 会跟随链接，
			// 否则链接会被当成普通目录/文件递归或读取，甚至造成循环。
			if (entry.is_symlink()) {
				logging::fatal("catalog does not support symlinks name={}", filesystem::path_abs(entry.path()));
			}

			if (std::filesystem::is_directory(entry.path())) {
				if (entry.path().filename() == CAMUS_PRIVATE_DIR) {
					logging::debug("skip private directory name={}", filesystem::path_abs(entry.path()));
					continue;
				}

				// 如果有子节点或目录中包含文件，则添加
				if (catalog_node child = build_catalog_tree(entry.path(), root);
					!child.children.empty() || !filesystem::path_empty(entry.path())) {
					node.children.push_back(child);
				}
			} else if (std::filesystem::is_regular_file(entry.path())) {
				if (const std::filesystem::path filename = entry.path().filename();
					filename == CAMUS_CATALOG_NAME || filename == CAMUS_README_NAME) {
					logging::debug("skip metadata file name={}", filesystem::path_abs(entry.path()));
					continue;
				}

				catalog_node file_node;
				file_node.path = std::filesystem::relative(entry.path(), root);
				const std::filesystem::path filepath =
					relative_path.empty() ? root / file_node.path : relative_path / file_node.path;

				file_node.contents = strings::split(filesystem::read_file(filepath, true), "\n");
				node.children.push_back(file_node);
			} else {
				logging::fatal("catalog does not support file={}", entry.path().string());
			}
		}

		// directory_iterator 顺序未定义，排序保证构建结果可复现
		std::ranges::sort(node.children, [](const catalog_node &a, const catalog_node &b) {
			return a.path.string() < b.path.string();
		});

		return node;
	}

	std::string catalog_node::serialize_json() const
	{
		const nlohmann::json j = *this;
		return j.dump(4);
	}

	void catalog_node::remove_children_if(const std::function<bool(const catalog_node &)> &predicate)
	{
		// 先递归处理子节点内部
		for (auto &child : children) {
			child.remove_children_if(predicate);
		}

		// 删除当前层满足条件的子节点
		children.erase(std::ranges::remove_if(children, predicate).begin(), children.end());
	}

	size_t catalog_node::size()
	{
		size_t size = 0;
		traverse_catalog_tree(*this, [&](catalog_node &, int) { size++; });

		return size;
	}

	void
	traverse_catalog_tree(catalog_node &node, const std::function<void(catalog_node &, int depth)> &fn, const int depth)
	{
		fn(node, depth);

		for (size_t i = 0; i < node.children.size(); ++i) {
			traverse_catalog_tree(node.children[i], fn, depth + 1);
		}
	}
} // namespace camus::catalog
