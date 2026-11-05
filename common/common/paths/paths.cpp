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

	std::filesystem::path local_data_dir()
	{
		return CAMUS_LOCAL_DATA_DIR;
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

	std::filesystem::path find_resource(const std::filesystem::path &relative_path)
	{
		if (const std::filesystem::path local = local_data_dir() / relative_path; std::filesystem::exists(local)) {
			return local;
		}

		return find(relative_path);
	}

	std::filesystem::path find_local_or_resource(const std::filesystem::path &relative_path)
	{
		std::error_code ec;
		if (std::filesystem::exists(relative_path, ec) && !ec) {
			return relative_path;
		}

		return find_resource(relative_path);
	}

	std::filesystem::path source_data_dir()
	{
		for (const auto &root : data_dirs()) {
			std::error_code ec;
			if (std::filesystem::is_directory(root / "theme", ec) && !ec &&
				std::filesystem::is_directory(root / "assets", ec) && !ec) {
				return root;
			}
		}

		return {};
	}

	std::filesystem::path default_assets_dir()
	{
		const auto non_empty_dir = [](const std::filesystem::path &path) {
			std::error_code ec;
			return std::filesystem::is_directory(path, ec) && !ec && !std::filesystem::is_empty(path, ec) && !ec;
		};

		if (const std::filesystem::path local = local_data_dir() / "assets"; non_empty_dir(local)) {
			return local;
		}

		for (const auto &root : data_dirs()) {
			if (const std::filesystem::path candidate = root / "assets"; non_empty_dir(candidate)) {
				return candidate;
			}
		}

		return {};
	}
} // namespace camus::paths
