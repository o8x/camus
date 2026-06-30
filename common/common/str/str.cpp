#include "str.h"

#include <iomanip>
#include <random>
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
} // namespace strings
