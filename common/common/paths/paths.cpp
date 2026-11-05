#include "paths.h"

#include <cstdlib>

#include <build/build.h>

namespace camus::paths
{
	std::filesystem::path data_dir()
	{
#ifdef __APPLE__
		if (const char *home = std::getenv("HOME"); home != nullptr && *home != '\0') {
			return std::filesystem::path(home) / CAMUS_DATA_DIR_MACOS;
		}

		return std::filesystem::path("/") / CAMUS_DATA_DIR_MACOS;
#else
		return CAMUS_DATA_DIR_LINUX;
#endif
	}

	std::vector<std::filesystem::path> data_dirs()
	{
#ifdef __APPLE__
		return {data_dir(), std::filesystem::path("/") / CAMUS_DATA_DIR_MACOS};
#else
		return {data_dir()};
#endif
	}

	std::filesystem::path find(const std::filesystem::path &relative_path)
	{
		for (const auto &root : data_dirs()) {
			std::filesystem::path candidate = root / relative_path;
			std::error_code ec;
			if (std::filesystem::exists(candidate, ec) && !ec) {
				return candidate;
			}
		}

		return {};
	}

	std::filesystem::path default_assets_dir()
	{
		for (const auto &root : data_dirs()) {
			const std::filesystem::path candidate = root / "assets";
			std::error_code ec;
			if (std::filesystem::is_directory(candidate, ec) && !ec && !std::filesystem::is_empty(candidate, ec) &&
				!ec) {
				return candidate;
			}
		}

		return {};
	}
} // namespace camus::paths
