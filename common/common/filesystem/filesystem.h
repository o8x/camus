#pragma once

#include <filesystem>

namespace filesystem
{
	// 写入文件
	int write_file(const std::string_view &path, const std::string_view &content);
} // namespace filesystem
