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
	// 工作目录下的本地资源目录 .camus（由 --install 生成）
	[[nodiscard]] std::filesystem::path local_data_dir();
	// 在候选数据目录下查找相对路径，返回第一个存在的路径；找不到返回空路径
	[[nodiscard]] std::filesystem::path find(const std::filesystem::path &relative_path);
	// 资源查找：优先工作目录 .camus，其次平台数据目录；找不到返回空路径
	[[nodiscard]] std::filesystem::path find_resource(const std::filesystem::path &relative_path);
	// 本地覆盖查找：优先工作目录下的相对路径，其次按 find_resource 查找；找不到返回空路径
	[[nodiscard]] std::filesystem::path find_local_or_resource(const std::filesystem::path &relative_path);
	// --install 的资源来源：第一个包含 theme 与 assets 的数据目录；找不到返回空路径
	[[nodiscard]] std::filesystem::path source_data_dir();
	// 默认静态资源目录：优先 .camus/assets，其次数据目录 assets；找不到返回空路径
	[[nodiscard]] std::filesystem::path default_assets_dir();
} // namespace camus::paths
