#pragma once

#include <filesystem>

namespace camus
{
	// 从平台数据目录安装 camus.yaml、posts 与 .gitignore 到工作目录；
	// 已存在的同名路径不会被覆盖。返回 0 表示成功，非 0 表示失败
	int install_templates(const std::filesystem::path &workdir);

	// 把平台数据目录中的完整资源复制到 <workdir>/.camus；force 为真时覆盖已有目录
	// 返回 0 表示成功（或已安装且未指定 force），非 0 表示失败
	int copy_resources(const std::filesystem::path &workdir, bool force);
} // namespace camus
