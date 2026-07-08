#pragma once

#include <cstdint>
#include <format>
#include <string>

namespace logging
{
	enum logging_level : uint8_t { DEBUG_LEVEL = 1, INFO_LEVEL = 2, WARN_LEVEL = 3, ERROR_LEVEL = 4, FATAL_LEVEL = 5 };

	logging_level set_level(const logging_level level);
	void print_log(logging_level level, const std::string &content);

	template <typename... Args> void warn(const std::string &format_str, Args &&...args)
	{
		print_log(WARN_LEVEL, std::vformat(format_str, std::make_format_args(args...)));
	}

	template <typename... Args> void debug(const std::string &format_str, Args &&...args)
	{
		print_log(DEBUG_LEVEL, std::vformat(format_str, std::make_format_args(args...)));
	}

	template <typename... Args> void info(const std::string &format_str, Args &&...args)
	{
		print_log(INFO_LEVEL, std::vformat(format_str, std::make_format_args(args...)));
	}

	template <typename... Args> void error(const std::string &format_str, Args &&...args)
	{
		print_log(ERROR_LEVEL, std::vformat(format_str, std::make_format_args(args...)));
	}

	template <typename... Args> void fatal(const std::string &format_str, Args &&...args)
	{
		print_log(FATAL_LEVEL, std::vformat(format_str, std::make_format_args(args...)));
		std::exit(127);
	}

} // namespace logging
