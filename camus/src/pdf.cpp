#include "pdf.h"

#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

namespace
{
	std::string read_binary_file(const std::filesystem::path &path)
	{
		std::ifstream stream(path, std::ios::binary);
		if (!stream) {
			return {};
		}

		std::ostringstream buffer;
		buffer << stream.rdbuf();
		return buffer.str();
	}

	std::string find_object(const std::string &data, const int number)
	{
		const std::string pattern = "\n" + std::to_string(number) + " 0 obj";
		const std::size_t begin = data.find(pattern);
		if (begin == std::string::npos) {
			return {};
		}

		const std::size_t start = begin + pattern.size();
		const std::size_t end = data.find("\nendobj", start);
		if (end == std::string::npos) {
			return {};
		}

		return data.substr(start, end - start);
	}

	std::vector<int> object_kids(const std::string &body)
	{
		std::vector<int> refs;

		std::smatch array;
		if (!std::regex_search(body, array, std::regex(R"(/Kids\s*\[([^\]]*)\])"))) {
			return refs;
		}

		const std::string list = array[1].str();
		const std::regex ref_pattern(R"((\d+)\s+0\s+R)");
		for (auto it = std::sregex_iterator(list.begin(), list.end(), ref_pattern); it != std::sregex_iterator();
			 ++it) {
			refs.push_back(std::stoi((*it)[1].str()));
		}

		return refs;
	}

	void collect_pages(const std::string &data, const int ref, std::vector<int> &pages, std::set<int> &seen)
	{
		if (!seen.insert(ref).second) {
			return;
		}

		const std::string body = find_object(data, ref);
		if (body.empty()) {
			return;
		}

		if (body.find("/Type /Page") != std::string::npos && body.find("/Type /Pages") == std::string::npos) {
			pages.push_back(ref);
			return;
		}

		for (const int kid : object_kids(body)) {
			collect_pages(data, kid, pages, seen);
		}
	}

	int first_int(const std::string &data, const std::string &pattern)
	{
		std::smatch match;
		if (std::regex_search(data, match, std::regex(pattern))) {
			return std::stoi(match[1].str());
		}

		return 0;
	}

	void collect_entries(
		const std::string &body,
		const std::map<int, int> &page_index,
		std::map<std::string, int> &result
	)
	{
		const std::regex entry_pattern(R"(/([^\s/\[\]<>()]+)\s*\[\s*(\d+)\s+0\s+R)");
		for (auto it = std::sregex_iterator(body.begin(), body.end(), entry_pattern);
			 it != std::sregex_iterator();
			 ++it) {
			const auto index = page_index.find(std::stoi((*it)[2].str()));
			if (index != page_index.end()) {
				result[(*it)[1].str()] = index->second;
			}
		}
	}
} // namespace

std::map<std::string, int> pdf::read_anchor_pages(const std::filesystem::path &path)
{
	std::map<std::string, int> result;

	const std::string data = read_binary_file(path);
	if (data.empty()) {
		return result;
	}

	const int pages_ref =
		first_int(data, R"(/Type\s*/Catalog[\s\S]*?/Pages\s+(\d+)\s+0\s+R)");
	if (pages_ref == 0) {
		return result;
	}

	std::vector<int> page_objects;
	std::set<int> seen;
	collect_pages(data, pages_ref, page_objects, seen);
	if (page_objects.empty()) {
		return result;
	}

	std::map<int, int> page_index;
	for (std::size_t i = 0; i < page_objects.size(); ++i) {
		page_index[page_objects[i]] = static_cast<int>(i) + 1;
	}

	const int dests_ref = first_int(data, R"(/Dests\s+(\d+)\s+0\s+R)");
	if (dests_ref != 0) {
		collect_entries(find_object(data, dests_ref), page_index, result);
	}

	if (result.empty()) {
		const std::regex name_entry_pattern(R"(\(([^)]*)\)\s*\[\s*(\d+)\s+0\s+R)");
		for (auto it = std::sregex_iterator(data.begin(), data.end(), name_entry_pattern);
			 it != std::sregex_iterator();
			 ++it) {
			const auto index = page_index.find(std::stoi((*it)[2].str()));
			if (index != page_index.end()) {
				result[(*it)[1].str()] = index->second;
			}
		}
	}

	return result;
}
