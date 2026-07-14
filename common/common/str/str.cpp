#include "str.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>

namespace strings
{
	static std::random_device rand_dev;
	static std::mt19937 rand_generator(rand_dev());

	std::vector<std::string> split(const std::string_view &s, const char delimiter)
	{
		std::vector<std::string> tokens;
		std::string token;
		std::istringstream token_stream(std::string(s.begin(), s.end()));

		while (std::getline(token_stream, token, delimiter)) {
			tokens.push_back(token);
		}

		return tokens;
	}

	std::vector<std::string> split(std::string_view s, const std::string_view delimiter, const size_t n)
	{
		std::vector<std::string> result;
		if (delimiter.empty()) {
			result.emplace_back(s);
			return result;
		}

		constexpr size_t unlimited = std::string_view::npos;
		const size_t max_parts = n == 0 ? unlimited : n;
		size_t pos = 0;
		while (result.size() + 1 < max_parts) {
			const auto found = s.find(delimiter, pos);
			if (found == std::string_view::npos) {
				break;
			}

			result.emplace_back(s.substr(pos, found - pos));
			pos = found + delimiter.size();
		}

		result.emplace_back(s.substr(pos));
		return result;
	}

	std::string rand_string(const uint16_t len)
	{
		const std::string str = "wasdijklnm";
		std::uniform_int_distribution digit(0, 9);

		std::string res;
		for (int i = 0; i < len; ++i) {
			res.append(std::to_string(digit(rand_generator)));
		}

		return res;
	}

	std::string uuid_v4()
	{
		std::uniform_int_distribution hex_digit(0, 15); // 16个值: 0-15
		std::uniform_int_distribution variant(8, 11);	// 4个值: 8,9,10,11

		std::string uuid;
		uuid.reserve(36);

		for (int i = 0; i < 36; ++i) {
			if (i == 8 || i == 13 || i == 18 || i == 23) {
				uuid += '-'; // 分隔符
			} else {
				int digit;
				if (i == 14) {						 // 版本号位置 (第15个字符, 索引14)
					digit = 4;						 // UUID v4 固定为4
				} else if (i == 19) {				 // 变体位置 (第20个字符, 索引19)
					digit = variant(rand_generator); // 8, 9, 10(a), 11(b)
				} else {
					digit = hex_digit(rand_generator); // 0-15
				}

				uuid += "0123456789abcdef"[digit];
			}
		}

		return uuid;
	}

	std::string trim_space(const std::string &str)
	{
		const size_t first = str.find_first_not_of(" \n\r\t\f\v");
		if (first == std::string::npos) {
			return ""; // 如果全是空白字符，则返回空字符串
		}

		const size_t last = str.find_last_not_of(" \n\r\t\f\v");
		return str.substr(first, last - first + 1);
	}

	std::string replace(const std::string &str, const std::string &from, const std::string &to)
	{
		std::string data = str;
		size_t start_pos = 0;
		while ((start_pos = data.find(from, start_pos)) != std::string::npos) {
			data.replace(start_pos, from.length(), to);
			start_pos += to.length();
		}

		return data;
	}

	std::string replace(const std::string &str, const std::map<std::string, std::string> &ctx)
	{
		std::string result = str;

		for (const auto &[from, to] : ctx) {
			if (!from.starts_with("{{")) {
				result = replace(result, from, to);
				continue;
			}

			result = replace(result, std::format("{}", from), to);
		}

		return result;
	}

	static std::unordered_map<std::string, std::regex> regx_history{};

	std::string replace_nocase(const std::string &str, const std::string &from, const std::string &to)
	{
		std::regex pattern;
		if (regx_history.contains(from)) {
			pattern = regx_history.at(from);
		} else {
			pattern = std::regex(from, std::regex_constants::icase);
		}

		return std::regex_replace(str, pattern, to, std::regex_constants::format_default);
	}

	std::string string_join(const std::vector<std::string> &vec, const std::string &delimiter)
	{
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

	std::string url_encode(const std::string &value)
	{
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

	std::string to_lower(const std::string_view &s)
	{
		std::string s1(s.begin(), s.end());
		std::ranges::transform(s1, s1.begin(), [](const uint8_t c) { return std::tolower(c); });
		return s1;
	}

	std::string coloring(const std::string &text, const std::string &color)
	{
		return std::format("\033[{}{}\033[0m", color, text);
	}

	// clang-format off
	std::string coloring_black(const std::string &t) {return coloring(t, "30m");}
	std::string coloring_bright_black(const std::string &t) {return coloring(t, "90m");}
	std::string coloring_red(const std::string &t) {return coloring(t, "31m");}
	std::string coloring_bright_red(const std::string &t) {return coloring(t, "91m");}
	std::string coloring_green(const std::string &t) {return coloring(t, "32m");}
	std::string coloring_bright_green(const std::string &t) {return coloring(t, "92m");}
	std::string coloring_yellow(const std::string &t) {return coloring(t, "33m");}
	std::string coloring_bright_yellow(const std::string &t) {return coloring(t, "93m");}
	std::string coloring_blue(const std::string &t) {return coloring(t, "34m");}
	std::string coloring_bright_blue(const std::string &t) {return coloring(t, "94m");}
	std::string coloring_purple(const std::string &t) {return coloring(t, "35m");}
	std::string coloring_bright_purple(const std::string &t) {return coloring(t, "95m");}
	std::string coloring_cyan(const std::string &t) {return coloring(t, "36m");}
	std::string coloring_bright_cyan(const std::string &t) {return coloring(t, "96m");}
	std::string coloring_white(const std::string &t) {return coloring(t, "37m");}
	std::string coloring_bright_white(const std::string &t) {return coloring(t, "97m");}
	std::string coloring_simple(const std::string &t) {return coloring(t, "39m");}

	// clang-format on

	size_t get_display_width(const std::string &str)
	{
		size_t width = 0;
		for (size_t i = 0; i < str.size(); ++i) {
			// 判断是否为 UTF-8 多字节字符
			if (const auto c = static_cast<unsigned char>(str[i]); (c & 0x80) == 0) {
				// ASCII 字符（0xxxxxxx），占1个宽度
				width += 1;
			} else if ((c & 0xE0) == 0xC0) {
				// 2字节 UTF-8 字符（110xxxxx），通常占1个宽度
				width += 1;
				i += 1; // 跳过后续字节
			} else if ((c & 0xF0) == 0xE0) {
				// 3字节 UTF-8 字符（1110xxxx），通常是中文，占2个宽度
				width += 2;
				i += 2; // 跳过后续字节
			} else if ((c & 0xF8) == 0xF0) {
				// 4字节 UTF-8 字符（11110xxx），如 Emoji，占2个宽度
				width += 2;
				i += 3; // 跳过后续字节
			}
		}

		return width;
	}

	std::string padding_left(const std::string &str, const size_t resize_width)
	{
		const size_t w = get_display_width(str);
		if (w >= resize_width) {
			// 如果字符串宽度已经达到或超过目标宽度，直接返回
			// 注意：如果超过，可以考虑截断，这里简单返回原字符串
			return str;
		}

		// 返回左对齐并填充空格的字符串
		return str + std::string(resize_width - w, ' ');
	}
} // namespace strings
