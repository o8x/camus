#pragma once
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace camus::util {
    std::string get_now_time(const std::string& format = "%Y-%m-%d %H:%M:%S");

    std::vector<std::string> split(const std::string& str, const std::string& delimiter);

    std::vector<std::string> split(const std::string& str, char delimiter);

    std::string uuid_v4();

    std::string trim_space(const std::string& str);

    bool write_file(const std::string& filename, const std::string& content);

    void replace_all(std::string& str, const std::string& from, const std::string& to);

    std::string join(const std::vector<std::string>& vec, const std::string& delimiter);

    std::string get_cwd();

    std::string url_encode(const std::string& value);
}
