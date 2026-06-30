#include "writer.h"

int main(const int argc, char **argv)
{
	std::string work_dir = "./blog";
	if (argc == 2) {
		work_dir = argv[1];
	}

	try {
		const auto c = std::make_unique<camus::writer>(work_dir);
		return c->generate();
	} catch (const std::exception &e) {
		logging::error("error: {}", e.what());
	}

	return 1;
}
