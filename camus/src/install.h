#pragma once

#include <filesystem>

namespace camus
{
	// 把平台数据目录中的资源安装到 <workdir>/.camus；force 为真时覆盖已有目录
	// 返回 0 表示成功（或已安装且未指定 force），非 0 表示失败
	int install_resources(const std::filesystem::path &workdir, bool force);
} // namespace camus
