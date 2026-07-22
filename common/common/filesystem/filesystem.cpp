#include "filesystem.h"

#include <fstream>
#include <sstream>

#include <unistd.h>

#include "common/functions/functions.h"
#include "common/logging/logging.h"
#include "common/str/str.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#else
#include <linux/limits.h>
#endif

namespace filesystem
{
	int write_file(const std::filesystem::path &path, const std::string_view &content)
	{
		std::ofstream outfile;
		if (outfile.open(std::string{path}, std::ofstream::out | std::ofstream::trunc); !outfile) {
			return -1;
		}

		outfile << content.data();
		outfile.close();
		return content.length();
	}

	std::string read_file(const std::string &name, bool trim)
	{
		std::ifstream file(name);
		if (!file.is_open()) {
			return "Could not open file: " + path_abs(name);
		}

		std::stringstream buffer;
		buffer << file.rdbuf();

		file.close();

		std::string res = buffer.str();
		return trim ? strings::trim_space(res) : res;
	}

	std::string clean_path(const std::string &path, const std::string &prefix)
	{
		const std::filesystem::path p(std::format("{}/{}", prefix, path));
		const std::filesystem::path res = p.lexically_normal();
		// Linux 下会返回 //，MacOS 不存在该问题
		if (res == "//") {
			return "/";
		}

		return res;
	}

	std::filesystem::path
	clean_path_prefix(const std::filesystem::path &path, const std::string &prefix, const bool absolute)
	{
		std::filesystem::path clean_path;
		if (absolute) {
			clean_path = std::filesystem::absolute(path).lexically_relative(std::filesystem::absolute(prefix));
		} else {
			clean_path = path.lexically_relative(prefix);
		}

		return clean_path;
	}

	bool path_empty(const std::filesystem::path &path)
	{
		if (!std::filesystem::exists(path)) {
			return false;
		}

		try {
			return std::filesystem::directory_iterator(path) == std::filesystem::directory_iterator{};
		} catch (...) {
			return false;
		}
	}

	int scan_path_files(const std::filesystem::path &path, const uint32_t max_scan)
	{
		uint32_t files = 0;
		for (const std::filesystem::directory_entry &it : std::filesystem::recursive_directory_iterator(path)) {
			if (files++; files > max_scan) {
				break;
			}
		}

		return files;
	}

	std::filesystem::path get_self_path(const std::string &arg0)
	{
#ifdef __linux__
		// Linux: 直接从 /proc 伪文件系统读取
		std::vector<char> buffer(PATH_MAX);
		if (const ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1); len != -1) {
			buffer[len] = '\0';
			return buffer.data();
		}
#endif

#ifdef __APPLE__
		uint32_t size = 0;
		_NSGetExecutablePath(nullptr, &size);

		std::vector<char> buffer(size);
		if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
			if (char *resolved = realpath(buffer.data(), nullptr)) {
				std::string result = resolved;
				free(resolved);
				return result;
			}
		}
#endif

		const std::filesystem::path def = std::filesystem::absolute(arg0);
		if (!std::filesystem::exists(def)) {
			error::panic("could not find executable path");
		}

		return def;
	}

	std::string path_abs(const std::filesystem::path &path)
	{
		return strings::replace(std::filesystem::absolute(path).string(), "//", "/");
	}

	void empty_path(const std::filesystem::path &path, const bool safe_check)
	{
		if (!std::filesystem::exists(path)) {
			std::filesystem::create_directories(path);
			return;
		}

		if (safe_check) {
			if (const int path_files = scan_path_files(path, 10000); path_files > 200) {
				logging::fatal(
					"Too many files and need to be deleted manually name={} files={}",
					path_abs(path),
					path_files
				);
			}
		}

		std::filesystem::remove_all(path);
		std::filesystem::create_directories(path);
	}

	bool path_equal(const std::filesystem::path &p1, const std::filesystem::path &p2, const bool clean_check)
	{
		if (p1.string() == p2.string()) {
			return true;
		}

		std::error_code ec;
		if (std::filesystem::equivalent(p1, p2, ec)) {
			return true; // 存在且相同
		}

		if (!ec) {
			return false; // 存在但不同
		}

		const bool check = std::filesystem::weakly_canonical(p1) == std::filesystem::weakly_canonical(p2);
		// 物理是否相同，和逻辑上是否相同
		if (!check && clean_check) {
			return clean_path(p1) == clean_path(p2);
		}

		return check;
	}
} // namespace filesystem
