#pragma once

namespace camus
{
	// 请求 GitHub 最新 release；有新版本时打印 curl 下载命令。成功返回 0。
	int check_update();
} // namespace camus
