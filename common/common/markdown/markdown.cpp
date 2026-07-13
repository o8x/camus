#include "markdown.h"

#include <cstring>
#include <memory>

#include "cmark-gfm.h"
#include "common/error/error.h"

namespace markdown
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
} // namespace markdown
