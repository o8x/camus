#pragma once

namespace camus
{
	// 请求 GitHub 最新 release；有新版本时打印下载链接。成功返回 0。
	int check_update();

	// 请求 GitHub 最新 release；有新版本时下载并执行安装命令。成功返回 0。
	int run_update();
} // namespace camus
