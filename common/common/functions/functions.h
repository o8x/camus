#pragma once

#include <iomanip>
#include <string>
#include <unistd.h>
#include <vector>
#include <cstdint>

namespace functions
{
	std::string get_now_time(const std::string &format = "%Y-%m-%d %H:%M:%S");

	std::vector<std::string> split(const std::string &str, const std::string &delimiter);

	std::vector<std::string> split(const std::string &str, char delimiter);

	uint64_t rand_int(const uint64_t min, const uint64_t max);
	std::string uuid_v4();

	std::string trim_space(const std::string &str);

	std::string replace(const std::string &str, const std::string &from, const std::string &to);

	std::string string_join(const std::vector<std::string> &vec, const std::string &delimiter);

	std::string get_cwd();

	std::string url_encode(const std::string &value);

	time_t datetime_to_unix(const std::string &datetime);

	std::string format_time_t(const time_t timestamp, const std::string &format = "%Y-%m-%d %H:%M:%S");
} // namespace functions
