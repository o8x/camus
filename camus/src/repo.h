#pragma once

#include <filesystem>
#include <string>

namespace camus
{
	// 在 workdir 执行 git init；remote 非空时设置 origin（已存在则更新）。
	// 返回 0 表示成功，非 0 表示失败。
	int init_repository(const std::filesystem::path &workdir, const std::string &remote);
} // namespace camus
