#include "update.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <build/build.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "common/str/str.h"

namespace
{
	constexpr const char *update_host = "https://github.com";

	// GIT_REPO 形如 https://github.com/<owner>/<repo>，去掉域名后得到 <owner>/<repo>。
	std::string repo_path()
	{
		std::string repo = GIT_REPO;
		constexpr const char *github_prefix = "https://github.com/";
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

	std::string select_download_url(
		const nlohmann::json &assets,
		const std::string &repo,
		const std::string &tag
	)
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
} // namespace

namespace camus
{
	int check_update()
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

		std::printf("checking latest release: GET %s (current v%s)\n", request_url.c_str(), PROJECT_VERSION);
		std::fflush(stdout);

		const auto response = client.Get(path);
		if (!response) {
			std::fprintf(
				stderr,
				"failed to fetch latest release: %s (error=%d) url=%s\n",
				httplib::to_string(response.error()).c_str(),
				static_cast<int>(response.error()),
				request_url.c_str()
			);
			return 1;
		}

		std::printf(
			"response: status=%d content-type=%s\n",
			response->status,
			response->get_header_value("Content-Type").c_str()
		);
		std::fflush(stdout);
		if (response->status != 200) {
			std::fprintf(stderr, "release metadata error: status=%d url=%s\n", response->status, request_url.c_str());
			if (!response->body.empty()) {
				std::fprintf(stderr, "response body: %s\n", response->body.c_str());
			}
			return 1;
		}

		const nlohmann::json release = nlohmann::json::parse(response->body, nullptr, false);
		if (release.is_discarded() || !release.is_object()) {
			std::fprintf(stderr, "failed to parse release metadata url=%s\n", request_url.c_str());
			if (!response->body.empty()) {
				std::fprintf(stderr, "response body: %s\n", response->body.c_str());
			}
			return 1;
		}

		const std::string tag = release.value("tag", "");
		const std::string version = release.value("version", tag);
		if (version.empty()) {
			std::fprintf(stderr, "release version not found url=%s\n", request_url.c_str());
			return 1;
		}

		std::printf(
			"latest release: version=%s tag=%s published=%s\n",
			version.c_str(),
			tag.c_str(),
			release.value("published_at", "").c_str()
		);

		if (compare_version(version, PROJECT_VERSION) <= 0) {
			std::printf("already up to date: v%s (latest: %s)\n", PROJECT_VERSION, version.c_str());
			return 0;
		}

		std::printf("new version available: %s (current v%s)\n", version.c_str(), PROJECT_VERSION);

		const std::string tag_for_url = tag.empty() ? "v" + version : tag;
		const std::string download_url =
			select_download_url(release.value("assets", nlohmann::json::array()), repo, tag_for_url);
		if (!download_url.empty()) {
			std::printf("download: curl -LO %s\n", download_url.c_str());
		} else {
			std::printf("download: %s/releases/latest\n", GIT_REPO);
		}

		return 0;
	}
} // namespace camus
