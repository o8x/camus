#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "build/build.h"
#include "common/cmdline/cmdline.h"
#include "common/filesystem/filesystem.h"
#include "install.h"
#include "open.h"
#include "repo.h"
#include "writer.h"

namespace
{
	// cmdline 的带值选项会强制消费下一个参数，这里把可选值选项的裸写法归一化：
	// 裸 --open 等价 --open=project，裸 --repo 等价 --repo=（只执行 git init），
	// 裸 --theme 等价 --theme=（复制整个 theme 目录）。
	std::vector<std::string> normalize_optional_value_args(const int argc, char **argv)
	{
		std::vector<std::string> args;
		for (int i = 0; i < argc; i++) {
			const std::string current = argv[i];
			if (current != "--open" && current != "--repo" && current != "--new" && current != "--theme") {
				args.push_back(current);
				continue;
			}

			std::string value = current == "--open" ? CAMUS_OPEN_TARGET_PROJECT : "";
			if (i + 1 < argc) {
				if (const std::string next = argv[i + 1]; !next.empty() && next.front() != '-') {
					value = next;
					i++;
				}
			}

			args.push_back(current + "=" + value);
		}

		return args;
	}

	void print_version()
	{
		std::cout
			<< std::format("v{} {} with CMake {} and C++ {}", PROJECT_VERSION, BUILD_TYPE, CMAKE_VERSION, CXX_STANDARD)
			<< std::endl
			<< std::format("Build Time: {} hash={}", BUILD_TIMESTAMP, GIT_COMMIT_HASH) << std::endl;
	}
} // namespace

int main(const int argc, char **argv)
{
	cmdline::parser arg;
	arg.set_program_name("Great writer Camus");
	arg.add("help", 'h', "print this help and exit");
	arg.add("version", 'v', "print version and exit");
	arg.add("install", 'i', "install camus.yaml, posts and .gitignore from data dir");
	arg.add<std::string>("new", 0, "create a new site directory and install the example files", false, "");
	arg.add("copy", 'c', "copy resources into .camus from data dir");
	arg.add<std::string>(
		"theme",
		0,
		"with --copy: copy only themes; no value copies all, value copies theme/<name>",
		false,
		""
	);
	arg.add("full", 0, "copy the complete resource tree with --copy");
	arg.add("force", 'f', "overwrite existing .camus when copying");
	arg.add<std::string>(
		"create",
		0,
		"create an article under the source directory; last segment is the file name",
		false,
		""
	);
	arg.add("build", 0, "build the site");
	arg.add("debug", 'd', "enable debugging");
	arg.add("watch", 'W', "automate building when changes occur");
	arg.add("dryrun", 'D', "dry run");
	arg.add("check", 'C', "validate configuration and run a dry build");
	arg.add<std::string>(
		"repo",
		0,
		"git init in workdir; optional value sets remote origin",
		false,
		""
	);
	arg.add<std::string>(
		"open",
		0,
		"open workdir in VS Code; use 'runtime' to open the data directory",
		false,
		CAMUS_OPEN_TARGET_PROJECT
	);
	arg.add<std::string>("workdir", 'w', "work dir", false, ".");

	const std::vector<std::string> args = normalize_optional_value_args(argc, argv);
	if (const bool parse_result = arg.parse(args); !parse_result || arg.exist("help")) {
		std::cout << arg.error_full() << std::endl << arg.usage();
		return 1;
	}

	if (arg.exist("new") && arg.get<std::string>("new").empty()) {
		std::cout << "option needs value: --new" << std::endl << std::endl << arg.usage();
		return 1;
	}

	if (arg.exist("create") && arg.get<std::string>("create").empty()) {
		std::cout << "option needs value: --create" << std::endl << std::endl << arg.usage();
		return 1;
	}

	if (arg.exist("debug") || WORK_ON_DEBUG) {
		logging::set_level(logging::DEBUG_LEVEL);
	}

	if (arg.exist("version")) {
		print_version();
		return 0;
	}

	if (arg.exist("build") || arg.exist("check")) {
		print_version();
	}

	try {
		std::filesystem::path workdir = filesystem::path_abs(arg.get<std::string>("workdir"));
		if (arg.exist("new")) {
			const std::filesystem::path target(arg.get<std::string>("new"));
			workdir = filesystem::path_abs(target.is_absolute() ? target : workdir / target);
		}

		workdir = workdir.lexically_normal();

		const camus::cmdline cmd{
			.workdir = workdir,
			.dryrun = arg.exist("dryrun"),
			.watch = arg.exist("watch"),
		};

		if (arg.exist("install") || arg.exist("new")) {
			if (const int installed = camus::install_templates(cmd.workdir); installed != 0) {
				return installed;
			}
		}

		if (arg.exist("theme") && !arg.exist("copy")) {
			std::cout << "option --theme requires --copy" << std::endl << std::endl << arg.usage();
			return 1;
		}

		if (arg.exist("copy")) {
			const int copied = arg.exist("theme")
								   ? camus::copy_theme(cmd.workdir, arg.get<std::string>("theme"), arg.exist("force"))
								   : camus::copy_resources(cmd.workdir, arg.exist("force"), arg.exist("full"));
			if (copied != 0) {
				return copied;
			}
		}

		if (arg.exist("repo")) {
			if (const int initialized = camus::init_repository(cmd.workdir, arg.get<std::string>("repo"));
				initialized != 0) {
				return initialized;
			}
		}

		if (arg.exist("create")) {
			if (const int created = camus::create_article(cmd.workdir, arg.get<std::string>("create"));
				created != 0) {
				return created;
			}
		}

		// 构建只由显式请求触发：--build、--watch、--check 或 --open。
		// 其余情况只执行已指定的操作；完全没有操作时打印用法。
		const bool build_requested =
			arg.exist("build") || arg.exist("watch") || arg.exist("check") || arg.exist("open");
		if (!build_requested) {
			const bool acted = arg.exist("install") || arg.exist("new") || arg.exist("copy") ||
							   arg.exist("repo") || arg.exist("create");
			if (!acted) {
				std::cout << arg.usage() << std::endl;
			}

			return 0;
		}

		const auto c = new camus::writer(cmd);
		if (arg.exist("check")) {
			return c->check();
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
