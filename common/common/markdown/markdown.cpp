#include "markdown.h"

#include <cstring>

#include "cmark-gfm.h"
#include "common/error/error.h"

namespace markdown
{
	std::pair<uint32_t, char *> markdown_to_html(const char *html, const std::string &engine)
	{
		if (engine == "cmark") {
			char *to_html = cmark_markdown_to_html(html, strlen(html), CMARK_OPT_DEFAULT);
			return std::make_pair(strlen(to_html), to_html);
		}

		error::panic("invalid engine specified name={}", engine);
		return {};
	}
} // namespace markdown
