#include "pdf.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
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

	struct xref_info {
		std::map<int, std::size_t> offsets;
		std::size_t xref_offset = 0;
		int size = 0;
		int info_ref = 0;
	};

	bool parse_classic_xref(const std::string &data, xref_info &info)
	{
		const std::size_t marker = data.rfind("startxref");
		if (marker == std::string::npos) {
			return false;
		}

		std::istringstream tail(data.substr(marker + 9));
		if (!(tail >> info.xref_offset) || info.xref_offset >= data.size() ||
			data.compare(info.xref_offset, 4, "xref") != 0) {
			return false;
		}

		const std::size_t xref_offset = info.xref_offset;

		std::istringstream stream(data.substr(xref_offset + 4));
		std::string token;
		while (stream >> token) {
			if (token == "trailer") {
				break;
			}

			int start = 0;
			int count = 0;
			if (!(std::istringstream(token) >> start) || !(stream >> count)) {
				return false;
			}

			for (int i = 0; i < count; ++i) {
				std::string offset;
				std::string generation;
				std::string type;
				if (!(stream >> offset >> generation >> type)) {
					return false;
				}

				if (type == "n") {
					info.offsets[start + i] = static_cast<std::size_t>(std::stoull(offset));
				}
			}
		}

		if (info.offsets.empty()) {
			return false;
		}

		const std::size_t trailer_pos = data.find("trailer", xref_offset);
		if (trailer_pos == std::string::npos) {
			return false;
		}

		const std::string region = data.substr(trailer_pos);
		std::smatch match;
		if (std::regex_search(region, match, std::regex(R"(/Size\s+(\d+))"))) {
			info.size = std::stoi(match[1].str());
		}

		if (std::regex_search(region, match, std::regex(R"(/Info\s+(\d+)\s+0\s+R)"))) {
			info.info_ref = std::stoi(match[1].str());
		}

		return info.size > 0;
	}

	std::string raw_object(const std::string &data, const std::size_t offset, const std::size_t limit)
	{
		if (offset >= data.size() || offset >= limit) {
			return {};
		}

		std::size_t search = offset;
		std::size_t end = std::string::npos;
		while (true) {
			const std::size_t found = data.find("endobj", search);
			if (found == std::string::npos || found >= limit) {
				break;
			}

			end = found;
			search = found + 6;
		}

		if (end == std::string::npos) {
			return {};
		}

		return data.substr(offset, end + 6 - offset);
	}

	std::string object_body(const std::string &raw)
	{
		const std::size_t begin = raw.find("obj");
		const std::size_t end = raw.rfind("endobj");
		if (begin == std::string::npos || end == std::string::npos || end <= begin) {
			return {};
		}

		return raw.substr(begin + 3, end - begin - 3);
	}

	void append_hex(std::string &output, const std::uint32_t value)
	{
		constexpr char digits[] = "0123456789ABCDEF";
		output.push_back(digits[(value >> 12) & 0xF]);
		output.push_back(digits[(value >> 8) & 0xF]);
		output.push_back(digits[(value >> 4) & 0xF]);
		output.push_back(digits[value & 0xF]);
	}

	std::string pdf_title(const std::string &text)
	{
		std::string output = "<FEFF";
		std::size_t i = 0;
		while (i < text.size()) {
			const auto byte = static_cast<unsigned char>(text[i]);
			std::uint32_t code = 0;
			std::size_t length = 0;

			if (byte < 0x80) {
				code = byte;
				length = 1;
			} else if ((byte & 0xE0) == 0xC0) {
				code = byte & 0x1F;
				length = 2;
			} else if ((byte & 0xF0) == 0xE0) {
				code = byte & 0x0F;
				length = 3;
			} else if ((byte & 0xF8) == 0xF0) {
				code = byte & 0x07;
				length = 4;
			} else {
				code = 0xFFFD;
				length = 1;
			}

			if (length > 1) {
				if (i + length > text.size()) {
					code = 0xFFFD;
					length = 1;
				} else {
					for (std::size_t k = 1; k < length; ++k) {
						const auto next = static_cast<unsigned char>(text[i + k]);
						if ((next & 0xC0) != 0x80) {
							code = 0xFFFD;
							length = 1;
							break;
						}

						code = (code << 6) | (next & 0x3F);
					}
				}
			}

			i += length;
			if (code > 0x10FFFF) {
				code = 0xFFFD;
			}

			if (code > 0xFFFF) {
				code -= 0x10000;
				append_hex(output, 0xD800 + (code >> 10));
				append_hex(output, 0xDC00 + (code & 0x3FF));
			} else {
				append_hex(output, code);
			}
		}

		output += ">";
		return output;
	}

	std::string pdf_name(const std::string &name)
	{
		constexpr char hex[] = "0123456789ABCDEF";
		std::string output;
		for (const unsigned char c : name) {
			const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
							  c == '-' || c == '_' || c == '.' || c == '+';
			if (safe) {
				output.push_back(static_cast<char>(c));
			} else {
				output.push_back('#');
				output.push_back(hex[(c >> 4) & 0xF]);
				output.push_back(hex[c & 0xF]);
			}
		}

		return output;
	}

	struct outline_node {
		std::string title;
		std::string anchor;
		int depth = 0;
		int parent = -1;
		int object = 0;
		int count = 0;
		std::vector<int> children;
	};

	int count_descendants(const std::vector<outline_node> &nodes, const int index)
	{
		int total = 0;
		for (const int child : nodes[index].children) {
			total += 1 + count_descendants(nodes, child);
		}

		return total;
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

bool pdf::write_outline(const std::filesystem::path &path, const std::vector<outline_entry> &entries)
{
	if (entries.empty()) {
		return true;
	}

	const std::string data = read_binary_file(path);
	if (data.empty()) {
		return false;
	}

	xref_info info;
	if (!parse_classic_xref(data, info)) {
		return false;
	}

	std::vector<std::pair<std::size_t, int>> ordered;
	ordered.reserve(info.offsets.size());
	for (const auto &[number, offset] : info.offsets) {
		ordered.emplace_back(offset, number);
	}

	std::ranges::sort(ordered, [](const auto &a, const auto &b) { return a.first < b.first; });

	std::map<int, std::string> objects;
	int catalog = 0;
	for (std::size_t i = 0; i < ordered.size(); ++i) {
		const auto &[offset, number] = ordered[i];
		const std::size_t limit = i + 1 < ordered.size() ? ordered[i + 1].first : info.xref_offset;
		const std::string raw = raw_object(data, offset, limit);
		if (raw.empty()) {
			continue;
		}

		objects[number] = raw;

		if (catalog == 0 && std::regex_search(object_body(raw), std::regex(R"(/Type\s*/Catalog)"))) {
			catalog = number;
		}
	}

	if (catalog == 0 || !objects.contains(catalog)) {
		return false;
	}

	std::vector<outline_node> nodes;
	nodes.reserve(entries.size());
	std::vector<int> stack;

	for (const auto &entry : entries) {
		while (!stack.empty() && nodes[stack.back()].depth >= entry.depth) {
			stack.pop_back();
		}

		outline_node node{
			.title = entry.title,
			.anchor = entry.anchor,
			.depth = entry.depth,
			.parent = stack.empty() ? -1 : stack.back(),
		};

		const int index = static_cast<int>(nodes.size());
		nodes.push_back(std::move(node));
		if (nodes[index].parent >= 0) {
			nodes[nodes[index].parent].children.push_back(index);
		}

		stack.push_back(index);
	}

	if (nodes.empty()) {
		return false;
	}

	int last_object = info.size - 1;
	if (!info.offsets.empty()) {
		last_object = std::max(last_object, info.offsets.rbegin()->first);
	}

	int next_object = last_object + 1;
	const int root_object = next_object++;

	for (auto &node : nodes) {
		node.object = next_object++;
		node.count = count_descendants(nodes, static_cast<int>(&node - nodes.data()));
	}

	std::vector<int> roots;
	for (std::size_t i = 0; i < nodes.size(); ++i) {
		if (nodes[i].parent < 0) {
			roots.push_back(static_cast<int>(i));
		}
	}

	if (roots.empty()) {
		return false;
	}

	std::map<int, std::string> generated;

	{
		std::ostringstream body;
		body << "<< /Type /Outlines /First " << nodes[roots.front()].object << " 0 R /Last "
			 << nodes[roots.back()].object << " 0 R /Count " << nodes.size() << " >>";
		generated[root_object] = body.str();
	}

	for (std::size_t i = 0; i < nodes.size(); ++i) {
		const outline_node &node = nodes[i];
		const int parent_object = node.parent < 0 ? root_object : nodes[node.parent].object;

		std::ostringstream body;
		body << "<< /Title " << pdf_title(node.title) << " /Parent " << parent_object << " 0 R";

		if (!node.anchor.empty()) {
			body << " /Dest /" << pdf_name(node.anchor);
		}

		if (!node.children.empty()) {
			body << " /First " << nodes[node.children.front()].object << " 0 R /Last "
				 << nodes[node.children.back()].object << " 0 R /Count " << node.count;
		}

		if (node.parent >= 0) {
			const auto &siblings = nodes[node.parent].children;
			const auto position = std::ranges::find(siblings, static_cast<int>(i));
			if (position != siblings.begin()) {
				body << " /Prev " << nodes[*(position - 1)].object << " 0 R";
			}

			if (position + 1 != siblings.end()) {
				body << " /Next " << nodes[*(position + 1)].object << " 0 R";
			}
		}

		body << " >>";
		generated[node.object] = body.str();
	}

	std::string &catalog_raw = objects[catalog];
	if (catalog_raw.find("/Outlines") == std::string::npos) {
		const std::size_t close = catalog_raw.rfind(">>");
		if (close == std::string::npos) {
			return false;
		}

		catalog_raw.insert(close, "/Outlines " + std::to_string(root_object) + " 0 R /PageMode /UseOutlines\n");
	}

	std::string output = data.substr(0, ordered.front().first);
	std::map<int, std::size_t> new_offsets;

	for (const auto &[offset, number] : ordered) {
		const auto found = objects.find(number);
		if (found == objects.end()) {
			continue;
		}

		new_offsets[number] = output.size();
		output += found->second;
		if (!output.ends_with('\n')) {
			output += '\n';
		}
	}

	for (const auto &[number, body] : generated) {
		new_offsets[number] = output.size();
		output += std::to_string(number) + " 0 obj\n" + body + "\nendobj\n";
	}

	const std::size_t xref_offset = output.size();
	const int final_size = next_object;

	output += "xref\n0 " + std::to_string(final_size) + "\n";
	output += "0000000000 65535 f \n";

	for (int number = 1; number < final_size; ++number) {
		const auto found = new_offsets.find(number);
		if (found == new_offsets.end()) {
			output += "0000000000 65535 f \n";
			continue;
		}

		std::ostringstream entry;
		entry << std::setw(10) << std::setfill('0') << found->second << " 00000 n \n";
		output += entry.str();
	}

	output += "trailer\n<< /Size " + std::to_string(final_size) + " /Root " + std::to_string(catalog) + " 0 R";
	if (info.info_ref != 0) {
		output += " /Info " + std::to_string(info.info_ref) + " 0 R";
	}

	output += " >>\nstartxref\n" + std::to_string(xref_offset) + "\n%%EOF\n";

	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream) {
		return false;
	}

	stream.write(output.data(), static_cast<std::streamsize>(output.size()));
	return stream.good();
}
