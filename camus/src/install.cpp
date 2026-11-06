#include "install.h"

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

#include <build/build.h>

#include "common/filesystem/filesystem.h"
#include "common/logging/logging.h"
#include "common/paths/paths.h"

namespace
{
	const std::vector<std::string> template_entries{"posts", "camus.yaml", ".gitignore"};
	const std::vector<std::filesystem::path> minimal_asset_entries{
		std::filesystem::path("css") / "style.css",
		std::filesystem::path("js") / "onload.js"
	};

	constexpr const char *private_readme = R"(# private

This directory is excluded from builds.

- Any directory named `private` is skipped when scanning sources.
- Files here are never rendered or published to the generated site or PDF.
- Use it for local drafts, notes, and private assets.
)";

	void ensure_private_dir(const std::filesystem::path &workdir)
	{
		const std::filesystem::path dir = workdir / "posts" / CAMUS_PRIVATE_DIR;
		std::filesystem::create_directories(dir);

		const std::filesystem::path readme = dir / "README.md";
		if (!std::filesystem::exists(readme)) {
			filesystem::write_file(readme, private_readme);
		}
	}

	void scaffold_workdir(const std::filesystem::path &workdir, const std::filesystem::path &source_dir)
	{
		if (!workdir.empty()) {
			std::filesystem::create_directories(workdir);
		}

		for (const auto &name : template_entries) {
			const std::filesystem::path source = source_dir / name;
			const std::filesystem::path target = workdir / name;

			if (!std::filesystem::exists(source) || std::filesystem::exists(target)) {
				continue;
			}

			std::filesystem::copy(source, target, std::filesystem::copy_options::recursive);
		}

		ensure_private_dir(workdir);
	}

	void copy_minimal_assets(const std::filesystem::path &source, const std::filesystem::path &local_dir, const bool force)
	{
		const std::filesystem::path source_assets = source / "assets";
		const std::filesystem::path target_assets = local_dir / "assets";

		std::filesystem::create_directories(target_assets / "css");
		std::filesystem::create_directories(target_assets / "js");

		for (const auto &relative : minimal_asset_entries) {
			const std::filesystem::path from = source_assets / relative;
			const std::filesystem::path to = target_assets / relative;

			if (!std::filesystem::exists(from) || (std::filesystem::exists(to) && !force)) {
				continue;
			}

			std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing);
		}

		// 字体目录采用合并查找，只需创建空目录让用户放入自己的字体，无需复制内置文件。
		std::filesystem::create_directories(target_assets / "fonts");
	}

	std::filesystem::path template_source_dir()
	{
		const std::filesystem::path config = camus::paths::find(CAMUS_CONF_NAME);
		return config.empty() ? std::filesystem::path{} : config.parent_path();
	}
} // namespace

namespace camus
{
	int install_templates(const std::filesystem::path &workdir)
	{
		const std::filesystem::path source = template_source_dir();
		if (source.empty()) {
			logging::error("install failed: no template resources found in data directories");
			return 1;
		}

		try {
			scaffold_workdir(workdir, source);
			logging::info("templates installed at '{}'", filesystem::path_abs(workdir));
		} catch (const std::exception &e) {
			logging::error("install failed error={}", e.what());
			return 1;
		}

		return 0;
	}

	int copy_resources(const std::filesystem::path &workdir, const bool force, const bool full)
	{
		const std::filesystem::path source = paths::source_data_dir();
		if (source.empty()) {
			logging::error("install failed: no resources found in data directories");
			return 1;
		}

		const std::filesystem::path local_dir = workdir / paths::local_data_dir();

		try {
			if (!full) {
				// 最小资源复制到工作目录根部（assets/），作为本地覆盖层；仅 --full 使用 .camus。
				copy_minimal_assets(source, workdir, force);
				logging::info("minimal resources installed at '{}'", filesystem::path_abs(workdir / "assets"));
				scaffold_workdir(workdir, source);
				return 0;
			}

			const bool installed = std::filesystem::is_directory(local_dir) && !std::filesystem::is_empty(local_dir);

			if (installed && !force) {
				logging::warn(
					"resources already installed at '{}', use --force to overwrite",
					filesystem::path_abs(local_dir)
				);
				scaffold_workdir(workdir, local_dir);
				return 0;
			}

			if (installed) {
				filesystem::empty_path(local_dir, true);
			}

			std::filesystem::create_directories(local_dir);
			for (const auto &entry : std::filesystem::directory_iterator(source)) {
				std::filesystem::copy(
					entry.path(),
					local_dir / entry.path().filename(),
					std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing
				);
			}

			logging::info("resources installed at '{}'", filesystem::path_abs(local_dir));
			scaffold_workdir(workdir, local_dir);
		} catch (const std::exception &e) {
			logging::error("install failed error={}", e.what());
			return 1;
		}

		return 0;
	}

	int copy_theme(const std::filesystem::path &workdir, const std::string &name, const bool force)
	{
		const std::filesystem::path source_root = paths::source_data_dir();
		if (source_root.empty()) {
			logging::error("theme copy failed: no resources found in data directories");
			return 1;
		}

		const std::filesystem::path source_theme = source_root / "theme";
		if (!std::filesystem::is_directory(source_theme)) {
			logging::error("theme copy failed: theme directory not found path={}", source_theme.string());
			return 1;
		}

		std::filesystem::path relative;
		if (!name.empty()) {
			relative = std::filesystem::path(name);
			if (relative.is_absolute()) {
				logging::error("theme copy failed: invalid theme name={}", name);
				return 1;
			}

			for (const auto &part : relative) {
				if (part == "." || part == "..") {
					logging::error("theme copy failed: invalid theme name={}", name);
					return 1;
				}
			}
		}

		const std::filesystem::path source = relative.empty() ? source_theme : source_theme / relative;
		if (!std::filesystem::is_directory(source)) {
			logging::error("theme copy failed: theme not found name={}", name);
			return 1;
		}

		const std::filesystem::path target = workdir / "theme" / relative;

		try {
			std::filesystem::create_directories(target.parent_path());

			std::filesystem::copy_options options = std::filesystem::copy_options::recursive;
			options |= force ? std::filesystem::copy_options::overwrite_existing
							 : std::filesystem::copy_options::skip_existing;
			std::filesystem::copy(source, target, options);

			// --theme 同时复制最小资源骨架（css/style.css、js/onload.js、空 fonts/）到工作目录根部。
			copy_minimal_assets(source_root, workdir, force);
		} catch (const std::exception &e) {
			logging::error("theme copy failed error={}", e.what());
			return 1;
		}

		logging::info(
			"theme copied from '{}' to '{}'",
			filesystem::path_abs(source),
			filesystem::path_abs(target)
		);
		return 0;
	}
} // namespace camus
