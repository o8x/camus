#include "filesystem.h"

#include "common/functions/functions.h"
#include "common/logging/logging.h"

#include <fstream>
#include <sstream>

namespace filesystem
{
	int write_file(const std::string_view &path, const std::string_view &content)
	{
		std::ofstream outfile;
		if (outfile.open(std::string{path}, std::ofstream::out | std::ofstream::trunc); !outfile) {
			return 1;
		}

		outfile << content.data();
		outfile.close();
		return 0;
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
		return trim ? functions::trim_space(res) : res;
	}

	std::string clean_path(const std::string &path, const std::string &prefix)
	{
		const std::filesystem::path p(std::format("{}/{}", prefix, path));
		return p.lexically_normal();
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
		logging::info("enter path session={} name={}", session_id, std::filesystem::current_path().string());
		fn(curr);
		std::filesystem::current_path(curr);
		logging::info("back path session={} name={}", session_id, curr.string());

		return curr;
	}
} // namespace filesystem
