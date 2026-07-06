#pragma once

#include <filesystem>
#include <functional>

namespace filesystem
{
	// 写入文件
	int write_file(const std::string_view &path, const std::string_view &content);
	// 读取文件
	std::string read_file(const std::string &name, bool trim = false);
	// 清理路径
	std::string clean_path(const std::string &path, const std::string &prefix = "");
	// 清理相对路径
	std::filesystem::path
	clean_path_prefix(const std::filesystem::path &path, const std::string &prefix, const bool absolute = false);

	// 切换工作目录运行函数，再切换回来
	std::filesystem::path with_current_dir(
		const std::filesystem::path &path, const std::function<void(const std::filesystem::path &path)> &fn
	);
} // namespace filesystem
