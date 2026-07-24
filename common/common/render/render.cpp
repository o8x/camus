#include "render.h"

#include <cstring>
#include <memory>

#include "cmark-gfm.h"
#include "common/error/error.h"
#include "common/str/str.h"

namespace render
{
	std::string render_markdown(const char *html, const std::string &engine, const int opt)
	{
		if (engine == "cmark-gfm") {
			const std::unique_ptr<char, decltype(&free)> html_ptr{
				cmark_markdown_to_html(html, strlen(html), opt),
				free
			};

			if (!html_ptr) {
				return {};
			}

			return std::string{html_ptr.get()};
		}

		logging::fatal("unsupported engine type name={}", engine);
		return "";
	}

	std::string inja_render(inja::Environment env, const std::string &html, const nlohmann::json &data)
	{
		try {
			return env.render(html, data);
		} catch (const inja::InjaError &e) {
			std::cout << strings::mark_string(html, e.location.line, e.location.column);
			logging::fatal("failed to render html error={}", e.what());
		}

		return "";
	}

	static inja::Environment inja_env;

	std::string inja_render(const std::string &html, const nlohmann::json &data)
	{
		return inja_render(inja_env, html, data);
	}
} // namespace render
