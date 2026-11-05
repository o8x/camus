#include "yaml_config.h"

#include <filesystem>
#include <fstream>
#include <iostream>

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
	} // namespace

	void yaml_config::supplement_other_vars()
	{
		const std::filesystem::path basedir = std::filesystem::current_path();

		public_config_["site"] = camus_.site;
		public_config_["route"] = routes_;
		public_config_["source_dir"] = camus_.source_dir;
		public_config_["assets_dir"] = camus_.assets_dir;
		public_config_["output"] = camus_.output;
		public_config_["deploy"] = camus_.deploy;
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
<a class="site-stats-link" href="{{ deploy.path_prefix }}/">Home</a> /
<a class="site-stats-link" href="{{ deploy.path_prefix }}/friends.html">Friends</a> /
<a class="site-stats-link" href="{{ deploy.path_prefix }}/stats.html">Stats</a> /
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

	void yaml_config::reload()
	{
		if (!std::filesystem::exists(filename_)) {
			error::panic("config file not fount name={}", filename_);
		}

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

		for (const auto &it : camus_data) {
			const auto key = strings::trim_space(it.first.as<std::string>());

			if (key == "render") {
				camus_.render.pdf_engine = get_string(it.second["pdf_engine"]);
				camus_.render.theme_name = get_string(it.second["theme"]);

				// 有值就生成
				if (!camus_.render.pdf_engine.empty()) {
					for (const auto &opt : it.second["pdf_options"]) {
						camus_.render.pdf_options.push_back(opt.as<std::string>());
					}
				}

				camus_.render.markdown_options = CMARK_OPT_DEFAULT;
				if (WORK_ON_DEBUG) {
					camus_.render.markdown_options |= CMARK_OPT_SOURCEPOS;
				}

				for (const auto &opt : it.second["markdown_options"]) {
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

				continue;
			}

			if (key == "output") {
				camus_.output.filename_case = get_string(it.second["filename_case"]);
				camus_.output.sitemap = it.second["sitemap"].as<bool>();
				camus_.output.meta = it.second["meta"].as<bool>();
				continue;
			}

			if (key == "deploy") {
				camus_.deploy.protocol = get_string(it.second["protocol"]);
				camus_.deploy.domain_name = get_string(it.second["domain_name"]);
				camus_.deploy.path_prefix = get_string(it.second["path_prefix"]);

				if (!camus_.deploy.protocol.empty()) {
					if (!camus_.deploy.protocol.ends_with("://")) {
						camus_.deploy.protocol = camus_.deploy.protocol + "://";
					}
				}

				if (!camus_.deploy.domain_name.empty()) {
					camus_.deploy.protocol = camus_.deploy.protocol.empty() ? "https://" : camus_.deploy.protocol;
					camus_.deploy.domain_name = strings::replace(camus_.deploy.domain_name, "/", "");
					camus_.deploy.domain_name = strings::replace(camus_.deploy.domain_name, " ", "");
				}

				if (!camus_.deploy.path_prefix.empty()) {
					camus_.deploy.path_prefix = filesystem::clean_path(camus_.deploy.path_prefix, "/");
				}
				continue;
			}
		}

		if (std::filesystem::is_directory("src")) {
			camus_.source_dir = "src";
		} else if (std::filesystem::is_directory("posts")) {
			camus_.source_dir = "posts";
		} else {
			camus_.source_dir = "src";
		}

		camus_.assets_dir = "assets";

		public_config_["camus"] = camus_;
		public_config_["camus"]["render"]["options"] =
			std::format("{}({})", camus_.render.markdown_options, strings::string_join(markdown_options, ","));

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

		const YAML::Node catalog_data = root["catalog"];
		if (!catalog_data.IsSequence()) {
			error::panic("parse catalog config failed");
		}

		// 解析 TOC
		for (const auto &item : catalog_data) {
			route_conf c{};

			if (item["title"]) {
				c.title = strings::trim_space(item["title"].as<std::string>());
				c.title = strings::replace(c.title, "\n", "<br />");
			}

			if (item["subtitle"]) {
				c.subtitle = strings::trim_space(item["subtitle"].as<std::string>());
				c.subtitle = strings::replace(c.subtitle, "\n", "<br />");
			}

			if (item["description"]) {
				c.description = strings::trim_space(item["description"].as<std::string>());
				c.description = strings::replace(c.description, "\n", "<br />");
			}

			if (item["path"]) {
				c.path = filesystem::clean_path(item["path"].as<std::string>(), "/");
			} else {
				logging::debug(
					"ignore catalog title={} subtitle={} description={}",
					c.title,
					c.subtitle,
					c.description
				);
				continue;
			}

			if (path_contains_private(c.path)) {
				logging::warn("ignore catalog using private directory path={}", c.path);
				continue;
			}

			if (const YAML::Node transfers = item["transfers"]; transfers && c.path != "/" && transfers.IsSequence()) {
				for (const auto &tr : transfers) {
					route_transfer_conf t{
						.source = tr["source"].as<std::string>(),
						.target = tr["target"].as<std::string>()
					};

					if (t.source == "." || t.source == "/") {
						t.source = c.path;
					} else {
						// 加上文件夹前缀，使文件名完整
						t.source = c.path / t.source;
					}

					c.transfers.push_back(t);
				}
			}

			routes_.push_back(c);
		}

		// 为空的继承根目录属性，如果根目录存在
		camus_.site = {.path = "/", .title = "A mysterious site"};
		for (auto &c : routes_) {
			if (c.path == "/") {
				camus_.site = c;
				break;
			}
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
} // namespace camus::config
