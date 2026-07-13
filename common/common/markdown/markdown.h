#pragma once

#include <cstdint>
#include <string>

#include "cmark-gfm.h"

namespace markdown
{
	std::string render_markdown(const char *html, const std::string& engine, const int opt = CMARK_OPT_DEFAULT);
} // namespace markdown
