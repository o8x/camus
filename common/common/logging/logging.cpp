#include "logging.h"

#include "common/functions/functions.h"

#include <iostream>

namespace logging
{
	void print_log(const LEVEL level, const std::string &content)
	{
		std::string level_str;
		switch (level) {
		case ERROR:
			level_str = "ERROR";
			break;
		case FATAL:
			level_str = "FATAL";
			break;
		default:
			level_str = "INFO";
			break;
		}

		std::cout << functions::get_now_time() << " [" << level_str << "] " << content << std::endl;
	}
} // namespace logging
