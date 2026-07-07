#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "common/error/error.h"

namespace strings
{
	// 生成UUID
	std::string uuid_v4();
	// 清除两侧空格
	std::string trim_space(const std::string &str);
	// 拆分字符串
	std::vector<std::string> split(const std::string_view &s, char delimiter);
	std::vector<std::string> split(std::string_view s, std::string_view delimiter, size_t n = 0);
	// 替换字符串
	std::string replace(const std::string &str, const std::string &from, const std::string &to);
	// 连接字符串
	std::string string_join(const std::vector<std::string> &vec, const std::string &delimiter);
	// 转义 URL 中的 unicode 字符
	std::string url_encode(const std::string &value);
	// 将字符串转换为大写
	std::string to_lower(const std::string_view &s);

	// 将字符串转换为各种类型
	template <class T> T convert_type(const std::string &data)
	{
		if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> ||
					  std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t>) {
			return std::stol(data);
		} else if constexpr (std::is_same_v<T, uint> || std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
							 std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>) {
			return std::stoul(data);
		} else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
			return std::stod(data);
		} else if constexpr (std::is_same_v<T, std::string>) {
			return data;
		}

		error::panic("unsupported type");
		return T{};
	}
} // namespace strings
