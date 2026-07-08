#include "filesystem.h"

#include <fstream>
#include <sstream>

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
			return "Could not open file: " + name;
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
		return p.lexically_normal();
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

	std::filesystem::path with_current_dir(
		const std::filesystem::path &path, const std::function<void(const std::filesystem::path &path)> &fn
	)
	{
		const std::filesystem::path curr = std::filesystem::current_path();
		if (curr == std::filesystem::absolute(path)) {
			fn(curr);
			return curr;
		}

		std::filesystem::create_directory(path);
		std::filesystem::current_path(path);

		const uint32_t session_id = functions::rand_int(1000, 9999);
		logging::debug("enter path session={} name={}", session_id, std::filesystem::current_path().string());
		fn(curr);
		std::filesystem::current_path(curr);
		logging::debug("back path session={} name={}", session_id, curr.string());

		return curr;
	}
} // namespace filesystem
