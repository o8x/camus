#pragma once

#include <cstdint>
#include <string>

namespace markdown
{
	std::pair<uint32_t, char *> markdown_to_html(const char *html, const std::string &engine = "cmark");
} // namespace markdown
