#include "log.h"
#include <iostream>
#include "utils.h"

namespace camus::log {
    void warn(const std::string &content) {
        log(WARNING, content);
    }

    void error(const std::string &content) {
        log(ERROR, content);
    }

    void info(const std::string &content) {
        log(INFO, content);
    }

    void log(const LEVEL level, const std::string &content) {
        std::string level_str;
        switch (level) {
            case WARNING: level_str = "WARN";
                break;
            case ERROR: level_str = "ERROR";
                std::cerr << util::get_now_time() << " [" << level_str << "] | " << content << std::endl;
                return;
            default:
                level_str = "INFO";
                break;
        }

        std::cout << util::get_now_time() << " [" << level_str << "] | " << content << std::endl;
    }
}
