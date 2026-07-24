#pragma once

#include <string>

#include "cmark-gfm.h"
#include "inja/inja.hpp"

namespace render
{
	std::string render_markdown(const char *html, const std::string &engine, const int opt = CMARK_OPT_DEFAULT);

	std::string inja_render(inja::Environment env, const std::string &html, const nlohmann::json &data);
	std::string inja_render(const std::string &html, const nlohmann::json &data);
} // namespace render
