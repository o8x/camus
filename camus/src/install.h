#pragma once

#include <filesystem>
#include <string>

namespace camus
{
	// 从平台数据目录安装 camus.yaml、posts 与 .gitignore 到工作目录；
	// 已存在的同名路径不会被覆盖。返回 0 表示成功，非 0 表示失败
	int install_templates(const std::filesystem::path &workdir);

	// full 为假时把最小资源复制到 <workdir>/assets（css/style.css、js/onload.js、
	// 空 fonts/）；full 为真时把完整资源复制到 <workdir>/.camus。force 为真时覆盖已有文件。
	// 返回 0 表示成功（或已安装且未指定 force），非 0 表示失败
	int copy_resources(const std::filesystem::path &workdir, bool force, bool full = false);

	// 只复制主题到 <workdir>/theme：name 为空时复制整个 theme 目录，
	// 非空时只复制 data_dir/theme/<name>；同时复制最小 assets 到 <workdir>/assets。
	// force 为真时覆盖已有文件，否则跳过。返回 0 表示成功，非 0 表示失败
	int copy_theme(const std::filesystem::path &workdir, const std::string &name, bool force);
} // namespace camus
