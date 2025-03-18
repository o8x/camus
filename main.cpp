#include <filesystem>
#include "generator.h"

int main(const int argc, char** argv) {
    std::string path = "./posts";
    if (argc == 2) {
        path = argv[1];
    }

    const std::string out_directory = "html";

    std::filesystem::remove_all(out_directory);
    std::filesystem::create_directory(out_directory);
    camus::generate_by_directory(path, out_directory);

    return 0;
}
