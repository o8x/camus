#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace camus::paths
{
	// 平台默认数据目录：macOS 为 ~/Library/Application Support/Camus，Linux 为 /var/lib/camus
	[[nodiscard]] std::filesystem::path data_dir();
	// 候选数据目录，按优先级排列（macOS 会追加系统级 /Library/Application Support/Camus）
	[[nodiscard]] std::vector<std::filesystem::path> data_dirs();
	// 在候选数据目录下查找相对路径，返回第一个存在的路径；找不到返回空路径
	[[nodiscard]] std::filesystem::path find(const std::filesystem::path &relative_path);
	// 默认静态资源目录：第一个存在且非空的 <data_dir>/assets；找不到返回空路径
	[[nodiscard]] std::filesystem::path default_assets_dir();
} // namespace camus::paths
