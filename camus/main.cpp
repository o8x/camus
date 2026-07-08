#include <cassert>
#include <fstream>

#include <arpa/inet.h>

#include "build/build.h"
#include "common/cmdline/cmdline.h"
#include "common/extract/extract.h"
#include "writer.h"

static int extract_template(const std::string &exec_name, const std::string &dest_dir)
{
	std::filesystem::create_directory(dest_dir);

	if (!filesystem::path_empty(dest_dir)) {
		logging::fatal(
			"destination path '{}' already exists and is not an empty directory.",
			std::filesystem::absolute(dest_dir).string()
		);
	}

	logging::info("Creating a new Camus site in '{}'", dest_dir);
	if (std::filesystem::exists(std::format("{}/camus.yaml", CAMUS_TEMPLATE_DIR))) {
		std::filesystem::copy(CAMUS_TEMPLATE_DIR, dest_dir, std::filesystem::copy_options::recursive);
		logging::info(R"(Installed a new Camus site into '{}')", std::filesystem::absolute(dest_dir).string());
		return 0;
	}

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
		logging::info(R"(Created Camus site at '{}')", std::filesystem::absolute(dest_dir).string());
	}

	return extract_archive ? 0 : 1;
}

int main(const int argc, char **argv)
{
	cmdline::parser arg;
	arg.set_program_name("Great writer Camus");
	arg.add("help", 'h', "print this help and exit");
	arg.add("version", 'v', "print version and exit");
	arg.add<std::string>("root", 'd', "root dir", false, "./");
	arg.add<std::string>("install", 'i', "install example site", false);

	if (const bool parse_result = arg.parse(argc, argv); !parse_result || arg.exist("help")) {
		std::cout << arg.error_full() << std::endl << arg.usage();
		return 1;
	}

	if (arg.exist("version")) {
		std::cout
			<< std::format("v{} {} with CMake {} and C++ {}", PROJECT_VERSION, BUILD_TYPE, CMAKE_VERSION, CXX_STANDARD)
			<< std::endl
			<< std::format("Build Time: {} hash={}", BUILD_TIMESTAMP, GIT_COMMIT_HASH) << std::endl;
		return 0;
	}

	if (arg.exist("install")) {
		return extract_template(argv[0], arg.get<std::string>("install"));
	}

	try {
		const auto c = std::make_unique<camus::writer>(arg.get<std::string>("root"));
		return c->generate();
	} catch (const std::exception &e) {
		logging::error("error: {}", e.what());
	}

	return 1;
}
