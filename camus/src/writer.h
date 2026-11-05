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

	struct article_stats {
		uint16_t articles;
		uint32_t words;
		uint32_t read_mins;
		time_t min_time;
		time_t max_time;

		std::set<std::string> tags;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(article_stats, articles, words, read_mins, min_time, max_time, tags);
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
		// 待写入文件缓存
		std::map<std::filesystem::path, std::string> pending_writes_;

		void add_write(const std::filesystem::path &path, std::string_view content);
		void flush_writes();
		void run_only_live(const std::function<void()> &func) const;

		// ── emit_article 子阶段 ────────────────────────
		void resolve_directory_routes();
		void parse_article_frontmatter();
		void prepare_output_dir() const;
		std::vector<catalog::catalog_node *> collect_articles();
		void fill_article_nav(std::vector<catalog::catalog_node *> &articles) const;
		std::vector<nlohmann::json> build_toc_items() const;
		void accumulate_article_stats(
			const catalog::catalog_node &node, size_t word_count, double read_mins,
			std::unordered_map<std::string, article_stats> &stats
		);
		void render_single_article(
			catalog::catalog_node &node, const std::vector<nlohmann::json> &toc_items,
			std::unordered_map<std::string, article_stats> &stats, const std::filesystem::path &out_dir
		);
		void render_stats_page(
			const std::unordered_map<std::string, article_stats> &stats, const std::vector<nlohmann::json> &toc_items
		);
		void finalize_directory_properties();
		void emit_article();
		void emit_friends();
		void emit_toc();
		void emit_sitemap();
		void emit_assets();
		void emit_pdf();

	  public:
		explicit writer(const cmdline &cmd);

		void inspect();
		void watch();
		int build();
	};
} // namespace camus
