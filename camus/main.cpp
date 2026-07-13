#include <cassert>
#include <fstream>
#include <thread>

#include <arpa/inet.h>

#include "build/build.h"
#include "common/cmdline/cmdline.h"
#include "common/extract/extract.h"
#include "common/functions/functions.h"
#include "writer.h"

static int
install_template(const std::filesystem::path &exec_name, const bool force, const std::filesystem::path &root_dir)
{
	const std::filesystem::path dest_dir = CAMUS_DIR;
	std::filesystem::create_directories(root_dir / dest_dir);
	std::filesystem::current_path(root_dir);

	const std::filesystem::path version_file = dest_dir / ".version";
	if (!filesystem::path_empty(dest_dir)) {
		const std::vector<std::string> version = strings::split(filesystem::read_file(version_file, true), "\n");
		if (!force) {
			if (version[0] != PROJECT_VERSION) {
				logging::warn(
					"The installed Camus v{} at {} is inconsistent with the current v{}",
					version[0],
					version[1],
					PROJECT_VERSION
				);
			}

			return 0;
		}

		logging::debug(
			"Local version is about to be deleted name={} version={} date={}",
			dest_dir.string(),
			version[0],
			version[1]
		);

		filesystem::empty_path(dest_dir, true);
	}

	logging::debug("separate template from {}", exec_name.string());

	std::ifstream file(exec_name, std::ios::in | std::ios::binary);
	assert(file.is_open());

	// 程序和压缩包体积被记录到了二进制文件末尾
	// | skip | count |   8   |   8   |
	// | 程序  | 压缩包 | skip  | count |
	uint32_t skip;
	uint32_t count;

	file.seekg(-8, std::ios::end);
	file.read(reinterpret_cast<char *>(&skip), sizeof(uint32_t));
	file.read(reinterpret_cast<char *>(&count), sizeof(uint32_t));
	file.seekg(ntohl(skip), std::ios::beg);

	count = ntohl(count);
	std::vector<char> buffer(count);
	file.read(buffer.data(), count);

	const bool extract_archive = extract::extract_tgz(buffer, dest_dir);
	if (extract_archive) {
		logging::info("Camus v{} installed at '{}'", PROJECT_VERSION, root_dir.string());

		filesystem::write_file(
			version_file,
			std::format("{}\n{}", PROJECT_VERSION, functions::format_time_t(time(nullptr)))
		);
		for (const auto &name : std::vector<std::string>{"assets", "posts", "camus.yaml", ".env", ".gitignore"}) {
			if (!std::filesystem::exists(name)) {
				std::filesystem::copy(dest_dir / name, name, std::filesystem::copy_options::recursive);
			}
		}
	} else {
		std::filesystem::remove(dest_dir);
	}

	return extract_archive ? 0 : 1;
}

int main(const int argc, char **argv)
{
	cmdline::parser arg;
	arg.set_program_name("Great writer Camus");
	arg.add("help", 'h', "print this help and exit");
	arg.add("version", 'v', "print version and exit");
	arg.add("install", 'i', "install example site into work dir");
	arg.add("force", 'f', "ignore errors during operation");
	arg.add("debug", 'd', "enable debugging");
	arg.add("watch", 'W', "automate building when changes occur");
	arg.add("dryrun", 'D', "dry run");
	arg.add("inspect", 'I', "inspect current configure");
	arg.add<std::string>("workdir", 'w', "work dir", false, ".");

	if (const bool parse_result = arg.parse(argc, argv); !parse_result || arg.exist("help")) {
		std::cout << arg.error_full() << std::endl << arg.usage();
		return 1;
	}

	if (arg.exist("debug") || std::string(BUILD_TYPE) == "Debug") {
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
			if (const int installed = install_template(
					filesystem::get_self_path(argv[0]),
					BUILD_TYPE == "Debug" || arg.exist("force"),
					cmd.workdir
				);
				installed != 0) {
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
		logging::fatal("error: {}", e.what());
	}

	return 0;
}
