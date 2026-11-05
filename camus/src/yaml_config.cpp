#include "yaml_config.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>

#include <build/build.h>

#include "cmark-gfm.h"
#include "common/error/error.h"
#include "common/filesystem/filesystem.h"
#include "common/functions/functions.h"
#include "common/paths/paths.h"
#include "common/render/render.h"
#include "common/str/str.h"
#include "inja/inja.hpp"
#include "yaml-cpp/yaml.h"

namespace camus::config
{
	namespace
	{
		bool path_contains_private(const std::filesystem::path &path)
		{
			for (const auto &part : path) {
				if (part.filename() == CAMUS_PRIVATE_DIR) {
					return true;
				}
			}

			return false;
		}

		struct directory_meta {
			std::string title;
			std::string subtitle;
			std::string description;
		};

		std::string normalize_meta(std::string value)
		{
			value = strings::trim_space(value);
			return strings::replace(value, "\n", "<br />");
		}

		// README.md 支持可选的 YAML Front Matter（title/subtitle/description）；
		// 未提供 title 时回退到正文第一个一级标题，未提供 description 时回退到正文。
		directory_meta read_readme_meta(const std::filesystem::path &dir)
		{
			directory_meta meta;

			const std::filesystem::path readme = dir / CAMUS_README_NAME;
			if (!std::filesystem::is_regular_file(readme)) {
				return meta;
			}

			const std::string raw = filesystem::read_file(readme.string(), true);
			if (raw.empty()) {
				return meta;
			}

			std::vector<std::string> lines = strings::split(raw, "\n");
			size_t body_start = 0;

			if (!lines.empty() && strings::trim_space(lines.front()) == "---") {
				size_t end = 0;
				for (size_t i = 1; i < lines.size(); ++i) {
					if (strings::trim_space(lines[i]) == "---") {
						end = i;
						break;
					}
				}

				if (end > 0) {
					const std::vector<std::string> front_lines(lines.begin() + 1, lines.begin() + end);
					try {
						if (const YAML::Node node = YAML::Load(strings::string_join(front_lines, "\n"));
							node.IsMap()) {
							const auto read = [&node](const char *key) -> std::string {
								if (const YAML::Node value = node[key]; value && value.IsScalar()) {
									return value.as<std::string>();
								}

								return "";
							};

							meta.title = read("title");
							meta.subtitle = read("subtitle");
							meta.description = read("description");
						}
					} catch (const std::exception &e) {
						logging::warn(
							"parse readme front matter failed path={} error={}",
							filesystem::path_abs(readme),
							e.what()
						);
					}

					body_start = end + 1;
				}
			}

			std::vector<std::string> body_lines(lines.begin() + static_cast<std::ptrdiff_t>(body_start), lines.end());
			std::string heading;

			if (meta.title.empty()) {
				for (auto it = body_lines.begin(); it != body_lines.end(); ++it) {
					const std::string line = strings::trim_space(*it);
					if (line.starts_with("# ")) {
						heading = strings::trim_space(line.substr(2));
						body_lines.erase(it);
						break;
					}
				}
			}

			if (meta.title.empty()) {
				meta.title = heading;
			}

			if (meta.description.empty()) {
				meta.description = strings::string_join(body_lines, "\n");
			}

			meta.title = normalize_meta(meta.title);
			meta.subtitle = normalize_meta(meta.subtitle);
			meta.description = normalize_meta(meta.description);
			return meta;
		}

		public_url_conf parse_public_url(const std::string &raw)
		{
			public_url_conf conf;
			conf.url = strings::trim_space(raw);
			if (conf.url.empty()) {
				return conf;
			}

			std::string rest = conf.url;
			if (const auto pos = rest.find("://"); pos != std::string::npos) {
				conf.protocol = rest.substr(0, pos + 3);
				rest = rest.substr(pos + 3);
			} else {
				conf.protocol = "https://";
			}

			const auto slash = rest.find('/');
			if (slash == std::string::npos) {
				conf.domain_name = rest;
			} else {
				conf.domain_name = rest.substr(0, slash);
				conf.path_prefix = filesystem::clean_path(rest.substr(slash), "");
				while (conf.path_prefix.size() > 1 && conf.path_prefix.ends_with('/')) {
					conf.path_prefix.pop_back();
				}

				if (conf.path_prefix == "/") {
					conf.path_prefix.clear();
				}
			}

			conf.domain_name = strings::replace(conf.domain_name, " ", "");
			conf.url = conf.full_prefix();
			return conf;
		}
	} // namespace

	void yaml_config::supplement_other_vars()
	{
		const std::filesystem::path basedir = std::filesystem::current_path();

		public_config_["site"] = camus_.site;
		public_config_["route"] = routes_;
		public_config_["source_dir"] = camus_.source_dir;
		public_config_["assets_dir"] = camus_.assets_dir;
		public_config_["meta"] = camus_.output.meta;
		public_config_["sitemap"] = camus_.output.sitemap;
		public_config_["filename_case"] = camus_.output.filename_case;
		public_config_["public_url"] = camus_.public_url.url;
		public_config_["protocol"] = camus_.public_url.protocol;
		public_config_["domain_name"] = camus_.public_url.domain_name;
		public_config_["path_prefix"] = camus_.public_url.path_prefix;
		public_config_["cdn_url"] = camus_.cdn_url;
		public_config_["render"] = camus_.render;
		public_config_["friends"] = camus_.friends;

		public_config_["git"] = std::unordered_map<std::string, std::string>{
			{"repo", GIT_REPO},
			{"branch", GIT_BRANCH},
			{"commit_hash", GIT_COMMIT_HASH}
		};

		std::string compiler_version = "unknown";
#if defined(__clang__)
		compiler_version = std::format("clang v{}.{}.{}", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__) || defined(__GNUG__)
		compiler_version = std::format("gcc v{}.{}.{}", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
		compiler_version = std::format("msvc v{}", _MSC_VER);
#endif

		const time_t now = std::time(nullptr);
		char buf[20];
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

		public_config_["build"] = std::unordered_map<std::string, std::string>{
			{"basedir", basedir},
			{"timestamp", std::to_string(now * 1000)},
			{"time", buf},
			{"build_type", BUILD_TYPE},
			{"cmake_version", CMAKE_VERSION},
			{"cxx_standard", std::to_string(CXX_STANDARD)},
			{"version", PROJECT_VERSION},
			{"version_major", std::to_string(PROJECT_VERSION_MAJOR)},
			{"compiler", compiler_version},
		};

		const std::string powered_by = R"(
<a class="site-stats-link" href="{{ path_prefix }}/">Home</a> /
<a class="site-stats-link" href="{{ path_prefix }}/friends.html">Friends</a> /
<a class="site-stats-link" href="{{ path_prefix }}/stats.html">Stats</a> /
<a class="site-stats-link" href="{{ git.repo }}" target="_blank">GitHub</a>
<br />
<small>
	<em>Powered by <a href="{{ git.repo }}/releases/tag/v{{ build.version }}" target="_blank" title="Camus v{{ build.version }}">
		Camus
	</a>
	built with {{ build.compiler }}</em>
</small>
)";

		public_config_["build"]["powered_by"] = render::inja_render(powered_by, json());
	}

	yaml_config::yaml_config(const std::string &filename) : camus_({}), filename_(filename)
	{
	}

	void yaml_config::set_watch_mode(const bool enabled)
	{
		watch_mode_ = enabled;
	}

	void yaml_config::reload()
	{
		if (!std::filesystem::exists(filename_)) {
			error::panic("config file not fount name={}", filename_);
		}

		// 允许重复 reload（watch / inspect / build），先重置上一轮状态，避免路由与友链累积
		camus_ = camus_conf{};
		routes_.clear();
		transfers_.clear();
		public_config_ = nlohmann::json{};

		logging::info("load config: {}", filesystem::path_abs(filename_));

		const YAML::Node root = YAML::LoadFile(filename_);

		const YAML::Node &camus_data = root;
		if (!camus_data.IsMap()) {
			error::panic("config file parse failed");
		}

		auto get_string = [](YAML::Node node) -> std::string {
			if (node.IsNull()) {
				return "";
			}

			return node.as<std::string>();
		};

		std::vector<std::string> markdown_options;
		camus_.render.pdf_options.clear();
		camus_.render.markdown_options = CMARK_OPT_DEFAULT;
		if (WORK_ON_DEBUG) {
			camus_.render.markdown_options |= CMARK_OPT_SOURCEPOS;
		}

		for (const auto &it : camus_data) {
			const auto key = strings::trim_space(it.first.as<std::string>());

			if (key == "render") {
				logging::warn("config key 'render' is no longer supported, move theme/pdf_engine/pdf_options/markdown_options to top level");
				continue;
			}

			if (key == "theme") {
				if (const std::string theme = get_string(it.second); !theme.empty()) {
					camus_.render.theme_name = theme;
				}
				continue;
			}

			if (key == "pdf_engine") {
				camus_.render.pdf_engine = get_string(it.second);
				continue;
			}

			if (key == "pdf_options") {
				if (it.second && it.second.IsSequence()) {
					for (const auto &opt : it.second) {
						camus_.render.pdf_options.push_back(opt.as<std::string>());
					}
				}
				continue;
			}

			if (key == "markdown_options") {
				if (it.second && it.second.IsSequence()) {
					for (const auto &opt : it.second) {
						const auto name = opt.as<std::string>();
						markdown_options.push_back(name);

						if (name == "SOURCEPOS") {
							camus_.render.markdown_options |= CMARK_OPT_SOURCEPOS;
						} else if (name == "HARDBREAKS") {
							camus_.render.markdown_options |= CMARK_OPT_HARDBREAKS;
						} else if (name == "UNSAFE") {
							camus_.render.markdown_options |= CMARK_OPT_UNSAFE;
						} else if (name == "NOBREAKS") {
							camus_.render.markdown_options |= CMARK_OPT_NOBREAKS;
						} else if (name == "NORMALIZE") {
							camus_.render.markdown_options |= CMARK_OPT_NORMALIZE;
						} else if (name == "VALIDATE_UTF8") {
							camus_.render.markdown_options |= CMARK_OPT_VALIDATE_UTF8;
						} else if (name == "SMART") {
							camus_.render.markdown_options |= CMARK_OPT_SMART;
						} else if (name == "GITHUB_PRE_LANG") {
							camus_.render.markdown_options |= CMARK_OPT_GITHUB_PRE_LANG;
						} else if (name == "LIBERAL_HTML_TAG") {
							camus_.render.markdown_options |= CMARK_OPT_LIBERAL_HTML_TAG;
						} else if (name == "FOOTNOTES") {
							camus_.render.markdown_options |= CMARK_OPT_FOOTNOTES;
						} else if (name == "STRIKETHROUGH_DOUBLE_TILDE") {
							camus_.render.markdown_options |= CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE;
						} else if (name == "TABLE_PREFER_STYLE_ATTRIBUTES") {
							camus_.render.markdown_options |= CMARK_OPT_TABLE_PREFER_STYLE_ATTRIBUTES;
						} else if (name == "FULL_INFO_STRING") {
							camus_.render.markdown_options |= CMARK_OPT_FULL_INFO_STRING;
						}
					}
				}
				continue;
			}

			if (key == "meta") {
				if (it.second.IsScalar()) {
					camus_.output.meta = it.second.as<bool>();
				}
				continue;
			}

			if (key == "sitemap") {
				if (it.second.IsScalar()) {
					camus_.output.sitemap = it.second.as<bool>();
				}
				continue;
			}

			if (key == "filename_case") {
				camus_.output.filename_case = get_string(it.second);
				continue;
			}

			if (key == "public_url") {
				camus_.public_url = parse_public_url(get_string(it.second));
				continue;
			}

			if (key == "cdn_url") {
				camus_.cdn_url = strings::trim_space(get_string(it.second));
				while (camus_.cdn_url.size() > 1 && camus_.cdn_url.ends_with('/')) {
					camus_.cdn_url.pop_back();
				}
				continue;
			}
		}

		// 监听模式由本地服务访问，公网地址无意义，按未配置处理
		if (watch_mode_) {
			camus_.public_url = {};
			camus_.cdn_url.clear();
		}

		if (std::filesystem::is_directory("src")) {
			camus_.source_dir = "src";
		} else if (std::filesystem::is_directory("posts")) {
			camus_.source_dir = "posts";
		} else {
			camus_.source_dir = "src";
		}

		camus_.assets_dir = "assets";

		camus_.source_dir = filesystem::path_abs(camus_.source_dir);
		camus_.output.dest_dir = filesystem::path_abs(camus_.output.dest_dir);
		if (!camus_.assets_dir.empty()) {
			camus_.assets_dir = filesystem::path_abs(camus_.assets_dir);
		}

		if (!std::filesystem::exists(camus_.source_dir)) {
			logging::fatal("source dir not exists name={}", camus_.source_dir.string());
		}

		if (!std::filesystem::exists(camus_.output.dest_dir)) {
			logging::warn("output dir not exists name={}", camus_.output.dest_dir.string());
		}

		if (!camus_.assets_dir.empty() && !std::filesystem::exists(camus_.assets_dir)) {
			logging::warn("assets dir not exists, fallback to default assets name={}", camus_.assets_dir.string());
			camus_.assets_dir.clear();
		}

		const auto read_theme = [](const std::string &name, const std::string &type) -> std::string {
			// 本地 theme/ 覆盖优先，其次读取平台数据目录中的默认主题
			std::filesystem::path home_file;
			if (const std::string replace_theme = std::format("theme/{}/{}.html", name, type);
				std::filesystem::exists(replace_theme)) {
				home_file = replace_theme;
			} else {
				home_file = paths::find_resource(std::filesystem::path("theme") / name / std::format("{}.html", type));
			}

			if (home_file.empty() || !std::filesystem::exists(home_file)) {
				logging::fatal("read theme content failed name={}/{}", name, type);
			}

			logging::debug("load  theme contents name={}/{}", name, type);
			return filesystem::read_file(home_file);
		};

		camus_.render.theme[CAMUS_THEME_TYPE_HOME] = read_theme(camus_.render.theme_name, "home");
		camus_.render.theme[CAMUS_THEME_TYPE_PAGE] = read_theme(camus_.render.theme_name, "page");
		camus_.render.theme[CAMUS_THEME_TYPE_STATS] = read_theme(camus_.render.theme_name, "stats");
		camus_.render.theme[CAMUS_THEME_TYPE_FRIENDS] = read_theme(camus_.render.theme_name, "friends");

		// 目录元信息来自各目录下的 README.md，路由路径本身不再需要入口配置
		auto find_route = [&](const std::string &path) -> route_conf * {
			for (auto &route : routes_) {
				if (route.path == path) {
					return &route;
				}
			}

			return nullptr;
		};

		auto upsert_route = [&](const std::string &path) -> route_conf & {
			if (route_conf *route = find_route(path)) {
				return *route;
			}

			route_conf c{};
			c.path = path;
			routes_.push_back(c);
			return routes_.back();
		};

		auto apply_readme = [&](const std::string &path) {
			std::filesystem::path dir = camus_.source_dir;
			if (path != "/") {
				dir /= std::filesystem::path(path).relative_path();
			}

			const directory_meta meta = read_readme_meta(dir);
			route_conf &route = upsert_route(path);

			if (!meta.title.empty()) {
				route.title = meta.title;
			}

			if (!meta.subtitle.empty()) {
				route.subtitle = meta.subtitle;
			}

			if (!meta.description.empty()) {
				route.description = meta.description;
			}
		};

		// 扫描源目录（含根目录）中的 README.md，用于目录页元信息
		for (auto it = std::filesystem::recursive_directory_iterator(camus_.source_dir);
			 it != std::filesystem::recursive_directory_iterator(); ++it) {
			if (it->is_symlink()) {
				continue;
			}

			if (it->is_directory()) {
				if (it->path().filename() == CAMUS_PRIVATE_DIR) {
					it.disable_recursion_pending();
				}
				continue;
			}

			if (!it->is_regular_file() || it->path().filename() != CAMUS_README_NAME) {
				continue;
			}

			const std::filesystem::path relative_dir =
				std::filesystem::relative(it->path().parent_path(), camus_.source_dir);
			const std::string path = relative_dir.empty() || relative_dir == "."
										? "/"
										: filesystem::clean_path(relative_dir.generic_string(), "/");

			if (path_contains_private(path)) {
				continue;
			}

			apply_readme(path);
		}

		// build 段（兼容旧键 transfer）：{ path, transfers: [{ source, target }] }
		if (const YAML::Node transfer_data = root["build"] ? root["build"] : root["transfer"]; transfer_data) {
			if (!transfer_data.IsSequence()) {
				logging::warn("ignore invalid transfer config reason=not a sequence");
			} else {
				for (const auto &item : transfer_data) {
					if (!item.IsMap() || !item["path"]) {
						logging::warn("ignore invalid transfer config reason=missing path");
						continue;
					}

					const std::string path = filesystem::clean_path(item["path"].as<std::string>(), "/");
					if (path_contains_private(path)) {
						logging::warn("ignore transfer using private directory path={}", path);
						continue;
					}

					if (path == "/") {
						logging::warn("ignore root transfer path=/");
						continue;
					}

					const YAML::Node transfers = item["transfers"];
					if (!transfers || !transfers.IsSequence()) {
						continue;
					}

					for (const auto &tr : transfers) {
						if (!tr.IsMap() || !tr["source"] || !tr["target"]) {
							logging::warn("ignore invalid transfer path={} reason=missing source/target", path);
							continue;
						}

						route_transfer_conf t{
							.source = tr["source"].as<std::string>(),
							.target = tr["target"].as<std::string>()
						};

						if (t.target.empty()) {
							logging::warn("ignore invalid transfer path={} reason=empty target", path);
							continue;
						}

						if (t.source == "." || t.source == "/") {
							t.source = path;
						} else {
							// 加上文件夹前缀，使文件名完整
							t.source = path / t.source;
						}

						transfers_.push_back(t);

						if (route_conf *route = find_route(path)) {
							route->transfers.push_back(t);
						}
					}
				}
			}
		}

		// 根路由优先，其余按 path 排序，保证 route / meta.json 输出稳定
		std::ranges::sort(routes_, [](const route_conf &a, const route_conf &b) {
			if (const bool a_root = a.path == "/", b_root = b.path == "/"; a_root != b_root) {
				return a_root;
			}

			return a.path < b.path;
		});

		// 为空的继承根目录属性，如果根目录存在
		camus_.site = {.path = "/", .title = "A mysterious site"};
		for (auto &c : routes_) {
			if (c.path == "/") {
				camus_.site = c;
				break;
			}
		}

		if (camus_.site.title.empty()) {
			camus_.site.title = "A mysterious site";
		}

		// 如果没出现，则不会自动继承属性。因此目录会出现为空的情况
		for (auto &c : routes_) {
			c.title = c.title.empty() ? camus_.site.title : c.title;
			c.subtitle = c.subtitle.empty() ? camus_.site.subtitle : c.subtitle;
			c.description = c.description.empty() ? camus_.site.description : c.description;
		}

		if (const YAML::Node friends_data = root["friends"]; friends_data && friends_data.IsSequence()) {
			for (const auto &tr : friends_data) {
				friend_link_conf t{
					.title = tr["title"].as<std::string>(),
					.desc = tr["desc"].as<std::string>(),
					.link_url = tr["link_url"].as<std::string>(),
					.image_url = tr["image_url"].as<std::string>(),
				};

				camus_.friends.push_back(t);
			}
		}

		// 兼容聚合对象必须在 site / friends / 绝对路径都确定后生成
		nlohmann::json camus_json;
		camus_json["source_dir"] = camus_.source_dir;
		camus_json["assets_dir"] = camus_.assets_dir;
		camus_json["site"] = camus_.site;
		camus_json["meta"] = camus_.output.meta;
		camus_json["sitemap"] = camus_.output.sitemap;
		camus_json["filename_case"] = camus_.output.filename_case;
		camus_json["public_url"] = camus_.public_url.url;
		camus_json["protocol"] = camus_.public_url.protocol;
		camus_json["domain_name"] = camus_.public_url.domain_name;
		camus_json["path_prefix"] = camus_.public_url.path_prefix;
		camus_json["cdn_url"] = camus_.cdn_url;
		camus_json["render"] = camus_.render;
		camus_json["friends"] = camus_.friends;
		camus_json["render"]["options"] =
			std::format("{}({})", camus_.render.markdown_options, strings::string_join(markdown_options, ","));
		public_config_["camus"] = camus_json;

		supplement_other_vars();
	}

	camus_conf yaml_config::camus() const
	{
		return camus_;
	}

	nlohmann::json yaml_config::json() const
	{
		return public_config_;
	}

	static void
	flatten_json(const nlohmann::json &j, const std::string &prefix, std::map<std::string, std::string> &result)
	{
		if (j.is_object()) {
			for (auto &[key, value] : j.items()) {
				// 构建新路径：前缀 + "." + 键名（如果前缀非空）
				std::string new_prefix = prefix.empty() ? key : prefix + "." + key;
				flatten_json(value, new_prefix, result);
			}
		} else if (j.is_array()) {
			for (size_t i = 0; i < j.size(); ++i) {
				// 数组元素使用索引作为路径后缀，例如 "scores.0"
				std::string new_prefix = prefix + "." + std::to_string(i);
				flatten_json(j[i], new_prefix, result);
			}
		} else {
			if (j.is_string()) {
				result[prefix] = j.get<std::string>();
			} else if (j.is_number_integer()) {
				result[prefix] = std::to_string(j.get<long long>()); // 避免溢出
			} else if (j.is_number_float()) {
				result[prefix] = std::to_string(j.get<double>());
			} else if (j.is_boolean()) {
				result[prefix] = j.get<bool>() ? "true" : "false";
			} else if (j.is_null()) {
				result[prefix] = "null";
			}
		}
	}

	std::map<std::string, std::string> yaml_config::map() const
	{
		std::map<std::string, std::string> result;
		flatten_json(json(), "", result);

		return result;
	}

	std::string yaml_config::render_var(const std::string &data) const
	{
		return render::inja_render(data, json());
	}

	std::expected<route_conf, std::string> yaml_config::match_route(const std::filesystem::path &path) const
	{
		for (const auto &it : routes_) {
			if (filesystem::path_equal(path, it.path, true)) {
				return it;
			}
		}

		return std::unexpected("route not found: " + path.string());
	}

	std::filesystem::path yaml_config::match_transfer(const std::filesystem::path &path) const
	{
		for (const auto &transfer : transfers_) {
			if (filesystem::path_equal(path, transfer.source, true)) {
				return transfer.target;
			}
		}

		return {};
	}
} // namespace camus::config
