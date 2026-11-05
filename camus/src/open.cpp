#include "open.h"

#include <cstdlib>
#include <string>

#include <build/build.h>

#include "common/filesystem/filesystem.h"
#include "common/logging/logging.h"
#include "common/paths/paths.h"

namespace
{
	std::string shell_quote(const std::string &value)
	{
		std::string quoted = "'";
		for (const char c : value) {
			quoted += c == '\'' ? "'\\''" : std::string(1, c);
		}

		return quoted + "'";
	}

	int run_command(const std::string &command)
	{
		logging::debug("open command: {}", command);
		return std::system(command.c_str());
	}

	int open_project(const std::filesystem::path &workdir)
	{
		const std::string path = shell_quote(filesystem::path_abs(workdir));

		if (run_command("code " + path) == 0) {
			return 0;
		}

#ifdef __APPLE__
		if (run_command("open -a 'Visual Studio Code' " + path) == 0) {
			return 0;
		}
#endif

		logging::error("open project failed: VS Code is not available path={}", filesystem::path_abs(workdir));
		return 1;
	}

	int open_runtime()
	{
		const std::filesystem::path data_dir = camus::paths::data_dir();
		if (!std::filesystem::exists(data_dir)) {
			logging::error("open runtime failed: data dir not found path={}", data_dir.string());
			return 1;
		}

		const std::string path = shell_quote(data_dir.string());

#ifdef __APPLE__
		const std::string command = "open " + path;
#else
		const std::string command = "xdg-open " + path;
#endif

		if (run_command(command) != 0) {
			logging::error("open runtime failed path={}", data_dir.string());
			return 1;
		}

		return 0;
	}
} // namespace

namespace camus
{
	int open_target(const std::string &target, const std::filesystem::path &workdir)
	{
		if (target == CAMUS_OPEN_TARGET_RUNTIME) {
			return open_runtime();
		}

		if (target != CAMUS_OPEN_TARGET_PROJECT && !target.empty()) {
			logging::warn("unknown open target '{}', fallback to project", target);
		}

		return open_project(workdir);
	}
} // namespace camus
