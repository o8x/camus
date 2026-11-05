#include <iostream>
#include <thread>

#include "build/build.h"
#include "common/cmdline/cmdline.h"
#include "common/filesystem/filesystem.h"
#include "install.h"
#include "writer.h"

int main(const int argc, char **argv)
{
	cmdline::parser arg;
	arg.set_program_name("Great writer Camus");
	arg.add("help", 'h', "print this help and exit");
	arg.add("version", 'v', "print version and exit");
	arg.add("install", 'i', "install resources into .camus from data dir");
	arg.add("force", 'f', "overwrite existing .camus when installing");
	arg.add("debug", 'd', "enable debugging");
	arg.add("watch", 'W', "automate building when changes occur");
	arg.add("dryrun", 'D', "dry run");
	arg.add("inspect", 'I', "inspect current configure");
	arg.add<std::string>("workdir", 'w', "work dir", false, ".");

	if (const bool parse_result = arg.parse(argc, argv); !parse_result || arg.exist("help")) {
		std::cout << arg.error_full() << std::endl << arg.usage();
		return 1;
	}

	if (arg.exist("debug") || WORK_ON_DEBUG) {
		logging::set_level(logging::DEBUG_LEVEL);
	}

	if (arg.exist("version")) {
		std::cout
			<< std::format("v{} {} with CMake {} and C++ {}", PROJECT_VERSION, BUILD_TYPE, CMAKE_VERSION, CXX_STANDARD)
			<< std::endl
			<< std::format("Build Time: {} hash={}", BUILD_TIMESTAMP, GIT_COMMIT_HASH) << std::endl;
		return 0;
	}

	try {
		const camus::cmdline cmd{
			.workdir = filesystem::path_abs(arg.get<std::string>("workdir")),
			.dryrun = arg.exist("dryrun"),
		};

		if (arg.exist("install")) {
			if (const int installed = camus::install_resources(cmd.workdir, arg.exist("force")); installed != 0) {
				return installed;
			}
		}

		const auto c = new camus::writer(cmd);
		if (arg.exist("inspect")) {
			c->inspect();
			return 0;
		}

		std::filesystem::current_path(cmd.workdir);
		logging::info("enter work directory: {}", cmd.workdir.string());

		if (const int code = c->build(); !arg.exist("watch")) {
			return code;
		}

		c->watch();
	} catch (const std::exception &e) {
		logging::fatal("exception exit error={}", e.what());

		if constexpr (WORK_ON_DEBUG) {
			throw std::runtime_error(std::format("exception error={}", e.what()));
		}
	}

	return 0;
}
