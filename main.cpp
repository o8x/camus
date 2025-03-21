#include <filesystem>
#include <unistd.h>

#include "config.h"
#include "generator.h"
#include "log.h"

int main(const int argc, char** argv) {
    std::string path = "./blog";
    if (argc == 2) {
        path = argv[1];
    }

    // 切换工作目录，设置相对路径
    chdir(path.c_str());

    camus::ini::get().parse("camus.ini");
    const std::string read_directory = camus::ini::get().config_.posts_directory;
    const std::string out_directory = camus::ini::get().config_.out_directory;

    std::filesystem::remove_all(out_directory);
    std::filesystem::create_directory(out_directory);

    camus::log::info(std::format("into directory: {}", read_directory));

    try {
        camus::generate_by_directory();
    } catch (const std::exception& e) {
        camus::log::error(e.what());
    }

    return 0;
}
