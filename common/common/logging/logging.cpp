#include "logging.h"

#include <iostream>
#include <mutex>
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

	namespace
	{
		const char *default_tag(const logging_level level)
		{
			if (level == FATAL_LEVEL) {
				return "FATA";
			}

			if (level == ERROR_LEVEL) {
				return "ERRO";
			}

			if (level == WARN_LEVEL) {
				return "WARN";
			}

			if (level == INFO_LEVEL) {
				return "INFO";
			}

			return "DEBU";
		}

		void append_prefix(std::stringstream &ss, const logging_level level, const std::string &tag)
		{
			ss << functions::get_now_time();

			if (level == FATAL_LEVEL) {
				ss << color::MAGENTA;
			} else if (level == ERROR_LEVEL) {
				ss << color::RED;
			} else if (level == WARN_LEVEL) {
				ss << color::YELLOW;
			} else if (level == INFO_LEVEL) {
				ss << color::GREEN;
			}

			ss << " [" << tag << "] ";
		}

		void write_out(const std::string &text)
		{
#ifdef HAS_SYNCSTREAM
			std::osyncstream sync_out(std::cout);
			sync_out << text;
#else
			std::lock_guard guard(mtx);
			std::cout << text << std::flush;
#endif
		}
	} // namespace

	void print_log(const logging_level level, const std::string &content)
	{
		print_log(level, default_tag(level), content);
	}

	void print_log(const logging_level level, const std::string &tag, const std::string &content)
	{
		// 验证日志级别
		if (level < log_level) {
			return;
		}

		std::stringstream ss;
		append_prefix(ss, level, tag);

		std::string text = content;
		text = strings::replace(content, std::string(getenv("HOME")), "~");
		text = strings::trim_space(text);
		ss << text << color::RESET << std::endl;

		write_out(ss.str());
	}

	void print_log_inline(const logging_level level, const std::string &tag, const std::string &content)
	{
		// 验证日志级别
		if (level < log_level) {
			return;
		}

		std::stringstream ss;
		// 回到行首并清空整行，避免上一次刷新的残留字符。
		ss << "\r\033[2K";
		append_prefix(ss, level, tag);

		std::string text = content;
		text = strings::replace(content, std::string(getenv("HOME")), "~");
		text = strings::trim_space(text);
		ss << text << color::RESET;

		write_out(ss.str());
	}
} // namespace logging
