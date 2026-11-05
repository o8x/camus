#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "build/build.h"
#include "common/cmdline/cmdline.h"
#include "common/filesystem/filesystem.h"
#include "install.h"
#include "open.h"
#include "writer.h"

namespace
{
	// cmdline 的带值选项会强制消费下一个参数，这里把裸 --open 归一化为 --open=project，
	// 同时允许 --open runtime 这种可选值写法。
	std::vector<std::string> normalize_open_args(const int argc, char **argv)
	{
		std::vector<std::string> args;
		for (int i = 0; i < argc; i++) {
			if (const std::string current = argv[i]; current != "--open") {
				args.push_back(current);
				continue;
			}

			std::string target = CAMUS_OPEN_TARGET_PROJECT;
			if (i + 1 < argc) {
				if (const std::string next = argv[i + 1]; !next.empty() && next.front() != '-') {
					target = next;
					i++;
				}
			}

			args.push_back("--open=" + target);
		}

		return args;
	}
} // namespace

int main(const int argc, char **argv)
{
	cmdline::parser arg;
	arg.set_program_name("Great writer Camus");
	arg.add("help", 'h', "print this help and exit");
	arg.add("version", 'v', "print version and exit");
	arg.add("install", 'i', "install camus.yaml, catalog.yaml, posts and .gitignore from data dir");
	arg.add("copy", 'c', "copy full resources into .camus from data dir");
	arg.add("force", 'f', "overwrite existing .camus when copying");
	arg.add("debug", 'd', "enable debugging");
	arg.add("watch", 'W', "automate building when changes occur");
	arg.add("dryrun", 'D', "dry run");
	arg.add("inspect", 'I', "inspect current configure");
	arg.add<std::string>(
		"open",
		0,
		"open workdir in VS Code; use 'runtime' to open the data directory",
		false,
		CAMUS_OPEN_TARGET_PROJECT
	);
	arg.add<std::string>("workdir", 'w', "work dir", false, ".");

	const std::vector<std::string> args = normalize_open_args(argc, argv);
	if (const bool parse_result = arg.parse(args); !parse_result || arg.exist("help")) {
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
			if (const int installed = camus::install_templates(cmd.workdir); installed != 0) {
				return installed;
			}
		}

		if (arg.exist("copy")) {
			if (const int copied = camus::copy_resources(cmd.workdir, arg.exist("force")); copied != 0) {
				return copied;
			}
		}

		const auto c = new camus::writer(cmd);
		if (arg.exist("inspect")) {
			c->inspect();
			return 0;
		}

		std::filesystem::current_path(cmd.workdir);
		logging::info("enter work directory: {}", cmd.workdir.string());

		const int code = c->build();
		if (arg.exist("open") && code == 0) {
			if (const int opened = camus::open_target(arg.get<std::string>("open"), cmd.workdir);
				opened != 0 && !arg.exist("watch")) {
				return opened;
			}
		}

		if (!arg.exist("watch")) {
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
