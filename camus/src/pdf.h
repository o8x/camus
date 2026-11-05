#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace pdf
{
	// 读取 PDF 的命名目标，返回 anchor -> 1 起始页码。
	// 解析失败时返回空表，调用方应回退到不带页码的目录。
	std::map<std::string, int> read_anchor_pages(const std::filesystem::path &path);
} // namespace pdf
