#pragma once

#include <fstream>
#include <iostream>
#include <thread>

#include <build/build.h>

#include "common/error/error.h"
#include "common/str/str.h"
#include "config.h"

namespace camus
{
	class writer
	{
		std::filesystem::path work_dir_;
		bool has_template_;

	  public:
		explicit writer(const std::filesystem::path &work_dir) :
			work_dir_(std::filesystem::absolute(work_dir)),
			has_template_(std::filesystem::exists(work_dir_ / CAMUS_DIR))
		{
			if (!std::filesystem::exists(work_dir_)) {
				error::panic("work directory not exists name={}", work_dir_.string());
			}

			std::filesystem::current_path(work_dir_);
			logging::info("current work directory: {}", work_dir_.string());

			conf_loader::get().parse_yaml("camus.yaml");
		}

		[[noreturn]] void watch() const
		{
			logging::info("watch directory: {}", work_dir_.string());

			while (true) {
				std::stringstream buffer;

				for (const auto &entry :
					 std::filesystem::recursive_directory_iterator(conf_loader::camus().source_dir)) {
					if (entry.path().extension() != ".md") {
						continue;
					}

					if (std::ifstream file(entry.path()); file.is_open()) {
						buffer << file.rdbuf();
						file.close();
					}
				}

				// 如果文件 hash 不同，就重新编译一遍
				static uint64_t current_hash = 0;
				if (const uint64_t hash = strings::make_hash(buffer.str()); current_hash != hash) {
					current_hash = hash;
					std::ignore = build();
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}

		[[nodiscard]] int build() const;
	};
} // namespace camus
