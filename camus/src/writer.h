#pragma once

#include <fstream>
#include <iostream>
#include <thread>

#include <build/build.h>

#include "common/error/error.h"
#include "common/net/net.h"
#include "common/str/str.h"
#include "config.h"
#include "httplib.h"

namespace camus
{
	class writer
	{
		std::filesystem::path work_dir_;
		bool has_template_;
		uint16_t watch_server_port_;
		std::string watch_server_addr_;
		httplib::Server watch_server_;

		std::atomic<bool> quit_ = false;

	  public:
		explicit writer(const std::filesystem::path &work_dir) :
			work_dir_(std::filesystem::absolute(work_dir)),
			has_template_(std::filesystem::exists(work_dir_ / CAMUS_DIR)),
			watch_server_addr_(net::get_local_addr()),
			watch_server_port_(3001)
		{
			if (!std::filesystem::exists(work_dir_)) {
				error::panic("work directory not exists name={}", work_dir_.string());
			}

			std::filesystem::current_path(work_dir_);
			logging::info("current work directory: {}", work_dir_.string());

			conf_loader::get().parse_yaml("camus.yaml");

			assert(!watch_server_addr_.empty());
		}

		void watch()
		{
			std::thread server_thread([&]() {
				watch_server_.set_mount_point("/", conf_loader::camus().output_dir);
				watch_server_.listen(watch_server_addr_, watch_server_port_);
			});

			logging::info("watch directory: {}", work_dir_.string());
			logging::info("watch server listen on http://{}:{}", watch_server_addr_, watch_server_port_);

			while (true) {
				if (quit_) {
					break;
				}

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

			watch_server_.stop();
			if (server_thread.joinable()) {
				server_thread.join();
			}
		}

		[[nodiscard]] int build() const;
	};
} // namespace camus
