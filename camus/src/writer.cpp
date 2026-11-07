#include "writer.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <build/build.h>
#include <unistd.h>

#include "common/error/error.h"
#include "common/filesystem/filesystem.h"
#include "common/functions/functions.h"
#include "common/logging/logging.h"
#include "common/net/net.h"
#include "common/paths/paths.h"
#include "common/render/render.h"
#include "common/str/str.h"
#include "inja/inja.hpp"
#include "pdf.h"
#include "update.h"
#include "yaml_config.h"

namespace
{
	bool command_exists(const std::string &name)
	{
		if (name.empty()) {
			return false;
		}

		if (name.contains('/')) {
			return access(name.c_str(), X_OK) == 0;
		}

		const char *env = std::getenv("PATH");
		if (env == nullptr) {
			return false;
		}

		for (const auto &dir : strings::split(env, ':')) {
			if (dir.empty()) {
				continue;
			}

			if (const std::filesystem::path candidate = std::filesystem::path(dir) / name;
				access(candidate.c_str(), X_OK) == 0) {
				return true;
			}
		}

		return false;
	}

	std::string find_command(const std::vector<std::string> &names)
	{
		for (const auto &name : names) {
			if (command_exists(name)) {
				return name;
			}
		}

		return "";
	}

	std::string resolve_pdf_engine(const std::string &engine)
	{
		const std::string name = strings::to_lower(strings::trim_space(engine));

		if (name.contains('/')) {
			return command_exists(name) ? name : "";
		}

		if (name == "chrome" || name == "chromium" || name == "google-chrome") {
#ifdef __APPLE__
			if (const std::string path = find_command(
					{"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
					 "/Applications/Chromium.app/Contents/MacOS/Chromium"}
				);
				!path.empty()) {
				return path;
			}
#endif
			return find_command({"google-chrome", "google-chrome-stable", "chromium", "chromium-browser"});
		}

		if (name == "edge" || name == "msedge" || name == "microsoft-edge") {
#ifdef __APPLE__
			if (const std::string path =
					find_command({"/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"});
				!path.empty()) {
				return path;
			}
#endif
			return find_command({"microsoft-edge", "microsoft-edge-stable", "microsoft-edge-beta"});
		}

		return "";
	}

	std::string path_to_file_url(const std::filesystem::path &path)
	{
		std::string url = "file://";
		const std::string value = path.string();
		for (const auto &part : strings::split(value, '/')) {
			if (!part.empty()) {
				url += "/" + strings::url_encode(part);
			}
		}

		return url.empty() ? "file:///" : url;
	}

	std::string shell_quote(const std::string &value)
	{
		return "'" + strings::replace(value, "'", "'\\''") + "'";
	}

	bool visible_in_toc(const camus::catalog::catalog_node &node)
	{
		return node.property.visibility != camus::catalog::hidden &&
			   node.property.visibility != camus::catalog::hidden_in_toc;
	}

	std::string html_escape(const std::string &value)
	{
		std::string out;
		out.reserve(value.size());
		for (const char c : value) {
			switch (c) {
			case '&':
				out += "&amp;";
				break;
			case '<':
				out += "&lt;";
				break;
			case '>':
				out += "&gt;";
				break;
			case '"':
				out += "&quot;";
				break;
			case '\'':
				out += "&#39;";
				break;
			default:
				out += c;
			}
		}

		return out;
	}

	std::filesystem::path find_pdf_theme_path(const std::string &name)
	{
		return camus::paths::find_local_or_resource(std::filesystem::path(CAMUS_PDF_THEME_DIR) / name);
	}

	std::string read_pdf_theme_file(const std::string &name, const bool required = true)
	{
		const std::filesystem::path relative = std::filesystem::path(CAMUS_PDF_THEME_DIR) / name;
		const std::filesystem::path path = find_pdf_theme_path(name);
		if (path.empty()) {
			if (required) {
				logging::fatal("read pdf theme content failed name={}", relative.string());
			}

			logging::debug("pdf theme content not found, skip name={}", relative.string());
			return "";
		}

		logging::debug("load pdf theme content name={}", relative.string());
		return filesystem::read_file(path);
	}

	// PDF 复用站点 assets/fonts：站点构建后已提升到 dest_dir/fonts，
	// 这里复制为 pdf_dir/assets/fonts，供 theme/pdf/fonts.css 以 assets/fonts/... 引用。
	void copy_pdf_asset_fonts(const std::filesystem::path &dest_dir, const std::filesystem::path &pdf_dir)
	{
		const std::filesystem::path source = dest_dir / CAMUS_PDF_FONTS_DIR;
		std::error_code ec;
		if (!std::filesystem::is_directory(source, ec) || ec) {
			logging::debug("pdf asset fonts not found, skip copy path={}", source.string());
			return;
		}

		const std::filesystem::path target = pdf_dir / "assets" / CAMUS_PDF_FONTS_DIR;
		std::filesystem::create_directories(target.parent_path());
		std::filesystem::copy(
			source,
			target,
			std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing
		);

		logging::debug("copy pdf asset fonts from={} to={}", source.string(), target.string());
	}

	// 把站点 assets/css/style.css 复制为 PDF 目录下的 assets.css，避免与 PDF 主题样式同名冲突。
	bool copy_pdf_asset_style(const std::filesystem::path &dest_dir, const std::filesystem::path &pdf_dir)
	{
		const std::filesystem::path source = dest_dir / "css" / "style.css";
		std::error_code ec;
		if (!std::filesystem::is_regular_file(source, ec) || ec) {
			logging::debug("pdf asset style not found, skip copy path={}", source.string());
			return false;
		}

		const std::filesystem::path target = pdf_dir / "assets.css";
		std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, ec);
		if (ec) {
			logging::warn(
				"pdf asset style copy failed source={} error={}",
				source.string(),
				ec.message()
			);
			return false;
		}

		logging::debug("copy pdf asset style from={} to={}", source.string(), target.string());
		return true;
	}

	// 解析 PDF 封面图：绝对路径直接使用；相对路径依次按 camus.yaml 所在目录（workdir）、
	// 源目录与站点 assets 目录尝试，命中第一个普通文件。返回空表示未找到。
	std::filesystem::path resolve_cover_image(
		const std::filesystem::path &workdir,
		const std::filesystem::path &source_dir,
		const std::filesystem::path &assets_dir,
		const std::string &cover_image,
		std::vector<std::filesystem::path> *attempted = nullptr
	)
	{
		const std::filesystem::path raw = strings::trim_space(cover_image);
		if (raw.empty()) {
			return {};
		}

		std::vector<std::filesystem::path> candidates;
		if (raw.is_absolute()) {
			candidates.push_back(raw);
		} else {
			candidates.push_back(workdir / raw);

			if (!source_dir.empty()) {
				candidates.push_back(source_dir / raw);
			}

			if (!assets_dir.empty()) {
				candidates.push_back(assets_dir / raw);
			}
		}

		for (const auto &candidate : candidates) {
			std::error_code ec;
			if (std::filesystem::is_regular_file(candidate, ec) && !ec) {
				return candidate;
			}

			if (attempted != nullptr) {
				attempted->push_back(candidate);
			}
		}

		return {};
	}

	std::string normalize_pdf_font_size(const std::string &raw)
	{
		const std::string default_size = camus::config::pdf_page_font_size_default;
		const std::string value = strings::trim_space(raw);
		if (value.empty()) {
			return default_size;
		}

		constexpr const char *unit = "pt";
		const std::size_t unit_size = 2;
		bool valid = value.size() > unit_size && value.ends_with(unit);
		if (valid) {
			const std::string number = value.substr(0, value.size() - unit_size);
			std::size_t consumed = 0;
			try {
				const double parsed = std::stod(number, &consumed);
				valid = consumed == number.size() && parsed > 0 && std::isfinite(parsed);
			} catch (...) {
				valid = false;
			}
		}

		if (!valid) {
			logging::warn(
				"invalid pdf.page.font_size '{}', only pt is supported, fallback to {}",
				value,
				default_size
			);
			return default_size;
		}

		return value;
	}

	// 严格的 UTF-8 解码：非法序列返回 U+FFFD 并前进一个字节。
	std::uint32_t decode_utf8(const std::string_view text, std::size_t &index)
	{
		const auto lead = static_cast<unsigned char>(text[index]);
		if (lead < 0x80) {
			++index;
			return lead;
		}

		int length = 0;
		std::uint32_t code = 0;
		if ((lead & 0xE0) == 0xC0) {
			length = 2;
			code = lead & 0x1F;
		} else if ((lead & 0xF0) == 0xE0) {
			length = 3;
			code = lead & 0x0F;
		} else if ((lead & 0xF8) == 0xF0) {
			length = 4;
			code = lead & 0x07;
		} else {
			++index;
			return 0xFFFD;
		}

		if (index + static_cast<std::size_t>(length) > text.size()) {
			index = text.size();
			return 0xFFFD;
		}

		for (int i = 1; i < length; ++i) {
			const auto next = static_cast<unsigned char>(text[index + static_cast<std::size_t>(i)]);
			if ((next & 0xC0) != 0x80) {
				++index;
				return 0xFFFD;
			}

			code = (code << 6) | (next & 0x3F);
		}

		index += static_cast<std::size_t>(length);
		return code;
	}

	bool is_cjk_ideograph(const std::uint32_t code)
	{
		return (code >= 0x3400 && code <= 0x4DBF) || (code >= 0x4E00 && code <= 0x9FFF) ||
			   (code >= 0xF900 && code <= 0xFAFF) || (code >= 0x20000 && code <= 0x2FA1F) ||
			   (code >= 0x30000 && code <= 0x3134F);
	}

	bool is_cjk_punctuation(const std::uint32_t code)
	{
		// CJK 符号与标点（排除 U+3000 全角空格）、全角标点、常见中文引号/破折号/省略号/间隔号。
		return (code >= 0x3001 && code <= 0x303F) || (code >= 0xFF01 && code <= 0xFF0F) ||
			   (code >= 0xFF1A && code <= 0xFF20) || (code >= 0xFF3B && code <= 0xFF40) ||
			   (code >= 0xFF5B && code <= 0xFF65) || (code >= 0x2018 && code <= 0x201F) || code == 0x00B7 ||
			   code == 0x2014 || code == 0x2026;
	}

	// 返回中文占比（%）：汉字与中文标点计入中文，ASCII 字母计入非中文；空白、数字、其它符号忽略。
	double chinese_ratio(const std::string_view text)
	{
		long long cn = 0;
		long long en = 0;
		for (std::size_t i = 0; i < text.size();) {
			const std::uint32_t code = decode_utf8(text, i);
			if (is_cjk_ideograph(code) || is_cjk_punctuation(code)) {
				++cn;
			} else if (code < 0x80 && std::isalpha(static_cast<unsigned char>(code)) != 0) {
				++en;
			}
		}

		return cn + en > 0 ? 100.0 * static_cast<double>(cn) / static_cast<double>(cn + en) : 0.0;
	}

	std::string pdf_page_style(const camus::config::pdf_page_conf &page, const std::string &font_size)
	{
		std::string size = strings::trim_space(page.size);
		std::string margin = strings::trim_space(page.margin);
		if (size.empty()) {
			size = camus::config::pdf_page_size_default;
		}

		if (margin.empty()) {
			margin = camus::config::pdf_page_margin_default;
		}

		// 封面使用独立的命名页（margin: 0），这里同步使用配置的页面尺寸，
		// 否则会在全局尺寸之外继续沿用主题里硬编码的 A4。
		return "\n:root { --pdf-font-size: " + font_size + "; }\n@page { size: " + size + "; margin: " + margin +
			   "; }\n@page cover { size: " + size + "; margin: 0; }\n";
	}
} // namespace

namespace camus
{
	void writer::run_only_live(const std::function<void()> &func) const
	{
		if (cmd_.dryrun) {
			return;
		}

		func();
	}

	void writer::add_write(const std::filesystem::path &path, std::string_view content)
	{
		if (cmd_.dryrun) {
			return;
		}

		// 统一为绝对路径，避免相对/绝对路径指向同一文件时重复检测失效
		const std::filesystem::path target =
			path.is_absolute() ? path.lexically_normal() : std::filesystem::absolute(path).lexically_normal();

		if (pending_writes_.contains(target)) {
			logging::fatal("output file already queued dest={}", target.string());
		}

		pending_writes_[target] = content;
	}

	void writer::flush_writes()
	{
		for (const auto &[path, content] : pending_writes_) {
			std::filesystem::create_directories(path.parent_path());
			filesystem::write_file(path, content);
		}

		pending_writes_.clear();
	}

	void writer::emit_article()
	{
		resolve_directory_routes();
		parse_article_frontmatter();
		prepare_output_dir();
		// 必须在生成 TOC 与统计数据前完成：计算目录属性、清理隐藏/无效节点
		finalize_directory_properties();

		auto articles = collect_articles();
		fill_article_nav(articles);

		const std::filesystem::path out_dir = std::filesystem::relative(conf_.camus().dest_dir(), cmd_.workdir);
		auto toc_items = build_toc_items();

		article_stats all_stats{};
		all_stats.min_time = std::numeric_limits<time_t>::max();
		std::unordered_map<std::string, article_stats> stats;
		stats["all"] = all_stats;

		for (auto *article : articles) {
			render_single_article(*article, toc_items, stats, out_dir);
		}

		render_stats_page(stats, toc_items);
	}

	void writer::resolve_directory_routes()
	{
		catalog::traverse_catalog_tree(catalog_, [&](catalog::catalog_node &node, const int depth) {
			if (!node.is_directory()) {
				return;
			}

			node.path_prefix = conf_.camus().public_url.path_prefix;

			if (depth == 0) {
				node.property.external_path = node.path;
				node.property.display_name = node.path.filename();
			}

			if (const auto route = conf_.match_route(node.path)) {
				if (!route->title.empty()) {
					node.property.display_name = route->title;
				}

				if (!route->subtitle.empty()) {
					node.property.subtitle = route->subtitle;
				}

				if (!route->description.empty()) {
					node.property.description = route->description;
				}
			}

			// transfers 在全部路由中匹配，从而支持 source 指向路由下的子目录
			if (const std::filesystem::path tr_path = conf_.match_transfer(node.path); !tr_path.empty()) {
				node.property.external_path = tr_path;
			}

			// 子目录继承父目录的输出前缀，并追加自身目录名；文件直接继承父目录输出前缀。
			// 这样父目录的搬运（transfer）才能完整作用到所有后代。
			for (auto &c : node.children) {
				c.property.external_path =
					c.is_directory() ? node.property.external_path / c.path.filename() : node.property.external_path;
			}
		});
	}

	void writer::parse_article_frontmatter()
	{
		catalog::traverse_catalog_tree(catalog_, [&](catalog::catalog_node &node, int) {
			if (node.path.extension() != ".md") {
				return;
			}

			node.path_prefix = conf_.camus().public_url.path_prefix;

			std::map<std::string, std::string> params;
			if (!node.contents.empty() && strings::trim_space(node.contents.front()) == "---") {
				bool closed = false;
				for (size_t i = 1; i < node.contents.size(); ++i) {
					const std::string line = strings::trim_space(node.contents[i]);
					if (line == "---") {
						node.contents.assign(
							node.contents.begin() + static_cast<std::ptrdiff_t>(i),
							node.contents.end()
						);
						closed = true;
						break;
					}

					const auto [left, right] = strings::split_pair(line, ":", true);
					if (left.empty() && right.empty()) {
						logging::fatal("failed to parse params file={} line={}({})", node.path.string(), i, line);
					}

					params.emplace(left, right);
				}

				if (!closed) {
					logging::fatal("failed to find front matter end file={}", node.path.string());
				}

				// 丢弃作为分隔符的结尾 --- 行本身，避免在正文中多出一条水平线
				if (!node.contents.empty() && strings::trim_space(node.contents.front()) == "---") {
					node.contents.erase(node.contents.begin());
				}
			}

			if (params["visibility"] == "hidden") {
				node.property.visibility = catalog::hidden;
				return;
			}

			bool hidden_flag = false;
			for (auto it = node.contents.begin(); it != node.contents.end();) {
				if (*it == "--hidden-section-start") {
					hidden_flag = true;
					it = node.contents.erase(it);
					continue;
				}

				if (*it == "--hidden-section-end") {
					hidden_flag = false;
					it = node.contents.erase(it);
					continue;
				}

				if (hidden_flag) {
					it = node.contents.erase(it);
				} else {
					++it;
				}
			}

			if (params.contains("tags")) {
				try {
					nlohmann::json::parse(params["tags"]).get_to(node.property.tags);
				} catch (const std::exception &e) {
					logging::fatal("parse article tags failed name={} error={}", node.path.string(), e.what());
				}
			}

			node.property.write_time = functions::datetime_to_unix(params["create-time"]);
			if (node.property.write_time <= 0) {
				node.property.write_time = 0;
				struct stat fileStat{};
				if (stat((conf_.camus().source_dir / node.path).c_str(), &fileStat) == 0) {
					node.property.write_time = fileStat.st_mtime;
				} else {
					logging::warn("failed to stat {}", node.path.string());
				}
			}

			node.property.visibility = params["visibility"] == "hidden_in_toc" ? catalog::hidden_in_toc : catalog::open;

			if (conf_.camus().output.filename_case == "uuid") {
				node.property.display_name = strings::uuid_v4();
			} else {
				node.property.display_name = params["title"];
			}

			if (node.property.display_name.empty()) {
				node.property.display_name = node.path.stem();
			}

			node.property.display_name = strings::replace(
				conf_.render_var(node.property.display_name),
				std::map<std::string, std::string>{
					{"/", " "},
					{"\\", " "},
					{"'", " "},
					{"\"", " "},
				}
			);

			if (const std::string alias = params["short-path"]; !alias.empty()) {
				const uint64_t hash = strings::make_hash(node.path);

				if (alias == "auto") {
					node.property.short_path = std::format("{:x}", hash % (UINT16_MAX * 2));
				} else {
					node.property.short_path = alias;
				}
			}
		});
	}

	void writer::prepare_output_dir() const
	{
		run_only_live([&]() {
			filesystem::empty_path(conf_.camus().dest_dir(), true);
			filesystem::empty_path(conf_.camus().pdf_dir(), true);
		});
	}

	std::vector<catalog::catalog_node *> writer::collect_articles()
	{
		std::vector<catalog::catalog_node *> articles;
		catalog::traverse_catalog_tree(catalog_, [&](catalog::catalog_node &node, int) {
			if (node.path.extension() == ".md" && node.property.visibility != catalog::hidden) {
				articles.push_back(&node);
			}
		});

		std::ranges::sort(articles, [](const catalog::catalog_node *a, const catalog::catalog_node *b) -> bool {
			if (a->property.write_time != b->property.write_time) {
				return a->property.write_time > b->property.write_time;
			}

			return a->path.string() < b->path.string();
		});

		return articles;
	}

	void writer::fill_article_nav(std::vector<catalog::catalog_node *> &articles) const
	{
		for (int i = 0; i < articles.size(); ++i) {
			auto &node = *articles[i];

			if (i + 1 < articles.size()) {
				node.render_data_.nav.next_path =
					filesystem::clean_path(articles[i + 1]->link_url(), conf_.camus().public_url.path_prefix);
				node.render_data_.nav.next_title = articles[i + 1]->property.display_name;
			}

			if (i > 0) {
				node.render_data_.nav.prev_path =
					filesystem::clean_path(articles[i - 1]->link_url(), conf_.camus().public_url.path_prefix);
				node.render_data_.nav.prev_title = articles[i - 1]->property.display_name;
			}
		}
	}

	std::vector<nlohmann::json> writer::build_toc_items() const
	{
		std::vector<nlohmann::json> items;
		for (const auto &child : catalog_.children) {
			if (visible_in_toc(child)) {
				items.push_back(nlohmann::json(child));
			}
		}

		std::ranges::sort(items, [](const nlohmann::json &a, const nlohmann::json &b) -> bool {
			if (const bool a_dir = a.value("is_dir", false); a_dir != b.value("is_dir", false)) {
				return a_dir;
			}

			if (const auto at = a.value("write_time", 0); at != b.value("write_time", 0)) {
				return at > b.value("write_time", 0);
			}

			return a.value("path", "") < b.value("path", "");
		});

		return items;
	}

	void writer::accumulate_article_stats(
		const catalog::catalog_node &node, const size_t word_count, const double read_mins,
		std::unordered_map<std::string, article_stats> &stats
	)
	{
		auto &all = stats["all"];
		all.max_time = std::max(all.max_time, node.property.write_time);
		all.min_time = std::min(all.min_time, node.property.write_time);
		all.articles++;
		all.words += word_count;
		all.read_mins += read_mins;
		all.tags.insert(node.property.tags.begin(), node.property.tags.end());

		if (uint16_t year = 1970 + node.property.write_time / (365 * 86400); year > 1996 && year < 2200) {
			const std::string year_name = std::to_string(year);
			auto &yr = stats[year_name];
			if (yr.min_time == 0) {
				yr.min_time = std::numeric_limits<time_t>::max();
			}

			if (node.property.write_time > 0) {
				yr.max_time = std::max(yr.max_time, node.property.write_time);
				yr.min_time = std::min(yr.min_time, node.property.write_time);
			}

			yr.articles++;
			yr.words += word_count;
			yr.read_mins += read_mins;
			yr.tags.insert(node.property.tags.begin(), node.property.tags.end());
		}
	}

	void writer::render_single_article(
		catalog::catalog_node &node, const std::vector<nlohmann::json> &toc_items,
		std::unordered_map<std::string, article_stats> &stats, const std::filesystem::path &out_dir
	)
	{
		if (node.link_url().empty()) {
			logging::fatal(
				"make article failed file={} name={} dest={}{}",
				node.path.string(),
				node.property.display_name,
				out_dir.string(),
				node.link_url().string()
			);
		}

		std::string markdown = strings::string_join(node.contents, "\n");

		const size_t word_count = strings::estimate_effective_length(markdown);
		const double read_mins = strings::estimate_reading_minutes(markdown);

		accumulate_article_stats(node, word_count, read_mins, stats);

		const std::string html = render::inja_render(
			inja_,
			render::render_markdown(
				markdown.data(),
				conf_.camus().render.markdown_engine,
				conf_.camus().render.markdown_options
			),
			conf_.json()
		);

		node.render_data_.stats.word_count = word_count;
		node.render_data_.stats.read_min = read_mins;
		node.render_data_.page.contents = html;

		nlohmann::json j = conf_.json();
		j.update(nlohmann::json(node));
		j["toc"] = toc_items;

		j["page"]["title"] = node.property.display_name;
		j["page"]["write_time"] = node.property.write_time;
		j["page"]["description"] = "";
		j["page"]["contents"] = html;
		j["stats"]["word_count"] = word_count;
		j["stats"]["read_min"] = read_mins;
		j["nav"] = node.render_data_.nav;

		logging::debug(
			"make article words={} name={} dest={}{}",
			word_count,
			node.path.string(),
			out_dir.string(),
			node.link_url().string()
		);

		const std::string contents = strings::replace(
			render::inja_render(inja_, conf_.camus().get_theme(config::CAMUS_THEME_TYPE_PAGE), j),
			std::map<std::string, std::string>{
				{" 00:00:00", ""},
				{"<img ", R"(<img width="100%")"},
			},
			true
		);

		for (const auto &f : std::set{node.link_url(), node.real_url()}) {
			add_write(out_dir / filesystem::clean_path(f, "./"), contents);
		}
	}

	void writer::render_stats_page(
		const std::unordered_map<std::string, article_stats> &stats, const std::vector<nlohmann::json> &toc_items
	)
	{
		nlohmann::json j = conf_.json();
		j["toc"] = toc_items;
		j["stats"] = stats;

		const std::string contents =
			render::inja_render(inja_, conf_.camus().get_theme(config::CAMUS_THEME_TYPE_STATS), j);
		add_write(conf_.camus().dest_dir() / "stats.html", contents);
	}

	void writer::finalize_directory_properties()
	{
		// 后序遍历：先完成子目录，父目录才能真正取到所有后代的 write_time
		std::function<void(catalog::catalog_node &)> finalize = [&](catalog::catalog_node &node) {
			for (auto &child : node.children) {
				finalize(child);
			}

			if (!node.is_directory()) {
				return;
			}

			for (const auto &c : node.children) {
				node.property.write_time = std::max(node.property.write_time, c.property.write_time);
			}

			node.property.subtitle =
				node.property.subtitle.empty() ? conf_.camus().site.subtitle : node.property.subtitle;
			node.property.description =
				node.property.description.empty() ? conf_.camus().site.description : node.property.description;
			node.property.display_name =
				node.property.display_name.empty() ? conf_.camus().site.title : node.property.display_name;
		};

		finalize(catalog_);

		// 移除隐藏/非 markdown 文件，以及因此变空的目录
		catalog_.remove_children_if([](const catalog::catalog_node &a) -> bool {
			if (a.is_directory()) {
				return a.children.empty();
			}

			return a.property.invalid();
		});
	}

	void writer::emit_friends()
	{
		std::vector<nlohmann::json> toc_items;
		for (const auto &child : catalog_.children) {
			if (!visible_in_toc(child)) {
				continue;
			}
			toc_items.push_back(nlohmann::json(child));
		}

		std::ranges::sort(toc_items, [](const nlohmann::json &a, const nlohmann::json &b) -> bool {
			if (const bool a_dir = a.value("is_dir", false); a_dir != b.value("is_dir", false)) {
				return a_dir;
			}

			if (const auto at = a.value("write_time", 0); at != b.value("write_time", 0)) {
				return at > b.value("write_time", 0);
			}

			return a.value("path", "") < b.value("path", "");
		});

		nlohmann::json json = conf_.json();
		json["toc"] = toc_items;
		json["friends"] = conf_.camus().friends;

		add_write(
			filesystem::clean_path("friends.html", conf_.camus().dest_dir()),
			render::inja_render(inja_, conf_.camus().get_theme(config::CAMUS_THEME_TYPE_FRIENDS), json)
		);
	}

	void writer::emit_toc()
	{
		if (cmd_.dryrun) {
			return;
		}

		struct dir_ctx {
			const catalog::catalog_node *parent = nullptr;
			std::vector<const catalog::catalog_node *> children;
		};

		std::map<const catalog::catalog_node *, dir_ctx> dirs;
		std::vector<const catalog::catalog_node *> order;

		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &node, int) {
			if (!node.is_directory()) {
				return;
			}

			// 按先序遍历顺序记录目录，保证 meta.json 等输出稳定
			order.push_back(&node);

			auto &[parent, children] = dirs[&node];
			for (const auto &child : node.children) {
				if (!visible_in_toc(child)) {
					continue;
				}

				if (child.is_directory()) {
					dirs[&child].parent = &node;
				}
				children.push_back(&child);
			}
		});

		nlohmann::json toc_full;
		toc_full["config"] = conf_.json();

		for (const auto *dir_node : order) {
			const auto &ctx = dirs.at(dir_node);
			nlohmann::json toc_item;

			if (ctx.parent) {
				const std::filesystem::path parent_url = ctx.parent->link_url();
				toc_item["parent"]["url"] = filesystem::clean_path(parent_url, conf_.camus().public_url.path_prefix);
				toc_item["parent"]["label"] = ctx.parent->property.display_name;
				if (parent_url == "/") {
					toc_item["parent"]["label"] = "/";
				}
			} else {
				toc_item["parent"]["url"] = "";
				toc_item["parent"]["label"] = "";
			}

			std::vector<nlohmann::json> items;
			for (const auto *child : ctx.children) {
				items.push_back(nlohmann::json(*child));
			}

			std::ranges::sort(items, [](const nlohmann::json &a, const nlohmann::json &b) -> bool {
				if (const bool a_dir = a.value("is_dir", false); a_dir != b.value("is_dir", false)) {
					return a_dir;
				}

				if (const auto at = a.value("write_time", 0); at != b.value("write_time", 0)) {
					return at > b.value("write_time", 0);
				}

				return a.value("path", "") < b.value("path", "");
			});

			toc_item["items"] = items;
			toc_item["current"] = *dir_node;
			// current.path 供主题判断是否根目录（如 current.path != "/"），
			// 使用相对目录树的原始路径，而不是带 public_url 前缀的 URL。
			toc_item["current"]["path"] = filesystem::clean_path(dir_node->path, "");

			toc_full["catalog"].push_back(toc_item);
			toc_item.update(conf_.json());

			add_write(
				filesystem::clean_path(
					std::format("{}/index.html", dir_node->real_url().string()),
					conf_.camus().dest_dir()
				),
				render::inja_render(inja_, conf_.camus().get_theme(config::CAMUS_THEME_TYPE_HOME), toc_item)
			);
		}

		if (conf_.camus().output.meta) {
			add_write(filesystem::clean_path("meta.json", conf_.camus().dest_dir()), toc_full.dump(4));
		}
	}

	void writer::emit_sitemap()
	{
		if (!conf_.camus().output.sitemap) {
			return;
		}

		std::vector<std::string> items;
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &node, int) {
			if (node.is_directory()) {
				return;
			}

			const std::string loc = std::format(
				"<url><loc>{}</loc><lastmod>{}</lastmod></url>",
				std::format("{}{}", conf_.camus().public_url.full_prefix(), node.link_url().string()),
				functions::format_time_t(node.property.write_time, "%Y-%m-%d")
			);

			items.push_back(loc);
		});

		logging::debug(std::format("make sitemap url={}/sitemap.xml", conf_.camus().public_url.full_prefix()));

		add_write(
			conf_.camus().dest_dir() / "sitemap.xml",
			std::format(
				R"(<?xml version="1.0" encoding="UTF-8"?><urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">{}</urlset>)",
				strings::string_join(items, "\n\t")
			)
		);
	}

	void writer::emit_assets()
	{
		if (cmd_.dryrun) {
			return;
		}

		const std::filesystem::path default_dir = paths::resource_assets_dir();
		const std::filesystem::path local_dir = paths::local_data_dir() / "assets";
		const std::filesystem::path user_dir = conf_.camus().assets_dir;

		const auto non_empty_dir = [](const std::filesystem::path &path) {
			return !path.empty() && std::filesystem::is_directory(path) && !std::filesystem::is_empty(path);
		};

		const bool has_default = non_empty_dir(default_dir);
		const bool has_local = non_empty_dir(local_dir);
		const bool has_user = non_empty_dir(user_dir);

		if (!has_default && !has_local && !has_user) {
			logging::debug("skip assets: no default, local or user assets found");
			return;
		}

		std::filesystem::create_directories(conf_.camus().dest_dir());

		// 资源文件夹按默认 -> .camus -> 本地 ./assets 的顺序叠加，后层覆盖前层
		const std::filesystem::path dest_dir = conf_.camus().dest_dir() / "assets";
		if (has_default) {
			std::filesystem::copy(
				default_dir,
				dest_dir,
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing
			);
		}

		if (has_local) {
			std::filesystem::copy(
				local_dir,
				dest_dir,
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing
			);
		}

		if (has_user) {
			std::filesystem::copy(
				user_dir,
				dest_dir,
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing
			);
		}

		// 替换资源文件中的模板
		size_t count = 0;
		const nlohmann::json json = conf_.json();
		for (const auto &entry : std::filesystem::recursive_directory_iterator(dest_dir)) {
			if (!entry.is_regular_file()) {
				continue;
			}

			++count;
			if (const std::filesystem::path ext = entry.path().extension();
				ext == ".css" || ext == ".js" || ext == ".txt" || ext == ".json") {
				filesystem::write_file(
					entry.path(),
					render::inja_render(inja_, filesystem::read_file(entry.path()), json)
				);
			}
		}

		// 默认会将资源文件都提升到输出文件夹中，再删除原文件
		std::filesystem::copy(
			dest_dir,
			conf_.camus().dest_dir(),
			std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing
		);

		filesystem::empty_path(dest_dir, true);

		logging::debug(
			std::format(
				"assets: promote copy default={} local={} user={} files={}",
				has_default ? std::filesystem::relative(default_dir, cmd_.workdir).string() : "-",
				has_local ? std::filesystem::relative(local_dir, cmd_.workdir).string() : "-",
				has_user ? std::filesystem::relative(user_dir, cmd_.workdir).string() : "-",
				count
			)
		);
	}

	void writer::emit_pdf()
	{
		const std::string pdf_engine = strings::trim_space(conf_.camus().pdf.engine);
		if (pdf_engine.empty() || strings::to_lower(pdf_engine) == "disable") {
			return;
		}

		if (cmd_.dryrun) {
			return;
		}

		const std::string engine = resolve_pdf_engine(pdf_engine);
		if (engine.empty()) {
			logging::warn("pdf engine not found, skip pdf generation name={}", pdf_engine);
			return;
		}

		struct pdf_article {
			const catalog::catalog_node *node;
			std::string markdown;
		};

		std::vector<pdf_article> articles;
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &node, int) {
			if (node.path.extension() != ".md" || node.property.visibility == catalog::hidden) {
				return;
			}

			std::string markdown = strings::string_join(node.contents, "\n");

			articles.push_back({&node, std::move(markdown)});
		});

		if (articles.empty()) {
			logging::debug("skip pdf generation: no articles");
			return;
		}

		const std::string &site_title = conf_.camus().site.title;
		const std::string cover_title = html_escape(site_title.empty() ? "Camus" : site_title);
		const std::string cover_subtitle = html_escape(conf_.camus().site.subtitle);
		const std::string cover_desc = html_escape(conf_.camus().site.description);
		const std::string generated_at = functions::format_time_t(std::time(nullptr), "%Y-%m-%d");
		const std::string domain = html_escape(conf_.camus().public_url.domain_name);

		const std::string template_source = read_pdf_theme_file(CAMUS_PDF_TEMPLATE_NAME);
		const std::string style_source = read_pdf_theme_file(CAMUS_PDF_STYLE_NAME);
		const std::string fonts_source = read_pdf_theme_file(CAMUS_PDF_FONTS_NAME, false);

		nlohmann::json view = conf_.json();
		view["pdf_page_offset"] = 0;
		std::string markdown = std::format("# {}\n\n{}\n", site_title, conf_.camus().site.description);

		const camus::config::pdf_page_conf page_conf = conf_.camus().pdf.page;
		const std::string page_size = strings::trim_space(page_conf.size).empty()
										  ? camus::config::pdf_page_size_default
										  : strings::trim_space(page_conf.size);
		const std::string page_margin = strings::trim_space(page_conf.margin).empty()
											? camus::config::pdf_page_margin_default
											: strings::trim_space(page_conf.margin);
		const std::string page_font_size = normalize_pdf_font_size(page_conf.font_size);

		nlohmann::json pdf;
		pdf["lang"] = site_title.empty() ? "en" : "zh-CN";
		pdf["title"] = cover_title;
		pdf["style"] = render::inja_render(inja_, style_source, view) + pdf_page_style(page_conf, page_font_size);
		pdf["page"] = {
			{"size", page_size},
			{"margin", page_margin},
			{"font_size", page_font_size},
		};
		pdf["fonts"] = fonts_source.empty() ? "" : render::inja_render(inja_, fonts_source, view);
		pdf["asset_style"] = false;
		pdf["cover"] = {
			{"eyebrow", "Camus"},
			{"title", cover_title},
			{"subtitle", cover_subtitle},
			{"description", cover_desc},
			{"meta", generated_at},
			{"image", ""},
			{"has_image", false},
		};
		pdf["title_page"] = {
			{"title", cover_title},
			{"subtitle", cover_subtitle},
			{"description", cover_desc},
		};
		pdf["meta"] = {
			{"article_count", articles.size()},
			{"generated_at", generated_at},
			{"domain", domain},
			{"has_domain", !domain.empty()},
			{"version", PROJECT_VERSION},
		};

		nlohmann::json toc_items = nlohmann::json::array();
		nlohmann::json article_items = nlohmann::json::array();

		// 目录按真实目录树多级输出：目录节点作为分组标题，文章节点作为可跳转条目。
		// 根节点（depth 0）只是容器，depth - 1 为模板中的显示层级。
		const config::pdf_sort_mode sort_mode = conf_.camus().pdf.sort;
		int index = 0;
		std::function<void(const catalog::catalog_node &, int)> append_node;
		append_node = [&](const catalog::catalog_node &node, const int depth) {
			if (depth > 0 && node.is_directory()) {
				toc_items.push_back({
					{"anchor", ""},
					{"index", 0},
					{"number", ""},
					{"title", html_escape(node.property.display_name)},
					{"outline_title", node.property.display_name},
					{"tags", ""},
					{"page", ""},
					{"page_label", ""},
					{"depth", depth - 1},
					{"is_dir", true},
				});
			}

			if (!node.is_directory()) {
				if (node.path.extension() != ".md" || node.property.visibility == catalog::hidden) {
					return;
				}

				++index;

				const std::string label = node.property.display_name;
				const std::string article_markdown = strings::string_join(node.contents, "\n");
				markdown += std::format("\n\n---\n\n## {}\n\n{}", label, article_markdown);

				std::string tags;
				for (const auto &tag : node.property.tags) {
					if (!tags.empty()) {
						tags += " · ";
					}

					tags += html_escape(tag);
				}

				const std::string anchor = std::format("pdf-article-{}", index);
				const std::string number = std::format("{:02}", index);

				toc_items.push_back({
					{"anchor", anchor},
					{"index", index},
					{"number", number},
					{"title", html_escape(label)},
					{"outline_title", label},
					{"tags", tags},
					{"page", ""},
					{"page_label", ""},
					{"depth", depth - 1},
					{"is_dir", false},
				});

				const std::string body_markdown = std::format("## {}\n\n{}", label, article_markdown);
				article_items.push_back({
					{"anchor", anchor},
					{"index", index},
					{"number", number},
					{"title", html_escape(label)},
					{"tags", tags},
					{"content",
					 render::render_markdown(
						 body_markdown.c_str(),
						 conf_.camus().render.markdown_engine,
						 conf_.camus().render.markdown_options
					 )},
				});

				return;
			}

			// PDF 顺序由 pdf.sort 决定：file_first / dir_first 分别把同层文件或目录
			// 排在前面，groups 内部按相对路径字典序升序；mixed 完全按路径字典序。
			std::vector<const catalog::catalog_node *> ordered;
			ordered.reserve(node.children.size());
			for (const auto &child : node.children) {
				ordered.push_back(&child);
			}

			std::ranges::sort(ordered, [sort_mode](const catalog::catalog_node *a, const catalog::catalog_node *b) {
				if (sort_mode != config::PDF_SORT_MIXED && a->is_directory() != b->is_directory()) {
					return sort_mode == config::PDF_SORT_DIR_FIRST ? a->is_directory() : !a->is_directory();
				}

				return a->path.string() < b->path.string();
			});

			for (const auto *child : ordered) {
				append_node(*child, depth + 1);
			}
		};

		append_node(catalog_, 0);

		// 依据合并后 Markdown 的中文占比决定 PDF 预定义名称使用中文还是英文。
		const double cn_ratio = chinese_ratio(markdown);
		// chinese_ratio() 返回百分比，0.5 即 50%。
		const bool use_chinese = cn_ratio > 50.0;
		const std::string label_cover = use_chinese ? "封面" : "Cover";
		const std::string label_contents = use_chinese ? "目录" : "Contents";
		const std::string label_site = use_chinese ? "站点" : "Site";
		const std::string label_generated = use_chinese ? "生成时间" : "Generated";
		const std::string label_compiler = use_chinese ? "编译器" : "Compiler";
		logging::debug("pdf language ratio={:.2f}% chinese={}", cn_ratio, use_chinese);

		pdf["labels"] = {
			{"cover", label_cover},
			{"contents", label_contents},
			{"site", label_site},
			{"generated", label_generated},
			{"compiler", label_compiler},
		};

		pdf["toc"] = std::move(toc_items);
		pdf["articles"] = std::move(article_items);

		const std::filesystem::path pdf_dir = conf_.camus().pdf_dir();
		const std::filesystem::path markdown_path = pdf_dir / "index.md";
		const std::filesystem::path html_path = pdf_dir / "index.html";
		std::filesystem::path pdf_path = pdf_dir / "index.pdf";
		const std::filesystem::path browser_log_path =
			std::filesystem::temp_directory_path() / std::format("camus-pdf-{}.log", getpid());

		std::filesystem::create_directories(pdf_dir);
		filesystem::write_file(markdown_path, markdown);
		copy_pdf_asset_fonts(conf_.camus().dest_dir(), pdf_dir);
		pdf["asset_style"] = copy_pdf_asset_style(conf_.camus().dest_dir(), pdf_dir);

		if (const std::string cover_image = strings::trim_space(conf_.camus().site.cover_image);
			!cover_image.empty()) {
			std::vector<std::filesystem::path> attempted;
			const std::filesystem::path cover_path = resolve_cover_image(
				cmd_.workdir,
				conf_.camus().source_dir,
				conf_.camus().assets_dir,
				cover_image,
				&attempted
			);

			if (cover_path.empty()) {
				std::string tried;
				for (const auto &path : attempted) {
					if (!tried.empty()) {
						tried += ", ";
					}

					tried += path.string();
				}

				logging::warn("pdf cover image not found path={} tried=[{}]", cover_image, tried);
			} else {
				const std::string extension = cover_path.extension().string();
				const std::string target_name = "cover" + (extension.empty() ? ".png" : extension);

				std::error_code cover_ec;
				if (std::filesystem::copy_file(
						cover_path,
						pdf_dir / target_name,
						std::filesystem::copy_options::overwrite_existing,
						cover_ec
					)) {
					pdf["cover"]["image"] = target_name;
					pdf["cover"]["has_image"] = true;
					logging::debug("pdf cover image source={}", cover_path.string());
				} else {
					logging::warn(
						"pdf cover image copy failed source={} error={}",
						cover_path.string(),
						cover_ec.message()
					);
				}
			}
		}

		view["pdf"] = std::move(pdf);

		std::vector<std::string> args;
		bool has_headless = false;
		bool has_print_to_pdf = false;
		bool has_user_data_dir = false;

		for (const auto &option : conf_.camus().pdf.build) {
			if (option.empty()) {
				continue;
			}

			if (option.starts_with("--headless")) {
				has_headless = true;
			}

			if (option.starts_with("--print-to-pdf")) {
				if (has_print_to_pdf) {
					continue;
				}

				has_print_to_pdf = true;

				if (const auto pos = option.find('='); pos != std::string::npos) {
					std::string value = option.substr(pos + 1);
					if (!value.empty()) {
						value = strings::replace(value, "{{dest_dir}}", pdf_dir.string());
						value = strings::replace(value, "{{ dest_dir }}", pdf_dir.string());

						std::filesystem::path path(value);
						if (path.is_relative()) {
							path = pdf_dir / path;
						}

						pdf_path = path.lexically_normal();
					}
				}
				continue;
			}

			if (option.starts_with("--user-data-dir")) {
				has_user_data_dir = true;
			}

			args.push_back(option);
		}

		if (!has_headless) {
			args.insert(args.begin(), "--headless");
		}

		if (const std::filesystem::path parent = pdf_path.parent_path(); !parent.empty()) {
			std::filesystem::create_directories(parent);
		}

		args.push_back("--print-to-pdf=" + pdf_path.string());

		const std::filesystem::path profile_dir =
			std::filesystem::temp_directory_path() / std::format("camus-pdf-{}", getpid());
		std::error_code ec;
		std::filesystem::remove_all(profile_dir, ec);
		std::filesystem::create_directories(profile_dir, ec);

		if (!has_user_data_dir) {
			args.push_back("--user-data-dir=" + profile_dir.string());
		}

		args.push_back(path_to_file_url(html_path));

		std::string command = shell_quote(engine);
		for (const auto &arg : args) {
			command += " " + shell_quote(arg);
		}

		// 浏览器的噪声日志（如 macOS 的 CVDisplayLink 报错）不直接输出到终端，
		// 重定向到临时文件，仅在失败时提示日志位置。
		command += " 2>" + shell_quote(browser_log_path.string());

		logging::debug("pdf command: {}", command);

		bool keep_browser_log = false;
		const auto render_and_print = [&]() {
			filesystem::write_file(html_path, render::inja_render(inja_, template_source, view));

			const int status = std::system(command.c_str());
			if (status != 0) {
				keep_browser_log = true;
				logging::warn(
					"pdf generation command failed status={} log={}",
					status,
					browser_log_path.string()
				);
				return false;
			}

			if (!std::filesystem::exists(pdf_path) || std::filesystem::file_size(pdf_path, ec) == 0) {
				keep_browser_log = true;
				logging::warn(
					"pdf generation failed, output not found name={} log={}",
					pdf_path.string(),
					browser_log_path.string()
				);
				return false;
			}

			return true;
		};

		if (!render_and_print()) {
			std::filesystem::remove_all(profile_dir, ec);
			return;
		}

		const std::map<std::string, int> anchor_pages = pdf::read_anchor_pages(pdf_path);

		// 正文第一篇文章的起始页计为第 1 页；引导页已移除，直接以其锚点计算偏移。
		std::string first_article_anchor;
		for (const auto &item : view["pdf"]["toc"]) {
			if (item.value("is_dir", false)) {
				continue;
			}

			const std::string anchor = item.value("anchor", "");
			if (!anchor.empty()) {
				first_article_anchor = anchor;
				break;
			}
		}

		int page_offset = 0;
		if (const auto first = anchor_pages.find(first_article_anchor); !first_article_anchor.empty() &&
																	first != anchor_pages.end()) {
			page_offset = 1 - first->second;
		}

		bool numbered = page_offset != 0;
		for (auto &item : view["pdf"]["toc"]) {
			const auto page = anchor_pages.find(item["anchor"].get<std::string>());
			if (page == anchor_pages.end()) {
				continue;
			}

			const int number = page->second + page_offset;
			if (number < 1) {
				continue;
			}

			item["page"] = number;
			item["page_label"] = std::format("{:03}", number);
			numbered = true;
		}

		// Chrome 不支持 @counter-style 的 pad 描述符，页脚补零改为按总页数生成固定符号表。
		const int total_pages = pdf::page_count(pdf_path);
		if (numbered || total_pages > 0) {
			const int width = std::max(3, static_cast<int>(std::to_string(std::max(total_pages, 1)).size()));
			std::ostringstream symbols;
			for (int number = 1; number <= std::max(total_pages, 1); ++number) {
				if (number > 1) {
					symbols << ' ';
				}

				symbols << '"' << std::setw(width) << std::setfill('0') << number << '"';
			}

			const std::string counter_css =
				"\n@counter-style camus-page { system: fixed; first: 0; symbols: " + symbols.str() + "; }\n";

			view["pdf_page_offset"] = page_offset;
			view["pdf"]["style"] =
				render::inja_render(inja_, style_source, view) + pdf_page_style(page_conf, page_font_size) + counter_css;
			logging::debug(
				"pdf page offset={} toc page numbers resolved count={} total_pages={}",
				page_offset,
				anchor_pages.size(),
				total_pages
			);

			if (!render_and_print()) {
				logging::warn("pdf generation with toc page numbers failed, keep previous output");
			}
		}

		std::vector<pdf::outline_entry> outline_entries;

		// 前置封面、目录页的顶层书签；仅当当前模板生成了对应命名目标时加入。
		const std::pair<std::string, std::string> fixed_entries[] = {
			{label_cover, "pdf-cover"},
			{label_contents, "pdf-toc"},
		};

		for (const auto &[title, anchor] : fixed_entries) {
			if (anchor_pages.contains(anchor)) {
				outline_entries.push_back({.title = title, .anchor = anchor, .depth = 0});
			}
		}

		for (const auto &item : view["pdf"]["toc"]) {
			outline_entries.push_back({
				.title = item.value("outline_title", item.value("title", "")),
				.anchor = item.value("anchor", ""),
				.depth = item.value("depth", 0),
				.is_dir = item.value("is_dir", false),
			});
		}

		if (!outline_entries.empty() && !pdf::write_outline(pdf_path, outline_entries)) {
			logging::warn("pdf outline injection failed dest={}", pdf_path.string());
		}

		std::filesystem::remove_all(profile_dir, ec);
		if (!keep_browser_log) {
			std::filesystem::remove(browser_log_path, ec);
		}

		logging::info("pdf generated dest={}", pdf_path.string());
		logging::debug("make pdf dest={} engine={}", pdf_path.string(), engine);
	}

	writer::writer(const cmdline &cmd) : conf_(CAMUS_CONF_NAME), cmd_(cmd)
	{
		if (cmd_.workdir = filesystem::path_abs(cmd_.workdir); !std::filesystem::exists(cmd_.workdir)) {
			error::panic("work directory not exists name={}", cmd_.workdir.string());
		}

		conf_.set_watch_mode(cmd_.watch);

		inja_.add_callback("format_datetime", 1, [](const inja::Arguments &args) {
			const int unix_ts = args.at(0)->get<int>();
			return functions::format_time_t(unix_ts);
		});

		inja_.add_callback("format_date", 1, [](const inja::Arguments &args) {
			const int unix_ts = args.at(0)->get<int>();
			return functions::format_time_t(unix_ts, "%Y-%m-%d");
		});

		inja_.add_callback("with_prefix", 1, [&](const inja::Arguments &args) {
			const std::string name = args.at(0)->get<std::string>();
			return filesystem::clean_path(name, conf_.camus().public_url.path_prefix);
		});

		inja_.add_callback("with_cdn", 1, [&](const inja::Arguments &args) {
			const std::string name = args.at(0)->get<std::string>();
			const auto camus = conf_.camus();
			const std::string path = filesystem::clean_path(name, camus.public_url.path_prefix);
			if (camus.cdn_url.empty()) {
				return path;
			}

			return std::format("{}{}", camus.cdn_url, path);
		});
	}

	void writer::watch()
	{
		constexpr uint16_t server_port = 3001;
		const std::string server_addr = net::get_local_addr();
		assert(!server_addr.empty());

		std::thread server_thread([&]() {
			watch_server_.set_mount_point("/", conf_.camus().dest_dir());
			watch_server_.listen("0.0.0.0", server_port);
		});

		const std::filesystem::path watch_path = std::filesystem::weakly_canonical(cmd_.workdir);
		// 输出目录与 PDF 目录都是构建产物，必须排除；规范化路径避免工作目录经符号链接
		// （如 macOS 的 /tmp -> /private/tmp）时前缀匹配失败，导致产物参与 hash 反复触发编译
		const std::filesystem::path dest_dir = std::filesystem::weakly_canonical(conf_.camus().dest_dir());
		const std::filesystem::path pdf_dir = std::filesystem::weakly_canonical(conf_.camus().pdf_dir());

		const auto is_generated = [&](const std::filesystem::path &path) {
			const auto within = [](const std::filesystem::path &p, const std::filesystem::path &dir) {
				const std::filesystem::path relative = p.lexically_relative(dir);
				if (relative.empty()) {
					return false;
				}

				return *relative.begin() != "..";
			};

			return within(path, dest_dir) || within(path, pdf_dir);
		};

		logging::info("watch directory: {}", dest_dir.string());
		logging::info("watch server listen on http://localhost:{} or http://{}:{}", server_port, server_addr, server_port);

		while (true) {
			if (quit_) {
				break;
			}

			if (!std::filesystem::exists(watch_path)) {
				logging::fatal("watch directory not exists name={}", watch_path.string());
				continue;
			}

			// 按路径排序后再汇总，避免目录遍历顺序变化造成 hash 抖动而反复编译
			std::vector<std::pair<std::string, uint64_t>> files;
			for (auto it = std::filesystem::recursive_directory_iterator(watch_path);
				 it != std::filesystem::recursive_directory_iterator();
				 ++it) {
				const auto &entry = *it;
				const std::filesystem::path path = entry.path().lexically_normal();
				if (is_generated(path)) {
					if (entry.is_directory()) {
						it.disable_recursion_pending();
					}
					continue;
				}

				if (entry.is_directory()) {
					continue;
				}

				const std::string path_name = path.string();
				const auto ext = path.extension();
				const bool is_theme = path_name.find("/theme/") != std::string::npos;
				if (ext == ".md" || ext == ".yaml" || ext == ".json" || is_theme) {
					files.emplace_back(path_name, strings::make_hash(filesystem::read_file(path_name)));
				}
			}

			std::ranges::sort(files, {}, &std::pair<std::string, uint64_t>::first);

			std::string signature;
			for (const auto &[path, hash] : files) {
				signature += path;
				signature += ':';
				signature += std::to_string(hash);
				signature += '\n';
			}

			// 如果文件 hash 不同，就重新编译一遍
			static uint64_t current_hash = strings::make_hash(signature);
			if (const uint64_t hash = strings::make_hash(signature); current_hash != hash) {
				current_hash = hash;
				std::ignore = build();
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}

		watch_server_.stop();
		if (server_thread.joinable()) {
			server_thread.join();
		}
	}

	int writer::check()
	{
		cmd_.dryrun = true;
		std::filesystem::current_path(cmd_.workdir);

		struct check_issue {
			bool error;
			std::string text;
		};

		std::vector<check_issue> issues;
		const auto report_issue = [&issues](const bool error, std::string text) {
			issues.push_back({error, std::move(text)});
		};

		const logging::logging_level previous_level = logging::set_level(logging::ERROR_LEVEL);
		const int build_code = build();
		logging::set_level(previous_level);

		const auto camus = conf_.camus();

		if (build_code != 0) {
			report_issue(true, std::format("dry-run build failed code={}", build_code));
		}

		size_t directories = 0;
		size_t articles = 0;
		size_t hidden = 0;
		size_t hidden_in_toc = 0;
		size_t words = 0;
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &node, int) {
			if (node.is_directory()) {
				++directories;
				return;
			}

			if (node.path.extension() != ".md") {
				return;
			}

			++articles;
			words += node.render_data_.stats.word_count;

			if (node.property.visibility == catalog::hidden) {
				++hidden;
			} else if (node.property.visibility == catalog::hidden_in_toc) {
				++hidden_in_toc;
			}
		});

		const auto count_files = [](const std::filesystem::path &path) -> int {
			std::error_code ec;
			if (path.empty() || !std::filesystem::is_directory(path, ec) || ec) {
				return -1;
			}

			return filesystem::scan_path_files(path, 10000);
		};

		const int user_assets = count_files(camus.assets_dir);
		const int local_assets = count_files(paths::local_data_dir() / "assets");
		const int data_assets = count_files(paths::resource_assets_dir());

		if (user_assets <= 0 && local_assets <= 0 && data_assets <= 0) {
			report_issue(false, "no assets found; generated pages may have no styles or scripts");
		}

		const auto check_output_dir = [&report_issue](const std::filesystem::path &path, const std::string &name) {
			std::error_code ec;
			if (!std::filesystem::is_directory(path, ec) || ec) {
				return;
			}

			const int files = filesystem::scan_path_files(path, 10000);
			if (files > 200) {
				report_issue(
					true,
					std::format("{} has {} files (>200); build will abort: {}", name, files, path.string())
				);
			}
		};

		check_output_dir(camus.dest_dir(), "output dir");
		check_output_dir(camus.pdf_dir(), "pdf dir");

		const std::string pdf_engine_name = strings::trim_space(camus.pdf.engine);
		std::string pdf_state = "disabled";
		if (!pdf_engine_name.empty() && strings::to_lower(pdf_engine_name) != "disable") {
			const std::string engine = resolve_pdf_engine(pdf_engine_name);
			if (engine.empty()) {
				report_issue(true, std::format("pdf engine not found name={}", pdf_engine_name));
			}

			const bool has_template = !find_pdf_theme_path(CAMUS_PDF_TEMPLATE_NAME).empty();
			const bool has_style = !find_pdf_theme_path(CAMUS_PDF_STYLE_NAME).empty();
			const bool has_fonts = !find_pdf_theme_path(CAMUS_PDF_FONTS_NAME).empty();

			if (!has_template) {
				report_issue(true, "pdf theme page.html not found");
			}

			if (!has_style) {
				report_issue(true, "pdf theme style.css not found");
			}

			pdf_state = std::format(
				"{} template={} style={} fonts={}",
				engine.empty() ? pdf_engine_name : engine,
				has_template ? "ok" : "missing",
				has_style ? "ok" : "missing",
				has_fonts ? "ok" : "optional"
			);

			if (const std::string cover = strings::trim_space(camus.site.cover_image); !cover.empty()) {
				std::vector<std::filesystem::path> attempted;
				const std::filesystem::path cover_path = resolve_cover_image(
					cmd_.workdir,
					camus.source_dir,
					camus.assets_dir,
					cover,
					&attempted
				);

				if (cover_path.empty()) {
					std::string tried;
					for (const auto &path : attempted) {
						if (!tried.empty()) {
							tried += ", ";
						}

						tried += path.string();
					}

					report_issue(false, std::format("pdf cover image not found path={} tried=[{}]", cover, tried));
				} else {
					pdf_state += " cover=" + cover_path.string();
				}
			}
		}

		const auto count_text = [](const int value) {
			return value < 0 ? std::string("-") : std::to_string(value);
		};

		const auto row = [](const std::string &name, const std::string &value) {
			std::cout << "  " << std::format("{:<10}", name) << value << '\n';
		};

		std::cout << '\n' << strings::coloring_bright_yellow("Check:") << '\n';
		row("workdir", cmd_.workdir.string());
		row("config", (cmd_.workdir / CAMUS_CONF_NAME).string());
		row("source", camus.source_dir.string());
		row("theme", camus.render.theme_name);
		row("site", camus.site.title);
		row("public", camus.public_url.url.empty() ? "(none)" : camus.public_url.url);
		row("output", camus.dest_dir().string());
		row(
			"assets",
			std::format(
				"local={} .camus={} data={}",
				count_text(user_assets),
				count_text(local_assets),
				count_text(data_assets)
			)
		);
		row("pdf", pdf_state);
		row(
			"catalog",
			std::format(
				"dirs={} articles={} hidden={} hidden_in_toc={} words={}",
				directories,
				articles,
				hidden,
				hidden_in_toc,
				words
			)
		);
		row("build", build_code == 0 ? "ok (dry-run)" : std::format("failed code={}", build_code));

		size_t max_width = 0;
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &node, int) {
			max_width = std::max(max_width, strings::get_display_width(node.path.generic_string()));
		});

		std::cout << '\n' << strings::coloring_bright_yellow("Catalog:") << '\n';
		catalog::traverse_catalog_tree(catalog_, [&](const catalog::catalog_node &node, const int depth) {
			std::string tag = "[page]";
			if (node.is_directory()) {
				tag = "[dir ]";
			} else if (node.property.visibility == catalog::hidden) {
				tag = "[hide]";
			} else if (node.property.visibility == catalog::hidden_in_toc) {
				tag = "[toc-]";
			}

			const std::string indent(depth == 0 ? 0 : (depth - 1) * 2, ' ');
			const std::string path = strings::padding_left(
				node.path.empty() ? "/" : node.path.generic_string(),
				max_width
			);
			std::cout << "  " << tag << " " << indent << path << " -> " << node.link_url().string() << '\n';
		});

		if (!issues.empty()) {
			std::cout << '\n' << strings::coloring_bright_yellow("Issues:") << '\n';
			for (const auto &issue : issues) {
				std::cout << "  " << (issue.error ? strings::coloring_red("[error]") : strings::coloring_yellow("[warn ]"))
						  << " " << issue.text << '\n';
			}
		}

		bool failed = build_code != 0;
		for (const auto &issue : issues) {
			failed = failed || issue.error;
		}

		std::cout << '\n'
				  << (failed ? strings::coloring_red("Result: FAILED") : strings::coloring_green("Result: OK"))
				  << '\n';
		return failed ? 1 : 0;
	}

	int writer::build()
	{
		std::filesystem::current_path(cmd_.workdir);

		if (cmd_.dryrun) {
			logging::info("camus running on dryrun mode");
		}

		conf_.reload();

		// 启动时自动检测更新：结果只打印，不影响构建；watch 重建不重复检测。
		if (!auto_update_checked_ && !cmd_.dryrun && conf_.camus().auto_check_update) {
			auto_update_checked_ = true;
			std::ignore = check_update();
		}

		if (catalog_ = catalog::build_catalog_tree(conf_.camus().source_dir); catalog_.empty()) {
			logging::warn("No articles found");
			return 1;
		}

		emit_article();
		emit_toc();
		emit_friends();
		emit_sitemap();
		emit_assets();

		flush_writes();
		emit_pdf();

		logging::info("build complete articles={} output={}", catalog_.size(), conf_.camus().dest_dir().string());
		return 0;
	}
} // namespace camus
