#include <filesystem>
#include <unistd.h>

#include "config.h"
#include "generator.h"
#include "common/logging/logging.h"

int main(const int argc, char** argv) {
    std::string path = "./blog";
    if (argc == 2) {
        path = argv[1];
    }

    // 切换工作目录，设置相对路径
    chdir(path.c_str());

    camus::ini::get().parse("camus.ini");
    const std::string read_directory = camus::ini::all().posts_directory;
    const std::string out_directory = camus::ini::all().out_directory;

    std::filesystem::remove_all(out_directory);
    std::filesystem::create_directory(out_directory);

    logging::info(std::format("into directory: {}", read_directory));

    try {
        camus::generate_by_directory();
    } catch (const std::exception& e) {
        logging::error(e.what());
    }

    return 0;
}
