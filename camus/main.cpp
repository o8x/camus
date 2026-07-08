#include <cassert>
#include <fstream>
#include <thread>

#include <arpa/inet.h>

#include "build/build.h"
#include "common/cmdline/cmdline.h"
#include "common/extract/extract.h"
#include "writer.h"

static int
install_template(const std::filesystem::path &exec_name, const bool force, const std::filesystem::path &root_dir)
{
	const std::filesystem::path dest_dir = CAMUS_DIR;
	std::filesystem::create_directories(root_dir / dest_dir);
	std::filesystem::current_path(root_dir);

	const std::filesystem::path version_file = dest_dir / ".version";
	if (!filesystem::path_empty(dest_dir)) {
		const std::string version = filesystem::read_file(version_file, true);
		if (!force) {
			if (version != PROJECT_VERSION) {
				logging::fatal(
					"Camus installed and version different installed={} current={}",
					version,
					PROJECT_VERSION
				);
				return 1;
			}

			return 0;
		}

		// 安全保护，如果文件过多则需要手动删除
		if (const int files = filesystem::scan_path_files(dest_dir, 10000); files > 50) {
			logging::fatal(
				"{} contains at least {} files. Please delete them manually.",
				std::filesystem::absolute(root_dir).string(),
				files
			);
		}

		logging::debug("Local version is about to be deleted version={} name={}", version, dest_dir.string());
		std::filesystem::remove_all(dest_dir);
	}

	logging::debug("read template from {}", exec_name.string());

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

		filesystem::write_file(version_file, PROJECT_VERSION);
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
	arg.add("watch", 0, "automate building when changes occur");
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

	if (arg.exist("install")) {
		int installed;
		filesystem::with_current_dir([&]() {
			installed = install_template(
				filesystem::get_self_path(argv[0]),
				arg.exist("force"),
				arg.get<std::string>("workdir")
			);
		});

		if (installed != 0) {
			return installed;
		}
	}

	try {
		const auto c = std::make_unique<camus::writer>(arg.get<std::string>("workdir"));
		if (const int code = c->build(); !arg.exist("watch")) {
			return code;
		}

		c->watch();
	} catch (const std::exception &e) {
		logging::error("error: {}", e.what());
	}

	return 0;
}
