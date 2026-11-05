#include "install.h"

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

#include "common/filesystem/filesystem.h"
#include "common/logging/logging.h"
#include "common/paths/paths.h"

namespace
{
	void scaffold_workdir(const std::filesystem::path &workdir, const std::filesystem::path &local_dir)
	{
		for (const auto &name : std::vector<std::string>{"posts", "camus.yaml", ".gitignore"}) {
			const std::filesystem::path source = local_dir / name;
			const std::filesystem::path target = workdir / name;

			if (!std::filesystem::exists(source) || std::filesystem::exists(target)) {
				continue;
			}

			std::filesystem::copy(source, target, std::filesystem::copy_options::recursive);
		}
	}
} // namespace

namespace camus
{
	int install_resources(const std::filesystem::path &workdir, const bool force)
	{
		const std::filesystem::path source = paths::source_data_dir();
		if (source.empty()) {
			logging::error("install failed: no resources found in data directories");
			return 1;
		}

		const std::filesystem::path local_dir = workdir / paths::local_data_dir();

		try {
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
} // namespace camus
