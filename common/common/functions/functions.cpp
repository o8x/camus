#include "functions.h"

#include <fstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <sstream>
#include <unistd.h>

namespace functions
{
	static std::random_device rand_dev;
	static std::mt19937 rand_generator(rand_dev());

	std::string get_now_time(const std::string &format)
	{
		const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
		const std::time_t now_c = std::chrono::system_clock::to_time_t(now);
		const std::tm now_tm = *std::localtime(&now_c);
		std::stringstream ss;

		ss << std::put_time(&now_tm, format.c_str());
		return ss.str();
	}

	uint64_t rand_int(const uint64_t min, const uint64_t max)
	{
		std::uniform_int_distribution dis(min, max);

		return dis(rand_generator);
	}

	time_t datetime_to_unix(const std::string &datetime)
	{
		std::tm tm = {};
		std::istringstream ss(datetime);
		ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

		return timegm(&tm);
	}

	std::string format_time_t(const time_t timestamp, const std::string &format)
	{
		const std::tm *tm = std::gmtime(&timestamp);

		std::ostringstream oss;
		oss << std::put_time(tm, format.c_str());
		return oss.str();
	}
} // namespace functions
