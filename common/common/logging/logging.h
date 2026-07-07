#pragma once

#include <format>
#include <string>

namespace logging
{
	enum LEVEL { DEBUG, INFO, WARN, ERROR, FATAL };

	void print_log(LEVEL level, const std::string &content);

	template <typename... Args> void warn(const std::string &format_str, Args &&...args)
	{
		print_log(WARN, std::vformat(format_str, std::make_format_args(args...)));
	}

	template <typename... Args> void debug(const std::string &format_str, Args &&...args)
	{
		print_log(DEBUG, std::vformat(format_str, std::make_format_args(args...)));
	}

	template <typename... Args> void info(const std::string &format_str, Args &&...args)
	{
		print_log(INFO, std::vformat(format_str, std::make_format_args(args...)));
	}

	template <typename... Args> void error(const std::string &format_str, Args &&...args)
	{
		print_log(ERROR, std::vformat(format_str, std::make_format_args(args...)));
	}

	template <typename... Args> void fatal(const std::string &format_str, Args &&...args)
	{
		print_log(FATAL, std::vformat(format_str, std::make_format_args(args...)));
		std::exit(127);
	}

} // namespace logging
