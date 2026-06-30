#pragma once

#include <iomanip>
#include <string>
#include <cstdint>

namespace functions
{
	std::string get_now_time(const std::string &format = "%Y-%m-%d %H:%M:%S");

	uint64_t rand_int(const uint64_t min, const uint64_t max);


	time_t datetime_to_unix(const std::string &datetime);

	std::string format_time_t(const time_t timestamp, const std::string &format = "%Y-%m-%d %H:%M:%S");
} // namespace functions
