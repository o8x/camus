#pragma once

#include <filesystem>
#include <string>

namespace camus
{
	// 按照 target 打开：project（默认）在 VS Code 中打开工作目录，runtime 用文件管理器打开数据目录；
	// 打开失败返回非 0，成功或未知 target 返回 0（未知 target 按 project 处理）
	int open_target(const std::string &target, const std::filesystem::path &workdir);
} // namespace camus
