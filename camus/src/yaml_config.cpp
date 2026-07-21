#include "yaml_config.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <build/build.h>

#include "cmark-gfm.h"
#include "common/error/error.h"
#include "common/filesystem/filesystem.h"
#include "common/functions/functions.h"
#include "common/str/str.h"
#include "yaml-cpp/yaml.h"

namespace camus::config
{
	void yaml_config::supplement_other_vars()
	{
		const std::filesystem::path basedir = std::filesystem::current_path();

		public_map_.emplace("build.basedir", basedir);
		public_map_.emplace("site.title", camus_.site.title);
		public_map_.emplace("site.subtitle", camus_.site.subtitle);
		public_map_.emplace("site.desc", camus_.site.description);

		for (const auto &r : routes_) {
			public_map_.emplace(std::format("route.{}.path", r.path), r.path);
			public_map_.emplace(std::format("route.{}.title", r.path), r.title);
			public_map_.emplace(std::format("route.{}.subtitle", r.path), r.subtitle);
			public_map_.emplace(std::format("route.{}.description", r.path), r.description);
			for (const auto &[source, target] : r.transfers) {
				public_map_.emplace(std::format("route.{}.transfers.{}.source", r.path, source.string()), source);
				public_map_.emplace(std::format("route.{}.transfers.{}.target", r.path, source.string()), target);
			}
		}

		// 解析 ENV 文件
		if (std::filesystem::exists(".env")) {
			for (const auto &it : strings::split(filesystem::read_file(".env"), "\n")) {
				std::vector<std::string> kv = strings::split(it, "=");
				const std::string k = strings::trim_space(kv[0]);
				if (kv.size() != 2 || k.empty()) {
					continue;
				}

				// 全局词典
				if (k.starts_with("DICT_")) {
					public_map_.emplace(
						std::format("dict.{}", strings::to_lower(strings::replace(k, "DICT_", ""))),
						strings::trim_space(kv[1])
					);
				} else {
					public_map_.emplace(std::format("env.{}", strings::to_lower(k)), strings::trim_space(kv[1]));
				}
			}
		}

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
		public_map_.emplace("build.timestamp", std::to_string(now * 1000));
		public_map_.emplace("build.time", buf);
		public_map_.emplace("build.build-type", BUILD_TYPE);
		public_map_.emplace("build.cmake-version", CMAKE_VERSION);
		public_map_.emplace("build.cxx-standard", std::to_string(CXX_STANDARD));
		public_map_.emplace("build.version", PROJECT_VERSION);
		public_map_.emplace("build.version_major", std::to_string(PROJECT_VERSION_MAJOR));
		public_map_.emplace("build.compiler", compiler_version);
		public_map_.emplace("git.repo", GIT_REPO);
		public_map_.emplace("git.branch", GIT_BRANCH);
		public_map_.emplace("git.commit-hash", GIT_COMMIT_HASH);
		public_map_.emplace(
			"build.powered-by",
			std::format(
				R"(
<a class="site-stats-link" href="/">Home</a> /
<a class="site-stats-link" href="/friends.html">Friends</a> /
<a class="site-stats-link" href="/stats.html">Stats</a> /
<a class="site-stats-link" href="{{env.github_url}}" target="_blank">GitHub</a>
<br />
<small>
	<em>Powered by <a href="{}/releases/tag/v{}" target="_blank" title="Camus v{}">Camus</a> built with {}</em>
</small>
)",
				GIT_REPO,
				PROJECT_VERSION,
				PROJECT_VERSION,
				compiler_version
			)
		);

		// 不暴露真实目录
		for (auto &val : public_map_ | std::views::values) {
			val = strings::replace(val, basedir, "/");
		}
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
		const YAML::Node camus_data = root["camus"];
		if (!camus_data.IsMap()) {
			error::panic("config file parse failed");
		}

		public_map_.emplace("build.config", filesystem::path_abs(filename_));

		for (const auto &it : camus_data) {
			const auto key = strings::trim_space(it.first.as<std::string>());

			camus_.render.html_engine = "default";
			if (key == "render") {
				camus_.render.markdown_engine = it.second["markdown_engine"].as<std::string>();
				camus_.render.html_engine = it.second["html_engine"].as<std::string>();
				camus_.render.meta = it.second["meta"].as<bool>();
				public_map_.emplace("camus.render.meta", camus_.render.meta ? "true" : "false");
				public_map_.emplace("camus.render.html_engine", camus_.render.html_engine);
				public_map_.emplace("camus.render.markdown_engine", camus_.render.markdown_engine);

				std::vector<std::string> options;

				if (camus_.render.markdown_engine == "cmark-gfm") {
					camus_.render.markdown_options = CMARK_OPT_DEFAULT;
					if (WORK_ON_DEBUG) {
						camus_.render.markdown_options |= CMARK_OPT_SOURCEPOS;
					}

					for (const auto &opt : it.second["markdown_options"]) {
						const auto name = opt.as<std::string>();
						options.push_back(name);

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

				public_map_.emplace(
					"camus.render.options",
					std::format("{}({})", camus_.render.markdown_options, strings::string_join(options, ","))
				);
				continue;
			}

			auto value = strings::trim_space(it.second.as<std::string>());
			if (key == "source_dir") {
				value = filesystem::path_abs(value);
				camus_.source_dir = value;
			} else if (key == "domain_name") {
				camus_.domain_name = value;
			} else if (key == "output_dir") {
				value = filesystem::path_abs(value);
				camus_.output_dir = value;
			} else if (key == "assets_dir") {
				value = filesystem::path_abs(value);
				camus_.assets_dir = value;
			} else if (key == "sitemap") {
				camus_.sitemap = value == "true";
			} else if (key == "theme") {
				camus_.theme_name = value;
			} else if (key == "filename_case") {
				camus_.filename_case = value;
			}

			public_map_.emplace(std::format("camus.{}", key), value);
		}

		if (!std::filesystem::exists(camus_.source_dir)) {
			logging::fatal("source dir not exists name={}", camus_.source_dir.string());
		}

		if (!std::filesystem::exists(camus_.output_dir)) {
			logging::warn("output dir not exists name={}", camus_.output_dir.string());
		}

		if (!std::filesystem::exists(camus_.assets_dir)) {
			logging::fatal("assets dir not exists name={}", camus_.assets_dir.string());
		}

		if (camus_.render.html_engine != "default") {
			camus_.theme_name = std::format("{}/{}", camus_.theme_name, camus_.render.html_engine);
		}

		const auto read_theme = [](const std::string &name, const std::string &type) -> std::string {
			std::string home_file = std::format("{}/theme/{}/{}.html", CAMUS_DIR, name, type);

			// 检查是否存在覆盖主题
			if (const std::string replace_theme = std::format("theme/{}/{}.html", name, type);
				std::filesystem::exists(replace_theme)) {
				home_file = replace_theme;
			}

			if (!std::filesystem::exists(home_file)) {
				logging::fatal("read theme content failed name={}/{}", name, type);
			}

			logging::debug("load  theme contents name={}/{}", name, type);
			return filesystem::read_file(home_file);
		};

		camus_.theme[CAMUS_THEME_TYPE_HOME] = read_theme(camus_.theme_name, "home");
		camus_.theme[CAMUS_THEME_TYPE_PAGE] = read_theme(camus_.theme_name, "page");
		camus_.theme[CAMUS_THEME_TYPE_STATS] = read_theme(camus_.theme_name, "stats");
		camus_.theme[CAMUS_THEME_TYPE_FRIENDS] = read_theme(camus_.theme_name, "friends");

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

				public_map_.emplace(std::format("friends.{}.title", t.title), t.title);
				public_map_.emplace(std::format("friends.{}.desc", t.title), t.desc);
				public_map_.emplace(std::format("friends.{}.link_url", t.title), t.link_url);
				public_map_.emplace(std::format("friends.{}.image_url", t.title), t.image_url);
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
		nlohmann::json j;

		for (const auto &[name, val] : public_map_) {
			auto [key, sub_key] = strings::split_pair(name, ".");
			if (key.empty()) {
				continue;
			}

			j[key][strings::replace(sub_key, "-", "_")] = val;
		}

		return j;
	}

	std::map<std::string, std::string> yaml_config::map() const
	{
		return public_map_;
	}

	std::string yaml_config::render_var(const std::string &data) const
	{
		std::string res = data;
		for (const auto &[key, value] : public_map_) {
			std::string key_name = "{{" + key + "}}";
			// 字典替换不带括号
			if (key.starts_with("dict.")) {
				key_name = strings::replace(key, "dict.", "");
				res = strings::replace_nocase(res, key_name, value);
			} else {
				res = strings::replace(res, "{{" + key + "}}", value);
			}
		}

		return res;
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
