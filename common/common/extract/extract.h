#pragma once

#include <iostream>
#include <string>
#include <vector>

#include <archive.h>
#include <archive_entry.h>

#include "common/filesystem/filesystem.h"

namespace extract
{
	static ssize_t copy_data(archive *ar, archive *aw)
	{
		const void *buff;
		size_t size;
		la_int64_t offset;

		while (true) {
			ssize_t r = archive_read_data_block(ar, &buff, &size, &offset);
			if (r == ARCHIVE_EOF) {
				return ARCHIVE_OK;
			}

			if (r < ARCHIVE_OK) {
				return r;
			}

			if (r = archive_write_data_block(aw, buff, size, offset); r < ARCHIVE_OK) {
				std::cerr << "archive_write_data_block() failed: " << archive_error_string(aw) << std::endl;
				{
					return r;
				}
			}
		}
	}

	static bool extract_tgz(const std::vector<char> &archive_data, const std::filesystem::path &destination_dir)
	{
		archive *a = archive_read_new();
		archive_read_support_format_all(a);
		archive_read_support_filter_all(a);
		archive *ext = archive_write_disk_new();
		archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
		archive_write_disk_set_standard_lookup(ext);

		// 如果文件流不正确，linux 下无法解压，macos 可能正常
		ssize_t r = archive_read_open_memory2(a, archive_data.data(), archive_data.size(), 1);
		if (r != ARCHIVE_OK) {
			std::cerr << "archive_read_open_memory() failed: " << archive_error_string(a) << std::endl;
			archive_read_free(a);
			archive_write_free(ext);
			return false;
		}

		for (;;) {
			archive_entry *entry;
			r = archive_read_next_header(a, &entry);
			if (r == ARCHIVE_EOF) {
				break;
			}

			if (r < ARCHIVE_OK) {
				std::cerr << "archive_read_next_header() warning: " << archive_error_string(a) << std::endl;
			}

			if (r < ARCHIVE_WARN) {
				goto cleanup;
			}

			// 4. 【关键】修改输出路径，指向目标目录
			const char *current_file = archive_entry_pathname(entry);
			std::string full_path = destination_dir / current_file;
			archive_entry_set_pathname(entry, full_path.c_str());

			// 5. 写入文件头
			r = archive_write_header(ext, entry);
			if (r < ARCHIVE_OK) {
				std::cerr << "archive_write_header() warning: " << archive_error_string(ext) << std::endl;
			} else if (archive_entry_size(entry) > 0) {
				// 6. 写入文件数据
				r = copy_data(a, ext);
				if (r < ARCHIVE_OK) {
					std::cerr << "copy_data() failed: " << archive_error_string(ext) << std::endl;
					goto cleanup;
				}
			}

			// 7. 完成当前条目的写入
			r = archive_write_finish_entry(ext);
			if (r < ARCHIVE_OK) {
				std::cerr << "archive_write_finish_entry() warning: " << archive_error_string(ext) << std::endl;
			}
		}

	cleanup:
		// 8. 清理资源
		archive_read_close(a);
		archive_read_free(a);
		archive_write_close(ext);
		archive_write_free(ext);
		return r == ARCHIVE_OK || r == ARCHIVE_EOF;
	}
} // namespace extract
