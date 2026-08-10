#include "WebSearchSupport.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>

namespace {
std::string decode(const std::string & value) {
	std::string out;
	for (std::size_t i = 0; i < value.size(); ++i) {
		if (value[i] == '%' && i + 2 < value.size() && std::isxdigit(static_cast<unsigned char>(value[i + 1])) && std::isxdigit(static_cast<unsigned char>(value[i + 2]))) {
			out.push_back(static_cast<char>(std::stoi(value.substr(i + 1, 2), nullptr, 16))); i += 2;
		} else if (value[i] == '+') out.push_back(' '); else out.push_back(value[i]);
	}
	return out;
}

std::string stripTags(const std::string & value) {
	return std::regex_replace(value, std::regex("<[^>]*>"), "");
}

std::string resultUrl(std::string href) {
	href = std::regex_replace(href, std::regex("&amp;"), "&");
	auto marker = href.find("uddg=");
	if (marker != std::string::npos) {
		auto encoded = href.substr(marker + 5);
		auto end = encoded.find('&');
		if (end != std::string::npos) encoded.resize(end);
		href = decode(encoded);
	}
	return href;
}
}

namespace ragWebExample {
std::string urlEncode(const std::string & value) {
	std::ostringstream out;
	out << std::uppercase << std::hex;
	for (const unsigned char ch : value) {
		if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') out << ch;
		else out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
	}
	return out.str();
}

std::string expandSearchUrl(const std::string & urlTemplate, const std::string & query) {
	auto result = urlTemplate;
	auto position = result.find("{query}");
	if (position != std::string::npos) result.replace(position, 7, urlEncode(query));
	return result;
}

std::vector<SearchHit> parseSearchHtml(const std::string & html, std::size_t maxResults) {
	std::vector<SearchHit> hits;
	std::set<std::string> seen;
	const std::regex anchor("<a[^>]+href=[\\\"']([^\\\"']+)[\\\"'][^>]*>([\\s\\S]*?)</a>", std::regex::icase);
	for (auto it = std::sregex_iterator(html.begin(), html.end(), anchor); it != std::sregex_iterator() && hits.size() < maxResults; ++it) {
		auto url = resultUrl((*it)[1].str());
		if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) continue;
		if (url.find("duckduckgo.com") != std::string::npos) continue;
		if (!seen.insert(url).second) continue;
		hits.push_back({ stripTags((*it)[2].str()), url });
	}
	return hits;
}

bool withinBounds(std::size_t acceptedPages, std::size_t totalBytes, std::size_t depth,
	std::size_t nextBytes, const Limits & limits, std::string & reason) {
	if (acceptedPages >= limits.maxPages) reason = "page limit";
	else if (depth > limits.maxDepth) reason = "depth limit";
	else if (nextBytes > limits.maxBytesPerPage) reason = "per-page byte limit";
	else if (nextBytes > limits.maxTotalBytes - std::min(totalBytes, limits.maxTotalBytes)) reason = "total byte limit";
	else { reason.clear(); return true; }
	return false;
}
}
