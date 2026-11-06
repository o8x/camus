#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace pdf
{
	struct outline_entry {
		std::string title;
		std::string anchor;
		int depth = 0;
		bool is_dir = false;
	};

	// 读取 PDF 的命名目标，返回 anchor -> 1 起始页码。
	// 解析失败时返回空表，调用方应回退到不带页码的目录。
	std::map<std::string, int> read_anchor_pages(const std::filesystem::path &path);

	// 返回 PDF 的总页数，解析失败时返回 0。
	int page_count(const std::filesystem::path &path);

	// 向 PDF 注入可交互的书签大纲（/Outlines）。成功返回 true。
	// 仅支持经典 xref 表（Chrome/Skia 默认输出），遇到不支持的结构时返回 false 且不修改文件。
	bool write_outline(const std::filesystem::path &path, const std::vector<outline_entry> &entries);
} // namespace pdf
