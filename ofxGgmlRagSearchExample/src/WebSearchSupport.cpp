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

std::string decodeHtml(std::string value) {
	const std::vector<std::pair<std::string, std::string>> entities = {
		{ "&#x27;", "'" }, { "&#39;", "'" }, { "&quot;", "\"" }, { "&amp;", "&" },
		{ "&lt;", "<" }, { "&gt;", ">" }, { "&nbsp;", " " }
	};
	for (const auto & entity : entities) {
		std::size_t position = 0;
		while ((position = value.find(entity.first, position)) != std::string::npos) {
			value.replace(position, entity.first.size(), entity.second); position += entity.second.size();
		}
	}
	return value;
}

std::string absoluteUrl(const std::string & sourceUrl, const std::string & href) {
	if (href.rfind("http://", 0) == 0 || href.rfind("https://", 0) == 0) return href;
	auto scheme = sourceUrl.find("://"); if (scheme == std::string::npos) return "";
	auto rootEnd = sourceUrl.find('/', scheme + 3); const auto root = rootEnd == std::string::npos ? sourceUrl : sourceUrl.substr(0, rootEnd);
	return href.empty() ? std::string() : (href[0] == '/' ? root + href : root + "/" + href);
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

std::string quoteSearchQuery(const std::string & person) {
	auto cleaned = person;
	cleaned.erase(cleaned.begin(), std::find_if(cleaned.begin(), cleaned.end(), [](unsigned char ch) { return !std::isspace(ch); }));
	cleaned.erase(std::find_if(cleaned.rbegin(), cleaned.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), cleaned.end());
	return cleaned.empty() ? std::string() : "\"" + cleaned + "\" quotes quotations";
}

std::string localModelAlias(const std::string & modelPath) {
	const auto slash = modelPath.find_last_of("/\\");
	auto name = slash == std::string::npos ? modelPath : modelPath.substr(slash + 1);
	const auto dot = name.find_last_of('.');
	if (dot != std::string::npos) name.resize(dot);
	std::string slug;
	for (const unsigned char ch : name) {
		if (std::isalnum(ch) || ch == '.' || ch == '_' || ch == '-') slug.push_back(static_cast<char>(ch));
		else if (!slug.empty() && slug.back() != '-') slug.push_back('-');
	}
	while (!slug.empty() && slug.front() == '-') slug.erase(slug.begin());
	while (!slug.empty() && slug.back() == '-') slug.pop_back();
	return slug.empty() ? std::string() : "local/" + slug;
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

std::vector<QuoteHit> extractStructuredQuotes(const std::string & sourceUrl, const std::string & html,
	const std::string & person, std::size_t maxQuotes) {
	std::vector<QuoteHit> quotes;
	std::set<std::string> seen;
	const std::regex anchor("<a([^>]*)>([\\s\\S]*?)</a>", std::regex::icase);
	const std::regex hrefExpression("href=[\\\"']([^\\\"']+)[\\\"']", std::regex::icase);
	for (auto it = std::sregex_iterator(html.begin(), html.end(), anchor); it != std::sregex_iterator() && quotes.size() < maxQuotes; ++it) {
		const auto attributes = (*it)[1].str();
		const bool brainyQuote = std::regex_search(attributes, std::regex("class=[\\\"'][^\\\"']*\\bb-qt\\b", std::regex::icase));
		const bool attributedTitle = std::regex_search(attributes, std::regex("class=[\\\"'][^\\\"']*\\btitle\\b", std::regex::icase)) &&
			attributes.find("data-author=\"" + person + "\"") != std::string::npos;
		if (!brainyQuote && !attributedTitle) continue;
		std::smatch hrefMatch; if (!std::regex_search(attributes, hrefMatch, hrefExpression)) continue;
		auto text = decodeHtml(stripTags((*it)[2].str()));
		text = std::regex_replace(text, std::regex("\\s+"), " ");
		if (text.size() < 12 || !seen.insert(text).second) continue;
		auto url = absoluteUrl(sourceUrl, decodeHtml(hrefMatch[1].str())); if (url.empty()) continue;
		quotes.push_back({ text, url });
	}
	return quotes;
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
