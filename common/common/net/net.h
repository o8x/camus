#pragma once

#include <string>

namespace net
{
	std::string get_local_addr(const std::string &dns_server = "114.114.114.114");
} // namespace net
