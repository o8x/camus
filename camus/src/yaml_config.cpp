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
		flattened_map.emplace("build.basedir", std::filesystem::current_path());
		flattened_map.emplace("site.title", camus_.site.title);
		flattened_map.emplace("site.subtitle", camus_.site.subtitle);
		flattened_map.emplace("site.description", camus_.site.description);

		for (const auto &r : routes_) {
			flattened_map.emplace(std::format("route.{}.path", r.path), r.path);
			flattened_map.emplace(std::format("route.{}.title", r.path), r.title);
			flattened_map.emplace(std::format("route.{}.subtitle", r.path), r.subtitle);
			flattened_map.emplace(std::format("route.{}.description", r.path), r.description);
			for (const auto &[source, target] : r.transfers) {
				flattened_map.emplace(std::format("route.{}.transfers.{}.source", r.path, source.string()), source);
				flattened_map.emplace(std::format("route.{}.transfers.{}.target", r.path, source.string()), target);
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

				flattened_map.emplace(std::format("env.{}", strings::to_lower(k)), strings::trim_space(kv[1]));
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
		flattened_map.emplace("build.timestamp", std::to_string(now * 1000));
		flattened_map.emplace("build.time", buf);
		flattened_map.emplace("build.build-type", BUILD_TYPE);
		flattened_map.emplace("build.cmake-version", CMAKE_VERSION);
		flattened_map.emplace("build.cxx-standard", std::to_string(CXX_STANDARD));
		flattened_map.emplace("build.version", PROJECT_VERSION);
		flattened_map.emplace("build.version_major", std::to_string(PROJECT_VERSION_MAJOR));
		flattened_map.emplace("build.compiler", compiler_version);
		flattened_map.emplace("git.repo", GIT_REPO);
		flattened_map.emplace("git.branch", GIT_BRANCH);
		flattened_map.emplace("git.commit-hash", GIT_COMMIT_HASH);
		flattened_map.emplace(
			"build.powered-by",
			std::format(
				R"(<small><em>Powered by <a href="{}/releases/tag/v{}" target="_blank" title="Camus v{}">Camus</a> built with {}</em></small>)",
				GIT_REPO,
				PROJECT_VERSION,
				PROJECT_VERSION,
				compiler_version
			)
		);
	}

	yaml_config::yaml_config(const std::string &filename)
	{
		if (!std::filesystem::exists(filename)) {
			error::panic("config file not fount name={}", filename);
		}

		logging::info("with config: {}", filesystem::path_abs(filename));

		const YAML::Node root = YAML::LoadFile(filename);
		const YAML::Node camus_data = root["camus"];
		if (!camus_data.IsMap()) {
			error::panic("config file parse failed");
		}

		flattened_map.emplace("build.config", filesystem::path_abs(filename));

		for (const auto &it : camus_data) {
			const auto key = strings::trim_space(it.first.as<std::string>());

			if (key == "render") {
				camus_.render.engine = it.second["engine"].as<std::string>();
				flattened_map.emplace("camus.render.engine", camus_.render.engine);

				std::vector<std::string> options;

				if (camus_.render.engine == "cmark-gfm") {
					camus_.render.options = CMARK_OPT_DEFAULT;
					if (BUILD_TYPE == "Debug") {
						camus_.render.options |= CMARK_OPT_SOURCEPOS;
					}

					for (const auto &opt : it.second["options"]) {
						const auto name = opt.as<std::string>();
						options.push_back(name);

						if (name == "SOURCEPOS") {
							camus_.render.options |= CMARK_OPT_SOURCEPOS;
						} else if (name == "HARDBREAKS") {
							camus_.render.options |= CMARK_OPT_HARDBREAKS;
						} else if (name == "UNSAFE") {
							camus_.render.options |= CMARK_OPT_UNSAFE;
						} else if (name == "NOBREAKS") {
							camus_.render.options |= CMARK_OPT_NOBREAKS;
						} else if (name == "NORMALIZE") {
							camus_.render.options |= CMARK_OPT_NORMALIZE;
						} else if (name == "VALIDATE_UTF8") {
							camus_.render.options |= CMARK_OPT_VALIDATE_UTF8;
						} else if (name == "SMART") {
							camus_.render.options |= CMARK_OPT_SMART;
						} else if (name == "GITHUB_PRE_LANG") {
							camus_.render.options |= CMARK_OPT_GITHUB_PRE_LANG;
						} else if (name == "LIBERAL_HTML_TAG") {
							camus_.render.options |= CMARK_OPT_LIBERAL_HTML_TAG;
						} else if (name == "FOOTNOTES") {
							camus_.render.options |= CMARK_OPT_FOOTNOTES;
						} else if (name == "STRIKETHROUGH_DOUBLE_TILDE") {
							camus_.render.options |= CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE;
						} else if (name == "TABLE_PREFER_STYLE_ATTRIBUTES") {
							camus_.render.options |= CMARK_OPT_TABLE_PREFER_STYLE_ATTRIBUTES;
						} else if (name == "FULL_INFO_STRING") {
							camus_.render.options |= CMARK_OPT_FULL_INFO_STRING;
						}
					}
				}

				flattened_map.emplace("camus.render.engine", camus_.render.engine);
				flattened_map.emplace(
					"camus.render.options",
					std::format("{}({})", camus_.render.options, strings::string_join(options, ","))
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
				// 默认使用本地主题
				std::string home_file = std::format("theme/{}/home.html", value);
				std::string page_file = std::format("theme/{}/page.html", value);

				// 检查是否存在 .camus/theme/default/home.html
				if (std::filesystem::exists(std::filesystem::path(CAMUS_DIR) / "theme" / value / "home.html")) {
					home_file = std::format("{}/theme/{}/home.html", CAMUS_DIR, value);
					page_file = std::format("{}/theme/{}/page.html", CAMUS_DIR, value);
				}

				if (!std::filesystem::exists(home_file)) {
					logging::fatal("theme {} not found file={}", value, std::filesystem::absolute(home_file).c_str());
				}

				camus_.theme_home = filesystem::read_file(home_file);
				camus_.theme_page = filesystem::read_file(page_file);
			} else if (key == "filename_case") {
				camus_.filename_case = value;
			} else if (key == "toc_format") {
				camus_.toc_format = value;
			}

			flattened_map.emplace(std::format("camus.{}", key), value);
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

		const YAML::Node catalog_data = root["catalog"];
		if (!catalog_data.IsSequence()) {
			error::panic("parse catalog config failed");
		}

		// 解析 TOC
		for (const auto &item : catalog_data) {
			route_conf c{
				.path = filesystem::clean_path(item["path"].as<std::string>(), "/"),
				.title = item["title"].as<std::string>(),
				.subtitle = item["subtitle"].as<std::string>(),
				.description = item["description"].as<std::string>(),
			};

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

		supplement_other_vars();
	}

	camus_conf yaml_config::camus() const
	{
		return camus_;
	}

	std::map<std::string, std::string> yaml_config::map() const
	{
		return flattened_map;
	}

	std::string yaml_config::render_var(const std::string &data) const
	{
		std::string res = data;
		for (const auto &[key, value] : flattened_map) {
			res = strings::replace(res, "{{" + key + "}}", value);
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
