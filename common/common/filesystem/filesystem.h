#pragma once

#include <filesystem>

#include "common/error/error.h"

namespace filesystem
{
	// 写入文件
	int write_file(const std::filesystem::path &path, const std::string_view &content);
	// 读取文件
	std::string read_file(const std::string &name, bool trim = false);
	// 清理路径
	std::string clean_path(const std::string &path, const std::string &prefix = "");
	// 清理相对路径
	std::filesystem::path
	clean_path_prefix(const std::filesystem::path &path, const std::string &prefix, const bool absolute = false);
	// 检查目录是否为空
	bool path_empty(const std::filesystem::path &path);
	// 检查文件夹中的文件数量
	int scan_path_files(const std::filesystem::path &path, const uint32_t max_scan = 200);
	// 获取自己的真实路径
	std::filesystem::path get_self_path(const std::string &arg0);
	// 获取路径的绝对路径
	std::string path_abs(const std::filesystem::path &path);
	// 清空文件夹
	void empty_path(const std::filesystem::path &path, const bool safe_check = false, const bool auto_create = false);
	// 判断两个路径是否指向相同的文件
	bool path_equal(const std::filesystem::path &p1, const std::filesystem::path &p2, const bool clean_check = false);
} // namespace filesystem
