#include "writer.h"

#include "common/cmdline/cmdline.h"

int main(const int argc, char **argv)
{
	cmdline::parser arg;

	arg.set_program_name("Great writer Camus");
	arg.add("help", 'h', "print this help and exit");
	arg.add("version", 'v', "print version and exit");
	arg.add<std::string>("root", 'd', "root dir", false, "./");

	if (const bool parse_result = arg.parse(argc, argv); !parse_result || arg.exist("help")) {
		std::cout << arg.error_full() << std::endl << arg.usage();
		return 1;
	}

	if (arg.exist("version")) {
		std::cout
			<< std::format("v{} {} with CMake {} and C++ {}", PROJECT_VERSION, BUILD_TYPE, CMAKE_VERSION, CXX_STANDARD)
			<< std::endl
			<< std::format("Build Time: {} hash={}", BUILD_TIMESTAMP, GIT_COMMIT_LONG) << std::endl;
		return 0;
	}

	try {
		const auto c = std::make_unique<camus::writer>(arg.get<std::string>("root"));
		return c->generate();
	} catch (const std::exception &e) {
		logging::error("error: {}", e.what());
	}

	return 1;
}
