#include "utils.h"

#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>
#include "libgomarkdown.h"
#include <__random/random_device.h>

namespace camus::util {
    std::string get_now_time(const std::string& format) {
        const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        const std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        const std::tm now_tm = *std::localtime(&now_c);
        std::stringstream ss;

        ss << std::put_time(&now_tm, format.c_str());
        return ss.str();
    }

    std::vector<std::string> split(const std::string& str, const std::string& delimiter) {
        std::vector<std::string> tokens;
        size_t start = 0;
        size_t end = str.find(delimiter);

        while (end != std::string::npos) {
            tokens.push_back(str.substr(start, end - start));
            start = end + delimiter.length();
            end = str.find(delimiter, start);
        }

        // 添加最后一个token
        tokens.push_back(str.substr(start, end - start));

        return tokens;
    }

    std::vector<std::string> split(const std::string& str, const char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);

        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }

        return tokens;
    }

    std::string uuid_v4() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(0, 15);
        std::uniform_int_distribution<int> dis2(8, 11);

        std::stringstream ss;
        int i;

        ss << std::hex;
        for (i = 0; i < 8; i++) {
            ss << dis(gen);
        }

        ss << "-";
        for (i = 0; i < 4; i++) {
            ss << dis(gen);
        }

        ss << "-4";
        for (i = 0; i < 3; i++) {
            ss << dis(gen);
        }

        ss << "-";
        ss << dis2(gen);
        for (i = 0; i < 3; i++) {
            ss << dis(gen);
        }

        ss << "-";
        for (i = 0; i < 12; i++) {
            ss << dis(gen);
        };

        return ss.str();
    }

    std::string trim_space(const std::string& str) {
        const size_t first = str.find_first_not_of(" \n\r\t\f\v");
        if (first == std::string::npos) {
            return ""; // 如果全是空白字符，则返回空字符串
        }

        const size_t last = str.find_last_not_of(" \n\r\t\f\v");
        return str.substr(first, (last - first + 1));
    }

    bool write_file(const std::string& filename, const std::string& content) {
        std::ofstream outfile(filename);

        if (!outfile.is_open()) {
            return false;
        }

        outfile << content;
        outfile.close();

        return true;
    }

    void replace_all(std::string& str, const std::string& from, const std::string& to) {
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    }

    std::string replace(const std::string& str, const std::string& from, const std::string& to) {
        std::string data = str;
        size_t start_pos = 0;
        while ((start_pos = data.find(from, start_pos)) != std::string::npos) {
            data.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }

        return data;
    }

    std::string join(const std::vector<std::string>& vec, const std::string& delimiter) {
        std::string result;

        for (size_t i = 0; i < vec.size(); ++i) {
            result += vec[i];
            // 除了最后一个元素外，添加分隔符
            if (i < vec.size() - 1) {
                result += delimiter;
            }
        }

        return result;
    }

    std::string get_cwd() {
        char buffer[1024];
        getcwd(buffer, sizeof(buffer));

        return std::string{buffer, sizeof(buffer)};
    }

    std::string url_encode(const std::string& value) {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;

        for (const char c : value) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
                continue;
            }

            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }

        return escaped.str();
    }

    time_t datetime_to_unix(const std::string& datetime) {
        std::tm tm = {};
        std::istringstream ss(datetime);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

        return timegm(&tm);
    }

    std::string format_time_t(const time_t timestamp, const std::string& format) {
        const std::tm* tm = std::gmtime(&timestamp);

        std::ostringstream oss;
        oss << std::put_time(tm, format.c_str());
        return oss.str();
    }

    std::pair<uint32_t, char*> markdown_to_html(char* html, const bool cmark) {
        if (cmark) {
            char* to_html = cmark_markdown_to_html(html, strlen(html), CMARK_OPT_DEFAULT);
            return std::make_pair(strlen(to_html), to_html);
        }

        auto [r0, r1] = MarkdownToHTML(html);
        return std::make_pair(r0, r1);
    }
}
