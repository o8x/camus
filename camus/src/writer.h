#pragma once

#include "common/error/error.h"
#include "config.h"

namespace camus
{
	class writer
	{
		std::filesystem::path work_dir_;

	  public:
		explicit writer(const std::filesystem::path &work_dir) : work_dir_(std::filesystem::absolute(work_dir))
		{
			if (!std::filesystem::exists(work_dir_)) {
				error::panic("work directory not exists name={}", work_dir_.string());
			}

			logging::info("current work directory: {}", work_dir_.string());
		}

		[[nodiscard]] int generate() const;
	};
} // namespace camus
