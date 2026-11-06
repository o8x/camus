#include "repo.h"

#include <cstdlib>
#include <string>

#include "common/filesystem/filesystem.h"
#include "common/logging/logging.h"

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

	int run_git(const std::string &command)
	{
		logging::debug("git command: {}", command);
		return std::system(command.c_str());
	}
} // namespace

namespace camus
{
	int init_repository(const std::filesystem::path &workdir, const std::string &remote)
	{
		const std::string path = filesystem::path_abs(workdir);
		const std::string quoted_path = shell_quote(path);

		if (run_git("git -C " + quoted_path + " init -q") != 0) {
			logging::error("git init failed path={}", path);
			return 1;
		}

		if (remote.empty()) {
			logging::info("git repository initialized path={}", path);
			return 0;
		}

		const std::string quoted_remote = shell_quote(remote);
		const bool has_origin =
			run_git("git -C " + quoted_path + " remote get-url origin >/dev/null 2>&1") == 0;
		const std::string action = has_origin ? " remote set-url origin " : " remote add origin ";

		if (run_git("git -C " + quoted_path + action + quoted_remote) != 0) {
			logging::error("git remote origin failed path={} remote={}", path, remote);
			return 1;
		}

		logging::info("git repository initialized path={} remote={}", path, remote);
		return 0;
	}
} // namespace camus
