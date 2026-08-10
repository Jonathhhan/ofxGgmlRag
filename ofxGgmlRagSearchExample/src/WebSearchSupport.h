#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace ragWebExample {
struct SearchHit { std::string title; std::string url; };
struct Limits {
	std::size_t maxSearchResults = 4;
	std::size_t maxPages = 4;
	std::size_t maxBytesPerPage = 512 * 1024;
	std::size_t maxTotalBytes = 2 * 1024 * 1024;
	std::size_t maxDepth = 0;
};

std::string urlEncode(const std::string & value);
std::string expandSearchUrl(const std::string & urlTemplate, const std::string & query);
std::vector<SearchHit> parseSearchHtml(const std::string & html, std::size_t maxResults);
bool withinBounds(std::size_t acceptedPages, std::size_t totalBytes, std::size_t depth,
	std::size_t nextBytes, const Limits & limits, std::string & reason);
}
