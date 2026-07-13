#include "catalog.h"

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
		if (node.path = std::filesystem::relative(path, root); node.path == ".") {
			node.path = "/";
		}

		if (!std::filesystem::is_directory(path)) {
			return node;
		}

		for (const auto &entry : std::filesystem::directory_iterator(path)) {
			if (std::filesystem::is_directory(entry.path())) {
				// 如果有子节点或目录中包含文件，则添加
				if (catalog_node child = build_catalog_tree(entry.path(), root);
					!child.children.empty() || !filesystem::path_empty(entry.path())) {
					node.children.push_back(child);
				}
			} else if (std::filesystem::is_regular_file(entry.path())) {
				catalog_node file_node;
				file_node.path = std::filesystem::relative(entry.path(), root);
				std::filesystem::path filepath;
				if (relative_path.empty()) {
					filepath = root / file_node.path;
				} else {
					filepath = relative_path / file_node.path;
				}

				file_node.contents = strings::split(filesystem::read_file(filepath, true), "\n");
				node.children.push_back(file_node);
			} else if (std::filesystem::is_symlink(entry.path())) {
				logging::fatal("catalog does not support symlinks name={}", filesystem::path_abs(entry.path()));
			} else {
				logging::fatal("catalog does not support file={}", entry.path().string());
			}
		}

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

	uint16_t catalog_node::size()
	{
		uint16_t size = 0;
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
