#pragma once
#include <string>

namespace camus::log {
    enum LEVEL {
        INFO,
        WARNING,
        ERROR
    };

    void warn(const std::string &content);

    void error(const std::string &content);

    void info(const std::string &content);

    void log(LEVEL level, const std::string &content);
}
