#include "filesystem.h"

#include "common/functions/functions.h"

#include <fstream>
#include <sstream>

namespace filesystem
{
	int write_file(const std::string_view &path, const std::string_view &content)
	{
		std::ofstream outfile;
		if (outfile.open(std::string{path}, std::ofstream::in | std::ofstream::out | std::ofstream::trunc); !outfile) {
			return 1;
		}

		outfile << content.data();
		outfile.close();
		return 0;
	}
} // namespace filesystem
