#include "update.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

#include <build/build.h>
#include <httplib.h>
#include <unistd.h>

#include "common/logging/logging.h"
#include "common/str/str.h"

namespace
{
	constexpr const char *update_host = "https://github.com";
	constexpr const char *github_prefix = "https://github.com/";

	struct release_check {
		bool has_update = false;
		std::string version;
		std::string download_url;
	};

	// GIT_REPO 形如 https://github.com/<owner>/<repo>，去掉域名后得到 <owner>/<repo>。
	std::string repo_path()
	{
		std::string repo = GIT_REPO;
		if (repo.starts_with(github_prefix)) {
			repo = repo.substr(std::string_view(github_prefix).size());
		}

		return repo;
	}

	std::vector<int> parse_version(std::string version)
	{
		version = strings::trim_space(version);
		if (!version.empty() && (version.front() == 'v' || version.front() == 'V')) {
			version.erase(version.begin());
		}

		std::vector<int> parts;
		for (const std::string &piece : strings::split(version, '.')) {
			try {
				parts.push_back(std::stoi(piece));
			} catch (...) {
				parts.push_back(0);
			}
		}

		return parts;
	}

	int compare_version(const std::string &left, const std::string &right)
	{
		const std::vector<int> a = parse_version(left);
		const std::vector<int> b = parse_version(right);
		const std::size_t size = std::max(a.size(), b.size());
		for (std::size_t i = 0; i < size; ++i) {
			const int av = i < a.size() ? a[i] : 0;
			const int bv = i < b.size() ? b[i] : 0;
			if (av != bv) {
				return av < bv ? -1 : 1;
			}
		}

		return 0;
	}

	std::string asset_url(const nlohmann::json &asset, const std::string &repo, const std::string &tag)
	{
		if (const std::string url = asset.value("url", ""); !url.empty()) {
			return url;
		}

		const std::string name = asset.value("name", "");
		if (name.empty() || tag.empty()) {
			return {};
		}

		return "https://github.com/" + repo + "/releases/download/" + tag + "/" + name;
	}

	std::string select_download_url(const nlohmann::json &assets, const std::string &repo, const std::string &tag)
	{
		if (!assets.is_array() || assets.empty()) {
			return {};
		}

#if defined(__APPLE__)
		const std::vector<std::string> extensions = {".dmg", ".pkg"};
#elif defined(__linux__)
		const std::vector<std::string> extensions = {".deb", ".rpm"};
#else
		const std::vector<std::string> extensions = {};
#endif

		for (const auto &extension : extensions) {
			for (const auto &asset : assets) {
				const std::string name = asset.value("name", "");
				if (name.ends_with(extension)) {
					return asset_url(asset, repo, tag);
				}
			}
		}

		return asset_url(assets.front(), repo, tag);
	}

	bool query_latest(release_check &result)
	{
		const std::string repo = repo_path();
		const std::string path = "/" + repo + "/releases/latest/download/latest.json";
		const std::string request_url = std::string(update_host) + path;

		httplib::Client client(update_host);
		client.set_connection_timeout(10);
		client.set_read_timeout(15);
		client.set_follow_location(true);
		client.set_default_headers({
			{"Accept", "application/json"},
			{"User-Agent", "camus"},
		});

		logging::info("checking for updates (current v{})", PROJECT_VERSION);

		const auto response = client.Get(path);
		if (!response) {
			logging::warn(
				"update check failed: {} (error={}) url={}",
				httplib::to_string(response.error()),
				static_cast<int>(response.error()),
				request_url
			);
			return false;
		}

		if (response->status != 200) {
			logging::warn("update check failed: status={}", response->status);
			if (!response->body.empty()) {
				logging::warn("response: {}", response->body);
			}
			return false;
		}

		const nlohmann::json release = nlohmann::json::parse(response->body, nullptr, false);
		if (release.is_discarded() || !release.is_object()) {
			logging::warn("update check failed: invalid release metadata");
			if (!response->body.empty()) {
				logging::warn("response: {}", response->body);
			}
			return false;
		}

		const std::string tag = release.value("tag", "");
		const std::string version = release.value("version", tag);
		if (version.empty()) {
			logging::warn("update check failed: release version not found");
			return false;
		}

		if (compare_version(version, PROJECT_VERSION) <= 0) {
			logging::info("already up to date: v{} (latest: {})", PROJECT_VERSION, version);
			return true;
		}

		logging::info("new version available: {} (current v{})", version, PROJECT_VERSION);

		const std::string tag_for_url = tag.empty() ? "v" + version : tag;
		result.has_update = true;
		result.version = version;
		result.download_url = select_download_url(release.value("assets", nlohmann::json::array()), repo, tag_for_url);
		return true;
	}

	std::string shell_quote(const std::string &value)
	{
		return "'" + strings::replace(value, "'", "'\\''") + "'";
	}

	bool split_url(const std::string &url, std::string &origin, std::string &path)
	{
		const auto scheme_end = url.find("://");
		if (scheme_end == std::string::npos) {
			return false;
		}

		const auto host_start = scheme_end + 3;
		const auto path_start = url.find('/', host_start);
		origin = url.substr(0, path_start);
		path = path_start == std::string::npos ? "/" : url.substr(path_start);
		return !origin.empty();
	}

	std::string url_file_name(const std::string &url)
	{
		std::string value = url;
		if (const auto pos = value.find_first_of("?#"); pos != std::string::npos) {
			value = value.substr(0, pos);
		}

		const std::string name = std::filesystem::path(value).filename().string();
		return name.empty() ? "camus-update" : name;
	}

	std::string human_size(const std::size_t bytes)
	{
		constexpr double unit = 1024.0;
		if (static_cast<double>(bytes) >= unit * unit) {
			return std::format("{:.1f} MB", static_cast<double>(bytes) / (unit * unit));
		}

		if (static_cast<double>(bytes) >= unit) {
			return std::format("{:.1f} KB", static_cast<double>(bytes) / unit);
		}

		return std::format("{} B", bytes);
	}

	bool download_file(const std::string &url, const std::filesystem::path &dest)
	{
		std::string origin;
		std::string path;
		if (!split_url(url, origin, path)) {
			logging::error("update download failed: invalid url={}", url);
			return false;
		}

		httplib::Client client(origin);
		client.set_connection_timeout(10);
		client.set_read_timeout(300);
		client.set_follow_location(true);
		client.set_default_headers({
			{"Accept", "application/octet-stream"},
			{"User-Agent", "camus"},
		});

		std::ofstream out(dest, std::ios::binary | std::ios::trunc);
		if (!out) {
			logging::error("update download failed: cannot open file={}", dest.string());
			return false;
		}

		logging::info("downloading: {}", url);

		const bool terminal = isatty(fileno(stdout)) != 0;
		std::size_t last_percent = 101;
		std::size_t last_report = 0;

		auto progress = [&](const std::size_t current, const std::size_t total) -> bool {
			if (total > 0) {
				const std::size_t percent = current * 100 / total;
				if (percent == last_percent) {
					return true;
				}

				last_percent = percent;
				if (terminal) {
					logging::print_log_inline(
						logging::INFO_LEVEL,
						"UPDT",
						std::format("{} / {} ({:3}%)", human_size(current), human_size(total), percent)
					);
				} else if (percent >= last_report + 10) {
					last_report = percent;
					logging::info_tag("UPDT", "download progress: {}%", percent);
				}

				return true;
			}

			if (current - last_report >= 1024 * 1024) {
				last_report = current;
				if (terminal) {
					logging::print_log_inline(logging::INFO_LEVEL, "UPDT", human_size(current));
				}
			}

			return true;
		};

		const auto response = client.Get(
			path,
			[&](const char *data, const std::size_t length) -> bool {
				out.write(data, static_cast<std::streamsize>(length));
				return static_cast<bool>(out);
			},
			progress
		);

		if (terminal) {
			std::cout << "\n";
		}

		out.close();

		if (!response) {
			logging::error(
				"update download failed: {} (error={}) url={}",
				httplib::to_string(response.error()),
				static_cast<int>(response.error()),
				url
			);
			std::filesystem::remove(dest);
			return false;
		}

		if (response->status != 200) {
			logging::error("update download failed: status={} url={}", response->status, url);
			std::filesystem::remove(dest);
			return false;
		}

		if (!out) {
			logging::error("update download failed: write error file={}", dest.string());
			std::filesystem::remove(dest);
			return false;
		}

		logging::info_tag("UPDT", "downloaded: {} ({})", dest.string(), human_size(std::filesystem::file_size(dest)));
		return true;
	}

	std::string install_command(const std::filesystem::path &file)
	{
#if defined(__APPLE__)
		return "sudo installer -pkg " + shell_quote(file.string()) + " -target /";
#elif defined(__linux__)
		const std::string name = file.filename().string();
		if (name.ends_with(".deb")) {
			return "sudo dpkg -i " + shell_quote(file.string());
		}

		if (name.ends_with(".rpm")) {
			return "sudo rpm -Uvh " + shell_quote(file.string());
		}

		return {};
#else
		return {};
#endif
	}
} // namespace

namespace camus
{
	int check_update()
	{
		release_check result;
		if (!query_latest(result)) {
			return 1;
		}

		if (!result.has_update) {
			return 0;
		}

		if (!result.download_url.empty()) {
			logging::info("download: curl -LO {}", result.download_url);
		} else {
			logging::info("download: {}/releases/latest", GIT_REPO);
		}

		return 0;
	}

	int run_update()
	{
		release_check result;
		if (!query_latest(result)) {
			return 1;
		}

		if (!result.has_update) {
			return 0;
		}

		if (result.download_url.empty()) {
			logging::error("update failed: no downloadable asset found for {}", result.version);
			return 1;
		}

		const std::filesystem::path dest = std::filesystem::temp_directory_path() / url_file_name(result.download_url);
		if (!download_file(result.download_url, dest)) {
			return 1;
		}

		const std::string command = install_command(dest);
		if (command.empty()) {
			logging::error("update downloaded but automatic install is not supported: {}", dest.string());
			return 1;
		}

		logging::info("install: {}", command);
		const int status = std::system(command.c_str());
		if (status != 0) {
			logging::error("update install failed: status={} file={}", status, dest.string());
			return 1;
		}

		std::error_code ec;
		std::filesystem::remove(dest, ec);
		logging::info("update installed: {}", result.version);
		return 0;
	}
} // namespace camus
