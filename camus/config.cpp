#include "config.h"

#include <fstream>
#include <iostream>

#include "utils.h"

namespace camus {
    ini& ini::get() {
        static ini i;
        return i;
    }

    config& ini::all() {
        return get().config_;
    }

    void ini::fill(std::string& data) {
        util::replace_all(data, "{{main-title}}", all().main_title);
        util::replace_all(data, "{{main-subtitle}}", all().main_subtitle);
        util::replace_all(data, "{{main-description}}", all().main_description);
    }

    std::string ini::make_key(const std::string& section, const std::string& key) {
        return section + "::" + key;
    }

    std::unordered_map<std::string, std::string> ini::parse(const std::string& name) {
        std::ifstream file(name);
        std::string line, section;

        std::unordered_map<std::string, std::string> result;

        while (std::getline(file, line)) {
            // 去除行首尾的空白字符
            line.erase(line.find_last_not_of(" \r\n\t") + 1);
            if (line.empty() || line[0] == ';' || line[0] == '#') {
                // 跳过空行和注释
                continue;
            }

            if (line[0] == '[') {
                // 新的节开始
                if (size_t end = line.find(']'); end != std::string::npos) {
                    section = line.substr(1, end - 1);
                }
            } else {
                // 读取键值对
                if (size_t delimiter = line.find('='); delimiter != std::string::npos) {
                    std::string key = line.substr(0, delimiter);
                    std::string value = line.substr(delimiter + 1);
                    result[make_key(section, key)] = value;
                }
            }
        }

        file.close();

        if (!result["camus::posts_directory"].empty()) {
            config_.posts_directory = result["camus::posts_directory"];
        }

        if (!result["camus::out_directory"].empty()) {
            config_.out_directory = result["camus::out_directory"];
        }

        if (!result["camus::main_title"].empty()) {
            config_.main_title = result["camus::main_title"];
        }

        if (!result["camus::main_description"].empty()) {
            config_.main_description = result["camus::main_description"];
        }

        if (!result["camus::main_subtitle"].empty()) {
            config_.main_subtitle = result["camus::main_subtitle"];
        }

        if (!result["template::home_template_file"].empty()) {
            config_.home_template_file = result["template::home_template_file"];
        }

        if (!result["template::page_template_file"].empty()) {
            config_.page_template_file = result["template::page_template_file"];
        }

        file_ = name;

        return result;
    }

    void ini::write(const std::unordered_map<std::string, std::string>& config) const {
        std::ofstream file(file_);
        std::string current_section;

        for (const auto& pair : config) {
            size_t delimiter = pair.first.find("::");
            std::string section = pair.first.substr(0, delimiter);
            std::string key = pair.first.substr(delimiter + 2);
            if (section != current_section) {
                // 写入新的节
                file << "[" << section << "]\n";
                current_section = section;
            }

            // 写入键值对
            file << key << "=" << pair.second << "\n";
        }

        file.close();
    }
}
