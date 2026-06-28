#include <filesystem>
#include <unistd.h>

#include "config.h"
#include "generator.h"
#include "common/logging/logging.h"

using namespace camus;

int main(const int argc, char **argv)
{
	std::string path = "./blog";
	if (argc == 2) {
		path = argv[1];
	}

	try {
		// 切换工作目录，设置相对路径
		path = std::filesystem::absolute(path);
		std::ignore = chdir(path.c_str());
		// 加载配置
		conf_loader::get().parse_yaml(path, "camus.yaml");

		const std::string read_dir = conf_loader::camus().source_dir;
		const std::string write_dir = conf_loader::camus().output_dir;
		generate_from_directory(read_dir, write_dir);
	} catch (const std::exception &e) {
		logging::error(e.what());
	}

	return 0;
}
