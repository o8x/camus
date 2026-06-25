#pragma once

#include "common/logging/logging.h"

#include <format>
#include <string>

namespace error
{
	template <typename... Args> void panic(const std::string &format_str, Args &&...args)
	{
		logging::fatal(std::vformat(format_str, std::make_format_args(args...)));
	}
} // namespace error
