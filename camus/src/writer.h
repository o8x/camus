#pragma once

#include "config.h"
#include "common/error/error.h"

namespace camus
{
	class writer
	{
		std::filesystem::path work_dir_;

	  public:
		explicit writer(std::filesystem::path work_dir) : work_dir_(std::move(work_dir))
		{
			if (!std::filesystem::exists(work_dir_)) {
				error::panic("work directory not exists name={}", work_dir_.string());
			}
		}

		[[nodiscard]] int generate() const;
	};
} // namespace camus
