#pragma once

#include "catalog.h"
#include "httplib.h"
#include "inja/inja.hpp"
#include "yaml_config.h"

namespace camus
{
	struct cmdline {
		// 初始工作目录
		std::filesystem::path workdir;
		// 空转
		bool dryrun;
	};

	class writer
	{
		// 停止服务
		std::atomic<bool> quit_ = false;
		// 服务器
		httplib::Server watch_server_;
		// 主配置文件
		config::yaml_config conf_;
		// 相对于源文件目录的目录树
		catalog::catalog_node catalog_{};
		// CMD 参数
		cmdline cmd_;
		// 模板引擎
		inja::Environment inja_;

		void run_only_live(const std::function<void()> &func) const;

		void emit_article();
		void emit_toc();
		void emit_sitemap();
		void emit_assets() const;

	  public:
		explicit writer(const cmdline &cmd);

		void inspect();
		void watch();
		int build();
	};
} // namespace camus
