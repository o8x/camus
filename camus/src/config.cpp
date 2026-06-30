#include "config.h"

#include "common/error/error.h"
#include "common/functions/functions.h"

// 只包含这一个即可，分别包含会模板出错
#include "common/str/str.h"
#include "yaml-cpp/yaml.h"

#include <fstream>
#include <filesystem>

namespace camus
{
	config::config(const YAML::Node &root)
	{
		const YAML::Node camus_data = root["camus"];
		if (!camus_data.IsMap()) {
			error::panic("config file parse failed");
		}

		const YAML::Node site_data = root["site"];
		if (!site_data.IsMap()) {
			error::panic("parse site config failed");
		}

		for (const auto &it : camus_data) {
			const auto key = strings::trim_space(it.first.as<std::string>());
			const auto value = strings::trim_space(it.second.as<std::string>());
			if (key == "source_dir") {
				camus.source_dir = value;
			} else if (key == "output_dir") {
				camus.output_dir = value;
			} else if (key == "assets_dir") {
				camus.assets_dir = value;
			} else if (key == "sitemap") {
				camus.sitemap = value == "true";
			} else if (key == "theme") {
				camus.theme_home = std::format("theme/{}/home.html", value);
				camus.theme_page = std::format("theme/{}/page.html", value);
			} else if (key == "filename_case") {
				camus.filename_case = value;
			} else if (key == "toc_format") {
				camus.toc_format = value;
			}

			flattened_map.emplace(std::format("camus.{}", key), value);
		}

		for (const auto &it : site_data) {
			const auto key = strings::trim_space(it.first.as<std::string>());
			const auto value = strings::trim_space(it.second.as<std::string>());
			if (key == "title") {
				site.title = value;
			} else if (key == "subtitle") {
				site.subtitle = value;
			} else if (key == "description") {
				site.description = value;
			} else if (key == "url") {
				site.url = value;
			} else if (key == "repo") {
				site.repo = value;
			}

			flattened_map.emplace(std::format("site.{}", key), value);
		}

		if (const YAML::Node &toc = root["toc"]; toc && toc.IsSequence()) {
			site.toc.push_back({.dir_name = "./"});
			for (const auto &item : toc) {
				site.toc.push_back(
					{.dir_name = item["dir_name"].as<std::string>(), .title = item["title"].as<std::string>()}
				);
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
		flattened_map.emplace("build.cxx-standard", CXX_STANDARD);
		flattened_map.emplace("build.version", PROJECT_VERSION);
		flattened_map.emplace("build.version_major", PROJECT_VERSION_MAJOR);
		flattened_map.emplace("build.compiler", compiler_version);
		flattened_map.emplace("git.repo", GIT_REPO);
		flattened_map.emplace("git.branch", GIT_BRANCH);
		flattened_map.emplace("git.commit-hash", GIT_COMMIT_LONG);
		flattened_map.emplace("git.repo-clean", GIT_IS_CLEAN);
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

	conf_loader &conf_loader::get()
	{
		static conf_loader i;
		return i;
	}

	camus_conf conf_loader::camus()
	{
		return get().config_->camus;
	}

	site_conf conf_loader::site()
	{
		return get().config_->site;
	}

	void conf_loader::parse_yaml(const std::string &filename)
	{
		if (!std::filesystem::exists(filename)) {
			error::panic("config file not fount name={}", filename);
		}

		file_ = filename;
		config_ = new config(YAML::LoadFile(filename));
		config_->camus.work_dir = std::filesystem::current_path();
		logging::info("with config name={}", filename);
	}

	std::string conf_loader::render_var(const std::string &data)
	{
		std::string res = data;
		for (const auto &[key, value] : get().config_->map()) {
			res = strings::replace(res, "{{" + key + "}}", value);
		}

		return res;
	}
} // namespace camus
