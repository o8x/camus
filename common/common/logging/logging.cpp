#include "logging.h"

#include <iostream>
#include <sstream>

#include <unistd.h>

#include "common/functions/functions.h"
#include "common/str/str.h"

#if defined(__clang__)
static std::mutex mtx;
#elif defined(__GNUC__) || defined(__GNUG__)
#include <syncstream>
#define HAS_SYNCSTREAM 1
#endif

namespace logging
{
	static logging_level log_level = INFO_LEVEL;

	namespace color
	{
		static bool is_supports_color()
		{
			// 检查是否输出到终端
			if (!isatty(fileno(stdout))) {
				return false;
			}

			// 遵循 NO_COLOR 标准
			if (const char *noColor = getenv("NO_COLOR"); noColor && noColor[0] != '\0') {
				return false;
			}

			// 没有 TERM 环境变量
			if (const char *term = getenv("TERM"); !term) {
				return false;
			}

			return true;
		}

		const std::string RED = is_supports_color() ? "\033[31m" : "";
		const std::string YELLOW = is_supports_color() ? "\033[33m" : "";
		const std::string GREEN = is_supports_color() ? "\033[32m" : "";
		const std::string MAGENTA = is_supports_color() ? "\033[35m" : "";
		const std::string RESET = is_supports_color() ? "\033[0m" : "";
	} // namespace color

	logging_level set_level(const logging_level level)
	{
		const logging_level current = log_level;
		log_level = level;

		debug("change logging level source={} dest={}", static_cast<uint8_t>(current), static_cast<uint8_t>(log_level));
		return log_level;
	}

	void print_log(const logging_level level, const std::string &content)
	{
		// 验证日志级别
		if (level < log_level) {
			return;
		}

		std::stringstream ss;

		ss << functions::get_now_time();

		if (level == FATAL_LEVEL) {
			ss << color::MAGENTA << " [FATA] ";
		} else if (level == ERROR_LEVEL) {
			ss << color::RED << " [ERRO] ";
		} else if (level == WARN_LEVEL) {
			ss << color::YELLOW << " [WARN] ";
		} else if (level == INFO_LEVEL) {
			ss << color::GREEN << " [INFO] ";
		} else if (level == DEBUG_LEVEL) {
			ss << " [DEBU] ";
		}

		std::string text = content;
		text = strings::replace(content, std::string(getenv("HOME")), "~");
		text = strings::trim_space(text);
		ss << text << color::RESET << std::endl;

		{
#ifdef HAS_SYNCSTREAM
			std::osyncstream sync_out(std::cout);
			sync_out << ss.str();
#else
			std::lock_guard guard(mtx);
			std::cout << ss.str() << std::flush;
#endif
		}
	}
} // namespace logging
