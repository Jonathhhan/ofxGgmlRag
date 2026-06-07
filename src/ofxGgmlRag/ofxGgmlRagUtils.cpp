#include "ofxGgmlRagUtils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace ofxGgmlRagUtils {
	namespace {
		namespace fs = std::filesystem;

		bool IsWhitespace(char value) {
			return std::isspace(static_cast<unsigned char>(value)) != 0;
		}

		bool IsTokenChar(char value) {
			return std::isalnum(static_cast<unsigned char>(value)) != 0 || value == '_';
		}

		char ToLowerAscii(char value) {
			return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
		}

		std::string NormalizeSourcePath(const std::string & value) {
			auto normalized = trim(value);
			std::replace(normalized.begin(), normalized.end(), '\\', '/');
			while (!normalized.empty() && normalized.back() == '/') {
				normalized.pop_back();
			}
			return normalized;
		}

		std::string PathToSource(const fs::path & path) {
			return NormalizeSourcePath(path.lexically_normal().generic_string());
		}

		std::string LowerAscii(const std::string & value) {
			std::string lowered;
			lowered.reserve(value.size());
			for (const auto ch : value) {
				lowered.push_back(ToLowerAscii(ch));
			}
			return lowered;
		}

		const std::unordered_set<std::string> & StopWords() {
			static const std::unordered_set<std::string> words = {
				"a", "an", "the", "is", "it", "in", "on", "at", "to", "of",
				"and", "or", "for", "with", "as", "by", "be", "are", "was",
				"that", "this", "which", "from", "not", "but", "so", "if",
				"what", "where", "when", "why", "how"
			};
			return words;
		}

		std::vector<std::string> ScoringTerms(const std::string & text, bool ignoreStopWords) {
			auto terms = tokenize(text);
			if (!ignoreStopWords) {
				return terms;
			}
			std::vector<std::string> filtered;
			for (const auto & term : terms) {
				if (StopWords().find(term) == StopWords().end()) {
					filtered.push_back(term);
				}
			}
			return filtered.empty() ? terms : filtered;
		}

		std::vector<std::string> DedupeQueries(const std::vector<std::string> & queries) {
			std::vector<std::string> output;
			std::unordered_set<std::string> seen;
			for (const auto & query : queries) {
				const auto cleaned = trim(query);
				if (cleaned.empty()) {
					continue;
				}
				const auto key = LowerAscii(cleaned);
				if (!seen.insert(key).second) {
					continue;
				}
				output.push_back(cleaned);
			}
			return output;
		}

		double ClampUnit(double value) {
			return std::max(0.0, std::min(1.0, value));
		}

		std::string NormalizeEvidenceText(const std::string & text) {
			std::string normalized;
			normalized.reserve(text.size());
			bool lastWasSpace = false;
			for (const auto ch : text) {
				const auto byte = static_cast<unsigned char>(ch);
				if (IsWhitespace(ch)) {
					if (!lastWasSpace) {
						normalized.push_back(' ');
						lastWasSpace = true;
					}
					continue;
				}
				lastWasSpace = false;
				normalized.push_back(ToLowerAscii(static_cast<char>(byte)));
			}
			return trim(normalized);
		}

		std::string StripCitationLeadIn(std::string value) {
			static const std::unordered_set<std::string> fillerWords = {
				"for", "about", "on", "regarding", "re", "related", "to", "the",
				"some", "any", "relevant", "supporting", "citation", "citations",
				"cite", "source", "sources", "quote", "quotes", "evidence"
			};
			value = trim(value);
			std::size_t start = 0;
			while (start < value.size()) {
				while (start < value.size() && !IsTokenChar(value[start])) {
					++start;
				}
				auto end = start;
				while (end < value.size() && IsTokenChar(value[end])) {
					++end;
				}
				if (end == start) {
					break;
				}
				const auto word = LowerAscii(value.substr(start, end - start));
				if (fillerWords.find(word) == fillerWords.end()) {
					break;
				}
				start = end;
			}
			auto stripped = trim(value.substr(start));
			while (!stripped.empty() && !std::isalnum(static_cast<unsigned char>(stripped.back()))) {
				stripped.pop_back();
			}
			return trim(stripped);
		}

		std::vector<std::string> TopicTerms(const std::string & topic) {
			std::vector<std::string> terms;
			for (const auto & term : tokenize(topic)) {
				if (term.size() >= 3 && StopWords().find(term) == StopWords().end()) {
					terms.push_back(term);
				}
			}
			return terms;
		}

		double ScoreQuoteCandidate(
			const std::string & quote,
			const std::string & label,
			const std::string & uri,
			const std::string & topic,
			const std::vector<std::string> & terms) {
			const auto normalizedQuote = NormalizeEvidenceText(quote);
			const auto normalizedTopic = NormalizeEvidenceText(topic);
			const auto normalizedLabel = NormalizeEvidenceText(label);
			const auto normalizedUri = NormalizeEvidenceText(uri);
			if (normalizedQuote.empty()) {
				return 0.0;
			}

			double score = 0.0;
			if (!normalizedTopic.empty() && normalizedQuote.find(normalizedTopic) != std::string::npos) {
				score += 5.0;
			}
			for (const auto & term : terms) {
				if (normalizedQuote.find(term) != std::string::npos) {
					score += 1.0;
				}
				if (!normalizedLabel.empty() && normalizedLabel.find(term) != std::string::npos) {
					score += 0.75;
				}
				if (!normalizedUri.empty() && normalizedUri.find(term) != std::string::npos) {
					score += 0.35;
				}
			}
			if (normalizedQuote.size() >= 40 && normalizedQuote.size() <= 420) {
				score += 0.5;
			}
			if (quote.find('\n') != std::string::npos) {
				score += 0.2;
			}
			if (quote.find('"') != std::string::npos) {
				score += 0.15;
			}
			return score;
		}

		void FinalizeCitationStats(ofxGgmlRagCitationSearchResult & result) {
			result.sourceDiversityScore = sourceDiversityScore(result.citations);
			double totalConfidence = 0.0;
			for (const auto & citation : result.citations) {
				totalConfidence += citation.confidenceScore;
			}
			result.averageConfidence = result.citations.empty()
				? 0.0
				: totalConfidence / static_cast<double>(result.citations.size());
		}

		std::vector<std::string> NormalizeTags(const std::vector<std::string> & input) {
			std::vector<std::string> tags;
			for (const auto & tag : input) {
				const auto normalized = trim(tag);
				if (normalized.empty()) {
					continue;
				}
				if (std::find(tags.begin(), tags.end(), normalized) == tags.end()) {
					tags.push_back(normalized);
				}
			}
			return tags;
		}

		std::vector<std::string> NormalizeExtensions(const std::vector<std::string> & input) {
			std::vector<std::string> extensions;
			for (const auto & extension : input) {
				auto normalized = LowerAscii(trim(extension));
				if (normalized.empty()) {
					continue;
				}
				if (normalized[0] != '.') {
					normalized = "." + normalized;
				}
				if (std::find(extensions.begin(), extensions.end(), normalized) == extensions.end()) {
					extensions.push_back(normalized);
				}
			}
			return extensions;
		}

		bool HasAllowedExtension(const fs::path & path, const std::vector<std::string> & extensions) {
			if (extensions.empty()) {
				return true;
			}
			const auto extension = LowerAscii(path.extension().generic_string());
			return std::find(extensions.begin(), extensions.end(), extension) != extensions.end();
		}

		bool LooksBinary(const std::string & text) {
			return text.find('\0') != std::string::npos;
		}

		bool StartsWithAt(const std::string & text, std::size_t offset, const std::string & prefix) {
			return offset + prefix.size() <= text.size() &&
				text.compare(offset, prefix.size(), prefix) == 0;
		}

		std::string HtmlTagName(const std::string & html, std::size_t tagBegin) {
			auto cursor = tagBegin + 1;
			if (cursor < html.size() && html[cursor] == '/') {
				++cursor;
			}
			while (cursor < html.size() && IsWhitespace(html[cursor])) {
				++cursor;
			}
			std::string name;
			while (cursor < html.size()) {
				const auto ch = html[cursor];
				if (!std::isalnum(static_cast<unsigned char>(ch))) {
					break;
				}
				name.push_back(ToLowerAscii(ch));
				++cursor;
			}
			return name;
		}

		bool IsHtmlBlockTag(const std::string & tagName) {
			static const std::unordered_set<std::string> blockTags = {
				"address", "article", "aside", "blockquote", "br", "dd", "div",
				"dl", "dt", "figcaption", "figure", "footer", "h1", "h2", "h3",
				"h4", "h5", "h6", "header", "hr", "li", "main", "nav", "ol",
				"p", "pre", "section", "table", "tbody", "td", "tfoot", "th",
				"thead", "tr", "ul"
			};
			return blockTags.find(tagName) != blockTags.end();
		}

		std::string RemoveHtmlElementBlocks(std::string html, const std::vector<std::string> & tagNames) {
			auto lowered = LowerAscii(html);
			for (const auto & tagName : tagNames) {
				const auto openNeedle = "<" + tagName;
				const auto closeNeedle = "</" + tagName + ">";
				std::size_t searchBegin = 0;
				while (searchBegin < lowered.size()) {
					const auto open = lowered.find(openNeedle, searchBegin);
					if (open == std::string::npos) {
						break;
					}
					if (open + openNeedle.size() < lowered.size()) {
						const auto next = lowered[open + openNeedle.size()];
						if (std::isalnum(static_cast<unsigned char>(next)) || next == '-') {
							searchBegin = open + openNeedle.size();
							continue;
						}
					}
					const auto close = lowered.find(closeNeedle, open + openNeedle.size());
					const auto eraseEnd = close == std::string::npos
						? lowered.find('>', open)
						: close + closeNeedle.size();
					if (eraseEnd == std::string::npos) {
						html.erase(open);
						lowered.erase(open);
						break;
					}
					html.erase(open, eraseEnd - open);
					lowered.erase(open, eraseEnd - open);
					searchBegin = open;
				}
			}
			return html;
		}

		char DecodeNumericHtmlEntity(const std::string & entity) {
			if (entity.size() < 3 || entity[0] != '#') {
				return '\0';
			}
			int value = 0;
			std::size_t begin = 1;
			const bool hex = begin < entity.size() && (entity[begin] == 'x' || entity[begin] == 'X');
			if (hex) {
				++begin;
			}
			for (std::size_t i = begin; i < entity.size(); ++i) {
				const auto ch = entity[i];
				int digit = -1;
				if (ch >= '0' && ch <= '9') {
					digit = ch - '0';
				} else if (hex && ch >= 'a' && ch <= 'f') {
					digit = 10 + ch - 'a';
				} else if (hex && ch >= 'A' && ch <= 'F') {
					digit = 10 + ch - 'A';
				}
				if (digit < 0) {
					return '\0';
				}
				value = value * (hex ? 16 : 10) + digit;
				if (value > 126) {
					return ' ';
				}
			}
			return value >= 0x20 && value <= 0x7e ? static_cast<char>(value) : ' ';
		}

		std::string DecodeHtmlEntities(const std::string & text) {
			std::string decoded;
			decoded.reserve(text.size());
			for (std::size_t i = 0; i < text.size(); ++i) {
				if (text[i] != '&') {
					decoded.push_back(text[i]);
					continue;
				}
				const auto semicolon = text.find(';', i + 1);
				if (semicolon == std::string::npos || semicolon - i > 12) {
					decoded.push_back(text[i]);
					continue;
				}
				const auto entity = text.substr(i + 1, semicolon - i - 1);
				const auto loweredEntity = LowerAscii(entity);
				if (loweredEntity == "amp") {
					decoded.push_back('&');
				} else if (loweredEntity == "lt") {
					decoded.push_back('<');
				} else if (loweredEntity == "gt") {
					decoded.push_back('>');
				} else if (loweredEntity == "quot") {
					decoded.push_back('"');
				} else if (loweredEntity == "apos" || loweredEntity == "#39") {
					decoded.push_back('\'');
				} else if (loweredEntity == "nbsp") {
					decoded.push_back(' ');
				} else if (!loweredEntity.empty() && loweredEntity[0] == '#') {
					decoded.push_back(DecodeNumericHtmlEntity(entity));
				} else {
					decoded.push_back('&');
					decoded += entity;
					decoded.push_back(';');
				}
				i = semicolon;
			}
			return decoded;
		}

		std::string CollapseTextWhitespace(const std::string & text) {
			std::string collapsed;
			collapsed.reserve(text.size());
			bool lastWasSpace = false;
			for (const auto ch : text) {
				if (IsWhitespace(ch)) {
					if (!lastWasSpace) {
						collapsed.push_back(' ');
						lastWasSpace = true;
					}
					continue;
				}
				lastWasSpace = false;
				collapsed.push_back(ch);
			}
			return trim(collapsed);
		}

		std::string ExtractHtmlElementText(const std::string & html, const std::string & tagName) {
			const auto lowered = LowerAscii(html);
			const auto openNeedle = "<" + tagName;
			const auto open = lowered.find(openNeedle);
			if (open == std::string::npos) {
				return "";
			}
			const auto contentBegin = lowered.find('>', open + openNeedle.size());
			if (contentBegin == std::string::npos) {
				return "";
			}
			const auto close = lowered.find("</" + tagName + ">", contentBegin + 1);
			if (close == std::string::npos) {
				return "";
			}
			return CollapseTextWhitespace(DecodeHtmlEntities(html.substr(contentBegin + 1, close - contentBegin - 1)));
		}

		std::string HtmlAttributeValue(const std::string & tag, const std::string & attributeName) {
			const auto loweredTag = LowerAscii(tag);
			const auto loweredAttribute = LowerAscii(attributeName);
			std::size_t searchBegin = 0;
			while (searchBegin < loweredTag.size()) {
				const auto found = loweredTag.find(loweredAttribute, searchBegin);
				if (found == std::string::npos) {
					return "";
				}
				const auto beforeOk = found == 0 || !IsTokenChar(loweredTag[found - 1]);
				const auto after = found + loweredAttribute.size();
				const auto afterOk = after >= loweredTag.size() || !IsTokenChar(loweredTag[after]);
				if (!beforeOk || !afterOk) {
					searchBegin = found + loweredAttribute.size();
					continue;
				}

				auto cursor = after;
				while (cursor < tag.size() && IsWhitespace(tag[cursor])) {
					++cursor;
				}
				if (cursor >= tag.size() || tag[cursor] != '=') {
					return "";
				}
				++cursor;
				while (cursor < tag.size() && IsWhitespace(tag[cursor])) {
					++cursor;
				}
				if (cursor >= tag.size()) {
					return "";
				}

				if (tag[cursor] == '"' || tag[cursor] == '\'') {
					const auto quote = tag[cursor];
					const auto valueBegin = cursor + 1;
					const auto valueEnd = tag.find(quote, valueBegin);
					return valueEnd == std::string::npos ? "" : tag.substr(valueBegin, valueEnd - valueBegin);
				}

				const auto valueBegin = cursor;
				auto valueEnd = valueBegin;
				while (valueEnd < tag.size() && !IsWhitespace(tag[valueEnd]) && tag[valueEnd] != '>') {
					++valueEnd;
				}
				return tag.substr(valueBegin, valueEnd - valueBegin);
			}
			return "";
		}

		std::string UrlScheme(const std::string & url) {
			const auto trimmed = trim(url);
			const auto schemeEnd = trimmed.find("://");
			if (schemeEnd == std::string::npos) {
				return "";
			}
			return LowerAscii(trimmed.substr(0, schemeEnd));
		}

		bool HasHttpScheme(const std::string & url) {
			const auto scheme = UrlScheme(url);
			return scheme == "http" || scheme == "https";
		}

		std::size_t UrlPathBegin(const std::string & url) {
			const auto schemeEnd = url.find("://");
			if (schemeEnd == std::string::npos) {
				return std::string::npos;
			}
			const auto authorityBegin = schemeEnd + 3;
			const auto pathBegin = url.find_first_of("/?#", authorityBegin);
			return pathBegin == std::string::npos ? url.size() : pathBegin;
		}

		std::string UrlOrigin(const std::string & url) {
			const auto trimmed = trim(url);
			const auto pathBegin = UrlPathBegin(trimmed);
			if (pathBegin == std::string::npos || pathBegin == 0) {
				return "";
			}
			return trimmed.substr(0, pathBegin);
		}

		std::string NormalizeUrlPath(const std::string & path) {
			std::vector<std::string> parts;
			std::size_t begin = 0;
			while (begin <= path.size()) {
				const auto end = path.find('/', begin);
				const auto part = path.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
				if (!part.empty() && part != ".") {
					if (part == "..") {
						if (!parts.empty()) {
							parts.pop_back();
						}
					} else {
						parts.push_back(part);
					}
				}
				if (end == std::string::npos) {
					break;
				}
				begin = end + 1;
			}

			std::ostringstream normalized;
			normalized << "/";
			for (std::size_t i = 0; i < parts.size(); ++i) {
				if (i > 0) {
					normalized << "/";
				}
				normalized << parts[i];
			}
			return normalized.str();
		}

		std::string NormalizeAbsoluteWebUrl(const std::string & url, bool includeFragments) {
			auto normalized = trim(url);
			if (normalized.empty() || !HasHttpScheme(normalized)) {
				return "";
			}
			if (!includeFragments) {
				const auto fragment = normalized.find('#');
				if (fragment != std::string::npos) {
					normalized = normalized.substr(0, fragment);
				}
			}
			const auto origin = UrlOrigin(normalized);
			if (origin.empty()) {
				return "";
			}
			auto pathAndQuery = normalized.substr(origin.size());
			if (pathAndQuery.empty() || pathAndQuery[0] == '?' || pathAndQuery[0] == '#') {
				pathAndQuery = "/" + pathAndQuery;
			}
			const auto query = pathAndQuery.find('?');
			const auto path = query == std::string::npos ? pathAndQuery : pathAndQuery.substr(0, query);
			const auto suffix = query == std::string::npos ? std::string() : pathAndQuery.substr(query);
			return origin + NormalizeUrlPath(path) + suffix;
		}

		std::string ResolveHtmlLink(const std::string & sourceUrl, const std::string & href, bool includeFragments) {
			auto decoded = trim(DecodeHtmlEntities(href));
			if (decoded.empty() || decoded[0] == '#') {
				return "";
			}
			const auto lowered = LowerAscii(decoded);
			if (lowered.find("javascript:") == 0 || lowered.find("mailto:") == 0 || lowered.find("data:") == 0) {
				return "";
			}

			const auto source = trim(sourceUrl);
			const auto sourceScheme = UrlScheme(source);
			const auto sourceOrigin = UrlOrigin(source);
			if (StartsWithAt(decoded, 0, "//")) {
				if (sourceScheme.empty()) {
					return "";
				}
				return NormalizeAbsoluteWebUrl(sourceScheme + ":" + decoded, includeFragments);
			}
			if (HasHttpScheme(decoded)) {
				return NormalizeAbsoluteWebUrl(decoded, includeFragments);
			}
			if (sourceOrigin.empty()) {
				return "";
			}
			if (decoded[0] == '/') {
				return NormalizeAbsoluteWebUrl(sourceOrigin + decoded, includeFragments);
			}

			const auto sourcePathBegin = UrlPathBegin(source);
			auto basePath = std::string("/");
			if (sourcePathBegin != std::string::npos && sourcePathBegin < source.size()) {
				auto sourcePath = source.substr(sourcePathBegin);
				const auto query = sourcePath.find_first_of("?#");
				if (query != std::string::npos) {
					sourcePath = sourcePath.substr(0, query);
				}
				const auto lastSlash = sourcePath.find_last_of('/');
				basePath = lastSlash == std::string::npos ? "/" : sourcePath.substr(0, lastSlash + 1);
			}
			return NormalizeAbsoluteWebUrl(sourceOrigin + basePath + decoded, includeFragments);
		}

		std::vector<std::string> NormalizeUrlPrefixes(const std::vector<std::string> & prefixes) {
			std::vector<std::string> normalized;
			for (const auto & prefix : prefixes) {
				const auto value = LowerAscii(trim(prefix));
				if (value.empty()) {
					continue;
				}
				if (std::find(normalized.begin(), normalized.end(), value) == normalized.end()) {
					normalized.push_back(value);
				}
			}
			return normalized;
		}

		bool UrlMatchesPrefix(const std::string & url, const std::vector<std::string> & prefixes) {
			if (prefixes.empty()) {
				return false;
			}
			const auto lowered = LowerAscii(url);
			for (const auto & prefix : prefixes) {
				if (lowered.find(prefix) == 0) {
					return true;
				}
			}
			return false;
		}

		std::string UrlPathAndQuery(const std::string & url) {
			const auto normalized = NormalizeAbsoluteWebUrl(url, false);
			const auto origin = UrlOrigin(normalized);
			if (origin.empty() || normalized.size() <= origin.size()) {
				return "/";
			}
			return normalized.substr(origin.size());
		}

		bool UserAgentMatches(const std::string & ruleAgent, const std::string & requestedAgent) {
			const auto rule = LowerAscii(trim(ruleAgent));
			const auto requested = LowerAscii(trim(requestedAgent));
			if (rule.empty() || requested.empty()) {
				return false;
			}
			return rule == "*" || requested.find(rule) != std::string::npos || rule.find(requested) != std::string::npos;
		}

		bool RobotsTxtPolicyAllowsUrl(const ofxGgmlRagRobotsTxtPolicy & policy, const std::string & url) {
			if (!policy) {
				return true;
			}
			const auto path = UrlPathAndQuery(url);
			bool allowed = true;
			std::size_t bestLength = 0;
			for (const auto & rule : policy.rules) {
				const auto prefix = trim(rule.pathPrefix);
				if (prefix.empty() || path.find(prefix) != 0) {
					continue;
				}
				const auto length = prefix.size();
				if (length > bestLength || (length == bestLength && rule.allow)) {
					bestLength = length;
					allowed = rule.allow;
				}
			}
			return allowed;
		}

		void AddWarning(ofxGgmlRagCorpus & corpus, const std::string & warning) {
			if (std::find(corpus.warnings.begin(), corpus.warnings.end(), warning) == corpus.warnings.end()) {
				corpus.warnings.push_back(warning);
			}
		}

		std::vector<fs::path> CollectRegularFiles(const fs::path & root, bool recursive, ofxGgmlRagCorpus & corpus) {
			std::vector<fs::path> files;
			std::error_code error;

			if (recursive) {
				fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, error);
				fs::recursive_directory_iterator end;
				if (error) {
					AddWarning(corpus, "could not scan sourceRoot: " + error.message());
					return files;
				}
				for (; it != end; it.increment(error)) {
					if (error) {
						AddWarning(corpus, "skipped directory entry: " + error.message());
						error.clear();
						continue;
					}
					if (!it->is_regular_file(error)) {
						error.clear();
						continue;
					}
					files.push_back(it->path());
				}
			} else {
				fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, error);
				fs::directory_iterator end;
				if (error) {
					AddWarning(corpus, "could not scan sourceRoot: " + error.message());
					return files;
				}
				for (; it != end; it.increment(error)) {
					if (error) {
						AddWarning(corpus, "skipped directory entry: " + error.message());
						error.clear();
						continue;
					}
					if (!it->is_regular_file(error)) {
						error.clear();
						continue;
					}
					files.push_back(it->path());
				}
			}

			std::sort(files.begin(), files.end(), [](const fs::path & left, const fs::path & right) {
				return PathToSource(left) < PathToSource(right);
			});
			return files;
		}

		std::size_t FindChunkEnd(const std::string & text, std::size_t begin, std::size_t maxChars) {
			const auto hardEnd = std::min(text.size(), begin + maxChars);
			if (hardEnd == text.size()) {
				return hardEnd;
			}

			for (auto cursor = hardEnd; cursor > begin; --cursor) {
				if (IsWhitespace(text[cursor - 1])) {
					return cursor - 1;
				}
			}

			return hardEnd;
		}

		bool ContainsAllTags(const std::vector<std::string> & tags, const std::vector<std::string> & requiredTags) {
			for (const auto & required : requiredTags) {
				if (std::find(tags.begin(), tags.end(), required) == tags.end()) {
					return false;
				}
			}
			return true;
		}

		bool ContainsAnyTag(const std::vector<std::string> & tags, const std::vector<std::string> & excludedTags) {
			for (const auto & excluded : excludedTags) {
				if (std::find(tags.begin(), tags.end(), excluded) != tags.end()) {
					return true;
				}
			}
			return false;
		}

		bool MatchesAnySourceRoot(const std::vector<std::string> & sourceRoots, const std::string & source) {
			for (const auto & root : sourceRoots) {
				if (sourceMatchesRoot(root, source)) {
					return true;
				}
			}
			return false;
		}

		bool AppendWithinLimit(std::string & target, const std::string & value, std::size_t maxChars) {
			if (target.size() + value.size() > maxChars) {
				return false;
			}
			target += value;
			return true;
		}

		void AppendJsonString(std::ostringstream & out, const std::string & value) {
			out << "\"";
			for (const auto ch : value) {
				const auto byte = static_cast<unsigned char>(ch);
				switch (ch) {
					case '"':
						out << "\\\"";
						break;
					case '\\':
						out << "\\\\";
						break;
					case '\n':
						out << "\\n";
						break;
					case '\r':
						out << "\\r";
						break;
					case '\t':
						out << "\\t";
						break;
					default:
						if (byte < 0x20) {
							out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(byte) << std::dec << std::setfill(' ');
						} else {
							out << ch;
						}
						break;
				}
			}
			out << "\"";
		}

		void AppendJsonStringArray(std::ostringstream & out, const std::vector<std::string> & values) {
			out << "[";
			for (std::size_t i = 0; i < values.size(); ++i) {
				if (i > 0) {
					out << ",";
				}
				AppendJsonString(out, values[i]);
			}
			out << "]";
		}

		void AppendJsonCitations(std::ostringstream & out, const std::vector<ofxGgmlRagCitation> & citations) {
			out << "[";
			for (std::size_t i = 0; i < citations.size(); ++i) {
				if (i > 0) {
					out << ",";
				}
				const auto & citation = citations[i];
				out << "{";
				out << "\"source\":";
				AppendJsonString(out, citation.source);
				out << ",\"label\":";
				AppendJsonString(out, citation.label);
				out << ",\"url\":";
				AppendJsonString(out, citation.url);
				out << ",\"start\":" << citation.start;
				out << ",\"end\":" << citation.end;
				out << "}";
			}
			out << "]";
		}

		std::string PrettyPrintJson(const std::string & compact) {
			std::ostringstream pretty;
			int indent = 0;
			bool inString = false;
			bool escaped = false;

			for (const auto ch : compact) {
				if (inString) {
					pretty << ch;
					if (escaped) {
						escaped = false;
					} else if (ch == '\\') {
						escaped = true;
					} else if (ch == '"') {
						inString = false;
					}
					continue;
				}

				switch (ch) {
					case '"':
						inString = true;
						pretty << ch;
						break;
					case '{':
					case '[':
						pretty << ch << "\n";
						++indent;
						pretty << std::string(static_cast<std::size_t>(indent * 2), ' ');
						break;
					case '}':
					case ']':
						pretty << "\n";
						--indent;
						pretty << std::string(static_cast<std::size_t>(indent * 2), ' ') << ch;
						break;
					case ',':
						pretty << ch << "\n" << std::string(static_cast<std::size_t>(indent * 2), ' ');
						break;
					case ':':
						pretty << ": ";
						break;
					default:
						pretty << ch;
						break;
				}
			}

			return pretty.str();
		}
	}

	std::string trim(const std::string & text) {
		auto begin = text.begin();
		while (begin != text.end() && IsWhitespace(*begin)) {
			++begin;
		}

		auto end = text.end();
		while (end != begin && IsWhitespace(*(end - 1))) {
			--end;
		}

		return std::string(begin, end);
	}

	std::vector<std::string> tokenize(const std::string & text) {
		std::vector<std::string> tokens;
		std::string current;
		for (const auto value : text) {
			if (IsTokenChar(value)) {
				current.push_back(ToLowerAscii(value));
				continue;
			}
			if (!current.empty()) {
				if (std::find(tokens.begin(), tokens.end(), current) == tokens.end()) {
					tokens.push_back(current);
				}
				current.clear();
			}
		}
		if (!current.empty() && std::find(tokens.begin(), tokens.end(), current) == tokens.end()) {
			tokens.push_back(current);
		}
		return tokens;
	}

	std::vector<std::string> refinedQueries(const std::string & query, std::size_t maxAdditionalQueries) {
		if (maxAdditionalQueries == 0) {
			return {};
		}
		const auto terms = ScoringTerms(query, true);
		std::vector<std::string> candidates;
		if (!terms.empty()) {
			std::ostringstream focused;
			for (std::size_t i = 0; i < terms.size(); ++i) {
				if (i > 0) {
					focused << " ";
				}
				focused << terms[i];
			}
			candidates.push_back(focused.str());
		}
		if (candidates.size() < maxAdditionalQueries && terms.size() >= 2) {
			candidates.push_back(terms.front() + " " + terms.back());
		}
		auto refined = DedupeQueries(candidates);
		if (refined.size() > maxAdditionalQueries) {
			refined.resize(maxAdditionalQueries);
		}
		return refined;
	}

	float l2Norm(const std::vector<float> & values) {
		double sum = 0.0;
		for (const auto value : values) {
			sum += static_cast<double>(value) * static_cast<double>(value);
		}
		return static_cast<float>(std::sqrt(sum));
	}

	float cosineSimilarity(const std::vector<float> & a, const std::vector<float> & b) {
		if (a.empty() || b.empty() || a.size() != b.size()) {
			return 0.0f;
		}
		const auto aNorm = l2Norm(a);
		const auto bNorm = l2Norm(b);
		if (aNorm == 0.0f || bNorm == 0.0f) {
			return 0.0f;
		}
		double dot = 0.0;
		for (std::size_t i = 0; i < a.size(); ++i) {
			dot += static_cast<double>(a[i]) * static_cast<double>(b[i]);
		}
		return static_cast<float>(dot / (static_cast<double>(aNorm) * static_cast<double>(bNorm)));
	}

	bool sourceMatchesRoot(const std::string & sourceRoot, const std::string & source) {
		const auto root = NormalizeSourcePath(sourceRoot);
		if (root.empty()) {
			return true;
		}

		const auto normalizedSource = NormalizeSourcePath(source);
		if (normalizedSource == root) {
			return true;
		}
		if (normalizedSource.size() <= root.size()) {
			return false;
		}
		return normalizedSource.compare(0, root.size(), root) == 0 && normalizedSource[root.size()] == '/';
	}

	bool hasInput(const ofxGgmlRagRequest & request) {
		return !trim(request.query).empty();
	}

	ofxGgmlRagValidation validate(const ofxGgmlRagRequest & request) {
		ofxGgmlRagValidation validation;
		if (!hasInput(request)) {
			validation.errors.push_back("query is required");
		}
		if (trim(request.sourceRoot).empty()) {
			validation.warnings.push_back("sourceRoot is empty");
		}
		validation.valid = validation.errors.empty();
		return validation;
	}

	std::vector<std::string> normalizedTags(const ofxGgmlRagRequest & request) {
		return NormalizeTags(request.tags);
	}

	std::string describe(const ofxGgmlRagRequest & request) {
		if (!hasInput(request)) {
			return "rag: empty request";
		}
		std::ostringstream description;
		description << "rag: " << trim(request.query);
		const auto root = trim(request.sourceRoot);
		if (!root.empty()) {
			description << " in " << root;
		}
		const auto tags = normalizedTags(request);
		if (!tags.empty()) {
			description << " [";
			for (std::size_t i = 0; i < tags.size(); ++i) {
				if (i > 0) {
					description << ", ";
				}
				description << tags[i];
			}
			description << "]";
		}
		return description.str();
	}

	ofxGgmlRagCorpus loadTextCorpus(
		const std::string & sourceRoot,
		const ofxGgmlRagCorpusOptions & options) {
		ofxGgmlRagCorpus corpus;
		const auto normalizedInputRoot = trim(sourceRoot);
		if (normalizedInputRoot.empty()) {
			corpus.error = "sourceRoot is required";
			return corpus;
		}

		std::error_code error;
		const auto rootPath = fs::absolute(fs::path(normalizedInputRoot), error).lexically_normal();
		if (error) {
			corpus.error = "could not resolve sourceRoot: " + error.message();
			return corpus;
		}
		corpus.sourceRoot = PathToSource(rootPath);
		if (!fs::exists(rootPath, error) || error) {
			corpus.error = "sourceRoot was not found";
			return corpus;
		}
		if (!fs::is_directory(rootPath, error) || error) {
			corpus.error = "sourceRoot is not a directory";
			return corpus;
		}

		const auto extensions = NormalizeExtensions(options.extensions);
		const auto tags = NormalizeTags(options.tags);
		const auto files = CollectRegularFiles(rootPath, options.recursive, corpus);
		corpus.stats.discoveredFileCount = files.size();

		for (const auto & file : files) {
			const auto source = PathToSource(file);
			if (!HasAllowedExtension(file, extensions)) {
				++corpus.stats.skippedFileCount;
				continue;
			}

			error.clear();
			const auto byteCount = fs::file_size(file, error);
			if (error) {
				++corpus.stats.skippedFileCount;
				AddWarning(corpus, "could not read size: " + source);
				continue;
			}
			if (byteCount > options.maxFileBytes) {
				++corpus.stats.skippedFileCount;
				corpus.stats.skippedByteCount += static_cast<std::size_t>(std::min<std::uintmax_t>(
					byteCount,
					static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())));
				AddWarning(corpus, "skipped oversized file: " + source);
				continue;
			}

			std::ifstream input(file, std::ios::binary);
			if (!input) {
				++corpus.stats.skippedFileCount;
				AddWarning(corpus, "could not read file: " + source);
				continue;
			}
			std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
			if (!input.good() && !input.eof()) {
				++corpus.stats.skippedFileCount;
				AddWarning(corpus, "could not finish reading file: " + source);
				continue;
			}
			if (LooksBinary(text)) {
				++corpus.stats.skippedFileCount;
				corpus.stats.skippedByteCount += text.size();
				AddWarning(corpus, "skipped binary-looking file: " + source);
				continue;
			}
			if (!options.includeEmptyFiles && trim(text).empty()) {
				++corpus.stats.skippedFileCount;
				continue;
			}

			ofxGgmlRagDocument document;
			document.source = source;
			document.text = text;
			document.tags = tags;
			corpus.documents.push_back(document);
		}

		corpus.stats.loadedDocumentCount = corpus.documents.size();
		if (corpus.documents.empty()) {
			corpus.error = "no supported text documents";
			return corpus;
		}
		corpus.success = true;
		return corpus;
	}

	std::string htmlToText(const std::string & html) {
		auto cleaned = RemoveHtmlElementBlocks(html, { "script", "style", "noscript", "head" });
		std::string text;
		text.reserve(cleaned.size());
		for (std::size_t i = 0; i < cleaned.size(); ++i) {
			if (cleaned[i] != '<') {
				text.push_back(cleaned[i]);
				continue;
			}
			if (StartsWithAt(cleaned, i, "<!--")) {
				const auto end = cleaned.find("-->", i + 4);
				if (end == std::string::npos) {
					break;
				}
				i = end + 2;
				continue;
			}
			const auto tagName = HtmlTagName(cleaned, i);
			if (IsHtmlBlockTag(tagName)) {
				text.push_back(' ');
			}
			const auto end = cleaned.find('>', i + 1);
			if (end == std::string::npos) {
				break;
			}
			i = end;
			if (IsHtmlBlockTag(tagName)) {
				text.push_back(' ');
			}
		}
		return CollapseTextWhitespace(DecodeHtmlEntities(text));
	}

	ofxGgmlRagHtmlRobotsPolicy htmlRobotsPolicy(const std::string & html) {
		ofxGgmlRagHtmlRobotsPolicy policy;
		const auto lowered = LowerAscii(html);
		std::size_t searchBegin = 0;
		while (searchBegin < lowered.size()) {
			const auto metaBegin = lowered.find("<meta", searchBegin);
			if (metaBegin == std::string::npos) {
				break;
			}
			if (metaBegin + 5 < lowered.size() && std::isalnum(static_cast<unsigned char>(lowered[metaBegin + 5]))) {
				searchBegin = metaBegin + 5;
				continue;
			}
			const auto tagEnd = lowered.find('>', metaBegin + 5);
			if (tagEnd == std::string::npos) {
				break;
			}
			const auto tag = html.substr(metaBegin, tagEnd - metaBegin + 1);
			const auto name = LowerAscii(trim(DecodeHtmlEntities(HtmlAttributeValue(tag, "name"))));
			const auto httpEquiv = LowerAscii(trim(DecodeHtmlEntities(HtmlAttributeValue(tag, "http-equiv"))));
			if (name == "robots" || name == "googlebot" || httpEquiv == "x-robots-tag") {
				const auto content = LowerAscii(DecodeHtmlEntities(HtmlAttributeValue(tag, "content")));
				const auto directives = tokenize(content);
				if (std::find(directives.begin(), directives.end(), "none") != directives.end()) {
					policy.noindex = true;
					policy.nofollow = true;
				}
				if (std::find(directives.begin(), directives.end(), "noindex") != directives.end()) {
					policy.noindex = true;
				}
				if (std::find(directives.begin(), directives.end(), "nofollow") != directives.end()) {
					policy.nofollow = true;
				}
			}
			searchBegin = tagEnd + 1;
		}
		return policy;
	}

	ofxGgmlRagHtmlDocument documentFromHtml(
		const std::string & sourceUrl,
		const std::string & html,
		const ofxGgmlRagHtmlOptions & options) {
		ofxGgmlRagHtmlDocument result;
		result.sourceUrl = trim(sourceUrl);
		if (result.sourceUrl.empty()) {
			result.error = "sourceUrl is required";
			return result;
		}
		if (html.empty()) {
			result.error = "html is required";
			return result;
		}
		if (options.maxHtmlBytes > 0 && html.size() > options.maxHtmlBytes) {
			result.error = "html exceeds maxHtmlBytes";
			return result;
		}
		const auto robots = htmlRobotsPolicy(html);
		if (options.respectRobotsMeta && robots.noindex) {
			result.error = "html robots noindex";
			return result;
		}

		result.title = ExtractHtmlElementText(html, "title");
		auto bodyText = htmlToText(html);
		if (options.includeTitleInText && !result.title.empty()) {
			bodyText = CollapseTextWhitespace(result.title + " " + bodyText);
		}
		if (bodyText.empty()) {
			result.error = "html text is empty";
			return result;
		}

		result.document.source = result.sourceUrl;
		result.document.text = bodyText;
		result.document.tags = NormalizeTags(options.tags);
		result.document.qualityHint = ClampUnit(options.qualityHint);
		result.success = true;
		return result;
	}

	ofxGgmlRagHtmlLinkFrontier planHtmlLinkFrontier(
		const std::string & sourceUrl,
		const std::string & html,
		const ofxGgmlRagHtmlLinkOptions & options) {
		ofxGgmlRagHtmlLinkFrontier frontier;
		frontier.sourceUrl = trim(sourceUrl);
		if (options.respectRobotsMeta && htmlRobotsPolicy(html).nofollow) {
			frontier.stats.robotsMetaNofollow = true;
			frontier.success = true;
			return frontier;
		}

		const auto sourceOrigin = LowerAscii(UrlOrigin(frontier.sourceUrl));
		if (sourceOrigin.empty()) {
			frontier.error = "sourceUrl origin is required";
			return frontier;
		}

		const auto cleaned = RemoveHtmlElementBlocks(html, { "script", "style", "noscript" });
		const auto lowered = LowerAscii(cleaned);
		const auto allowedPrefixes = NormalizeUrlPrefixes(options.allowedUrlPrefixes);
		const auto excludedPrefixes = NormalizeUrlPrefixes(options.excludedUrlPrefixes);
		const auto applyRobotsTxt = options.respectRobotsTxt && !trim(options.robotsTxt).empty();
		const auto robotsTxtPolicy = applyRobotsTxt
			? parseRobotsTxt(options.robotsTxt, options.robotsTxtUserAgent)
			: ofxGgmlRagRobotsTxtPolicy();
		std::unordered_set<std::string> seen;
		std::size_t searchBegin = 0;
		while (searchBegin < lowered.size()) {
			const auto anchorBegin = lowered.find("<a", searchBegin);
			if (anchorBegin == std::string::npos) {
				break;
			}
			if (anchorBegin + 2 < lowered.size() && std::isalnum(static_cast<unsigned char>(lowered[anchorBegin + 2]))) {
				searchBegin = anchorBegin + 2;
				continue;
			}
			const auto tagEnd = lowered.find('>', anchorBegin + 2);
			if (tagEnd == std::string::npos) {
				break;
			}
			const auto tag = cleaned.substr(anchorBegin, tagEnd - anchorBegin + 1);
			const auto loweredTag = LowerAscii(tag);
			const auto href = loweredTag.find("href");
			if (href == std::string::npos) {
				searchBegin = tagEnd + 1;
				continue;
			}
			auto cursor = href + 4;
			while (cursor < loweredTag.size() && IsWhitespace(loweredTag[cursor])) {
				++cursor;
			}
			if (cursor >= loweredTag.size() || loweredTag[cursor] != '=') {
				searchBegin = tagEnd + 1;
				continue;
			}
			++cursor;
			while (cursor < tag.size() && IsWhitespace(tag[cursor])) {
				++cursor;
			}
			if (cursor >= tag.size()) {
				searchBegin = tagEnd + 1;
				continue;
			}

			std::string hrefValue;
			if (tag[cursor] == '"' || tag[cursor] == '\'') {
				const auto quote = tag[cursor];
				const auto valueBegin = cursor + 1;
				const auto valueEnd = tag.find(quote, valueBegin);
				if (valueEnd != std::string::npos) {
					hrefValue = tag.substr(valueBegin, valueEnd - valueBegin);
				}
			} else {
				const auto valueBegin = cursor;
				auto valueEnd = valueBegin;
				while (valueEnd < tag.size() && !IsWhitespace(tag[valueEnd]) && tag[valueEnd] != '>') {
					++valueEnd;
				}
				hrefValue = tag.substr(valueBegin, valueEnd - valueBegin);
			}

			++frontier.stats.discoveredLinkCount;
			ofxGgmlRagHtmlLinkDecision decision;
			decision.href = hrefValue;
			decision.url = ResolveHtmlLink(frontier.sourceUrl, hrefValue, options.includeFragments);
			if (decision.url.empty()) {
				decision.reason = "invalid href";
				++frontier.stats.invalidLinkCount;
			} else if (options.sameOriginOnly && LowerAscii(UrlOrigin(decision.url)) != sourceOrigin) {
				decision.reason = "off origin";
				++frontier.stats.offOriginLinkCount;
			} else if (!allowedPrefixes.empty() && !UrlMatchesPrefix(decision.url, allowedPrefixes)) {
				decision.reason = "outside allowed prefixes";
				++frontier.stats.scopeBlockedLinkCount;
			} else if (UrlMatchesPrefix(decision.url, excludedPrefixes)) {
				decision.reason = "excluded prefix";
				++frontier.stats.scopeBlockedLinkCount;
			} else if (applyRobotsTxt && !RobotsTxtPolicyAllowsUrl(robotsTxtPolicy, decision.url)) {
				decision.reason = "robots.txt disallow";
				++frontier.stats.robotsBlockedLinkCount;
			} else if (!seen.insert(LowerAscii(decision.url)).second) {
				decision.reason = "duplicate";
				++frontier.stats.duplicateLinkCount;
			} else if (frontier.links.size() >= options.maxLinks) {
				decision.reason = "maxLinks reached";
				frontier.stats.maxLinksReached = true;
			} else {
				decision.accepted = true;
				decision.reason = "accepted";
				frontier.links.push_back(decision.url);
				++frontier.stats.acceptedLinkCount;
			}
			if (!decision.accepted) {
				++frontier.stats.skippedLinkCount;
			}
			frontier.decisions.push_back(decision);
			searchBegin = tagEnd + 1;
		}
		frontier.success = true;
		return frontier;
	}

	std::vector<std::string> extractHtmlLinks(
		const std::string & sourceUrl,
		const std::string & html,
		const ofxGgmlRagHtmlLinkOptions & options) {
		return planHtmlLinkFrontier(sourceUrl, html, options).links;
	}

	ofxGgmlRagHtmlBatch documentsFromHtmlPages(
		const std::vector<ofxGgmlRagHtmlPage> & pages,
		const ofxGgmlRagHtmlBatchOptions & options) {
		ofxGgmlRagHtmlBatch batch;
		batch.stats.pageCount = pages.size();
		if (pages.empty()) {
			batch.error = "no HTML pages";
			return batch;
		}
		if (options.maxPages == 0) {
			batch.error = "maxPages is zero";
			return batch;
		}

		std::unordered_set<std::string> seenLinks;
		const auto pageCount = std::min(options.maxPages, pages.size());
		if (pageCount < pages.size()) {
			batch.stats.skippedPageCount += pages.size() - pageCount;
			batch.warnings.push_back("skipped HTML pages beyond maxPages");
		}

		for (std::size_t i = 0; i < pageCount; ++i) {
			const auto converted = documentFromHtml(pages[i].sourceUrl, pages[i].html, options.document);
			if (!converted) {
				++batch.stats.skippedPageCount;
				const auto label = trim(pages[i].sourceUrl).empty() ? std::string("page #") + std::to_string(i) : trim(pages[i].sourceUrl);
				batch.warnings.push_back(label + ": " + (converted.error.empty() ? "HTML conversion failed" : converted.error));
				continue;
			}
			batch.documents.push_back(converted.document);

			if (!options.collectLinks || batch.links.size() >= options.links.maxLinks) {
				continue;
			}
			auto linkOptions = options.links;
			const auto links = extractHtmlLinks(converted.sourceUrl, pages[i].html, linkOptions);
			for (const auto & link : links) {
				if (batch.links.size() >= options.links.maxLinks) {
					break;
				}
				if (seenLinks.insert(LowerAscii(link)).second) {
					batch.links.push_back(link);
				}
			}
		}

		batch.stats.loadedDocumentCount = batch.documents.size();
		batch.stats.linkCount = batch.links.size();
		if (batch.documents.empty()) {
			batch.error = "no HTML documents";
			return batch;
		}
		batch.success = true;
		return batch;
	}

	ofxGgmlRagRobotsTxtPolicy parseRobotsTxt(
		const std::string & robotsTxt,
		const std::string & userAgent) {
		ofxGgmlRagRobotsTxtPolicy policy;
		policy.userAgent = trim(userAgent).empty() ? "*" : trim(userAgent);
		if (trim(robotsTxt).empty()) {
			policy.success = true;
			return policy;
		}

		std::istringstream lines(robotsTxt);
		std::string line;
		bool groupMatches = false;
		bool groupHasRules = false;
		while (std::getline(lines, line)) {
			const auto comment = line.find('#');
			if (comment != std::string::npos) {
				line = line.substr(0, comment);
			}
			line = trim(line);
			if (line.empty()) {
				groupMatches = false;
				groupHasRules = false;
				continue;
			}

			const auto colon = line.find(':');
			if (colon == std::string::npos) {
				continue;
			}
			const auto field = LowerAscii(trim(line.substr(0, colon)));
			auto value = trim(line.substr(colon + 1));
			if (field == "user-agent") {
				if (groupHasRules) {
					groupMatches = false;
					groupHasRules = false;
				}
				groupMatches = groupMatches || UserAgentMatches(value, policy.userAgent);
				continue;
			}
			if (field != "allow" && field != "disallow") {
				continue;
			}
			groupHasRules = true;
			if (!groupMatches) {
				continue;
			}
			value = trim(DecodeHtmlEntities(value));
			if (value.empty()) {
				continue;
			}
			ofxGgmlRagRobotsTxtRule rule;
			rule.pathPrefix = value[0] == '/' ? value : "/" + value;
			rule.allow = field == "allow";
			policy.rules.push_back(rule);
		}

		policy.success = true;
		return policy;
	}

	bool robotsTxtAllows(
		const std::string & url,
		const std::string & robotsTxt,
		const std::string & userAgent) {
		const auto policy = parseRobotsTxt(robotsTxt, userAgent);
		return RobotsTxtPolicyAllowsUrl(policy, url);
	}

	std::vector<ofxGgmlRagChunk> chunkText(
		const std::string & source,
		const std::string & text,
		const ofxGgmlRagChunkOptions & options,
		const std::vector<std::string> & tags) {
		std::vector<ofxGgmlRagChunk> chunks;
		const auto normalizedSource = trim(source);
		const auto normalizedTags = NormalizeTags(tags);
		const auto maxChars = std::max<std::size_t>(1, options.maxChars);
		const auto overlapChars = std::min(options.overlapChars, maxChars - 1);

		std::size_t begin = 0;
		while (begin < text.size()) {
			while (begin < text.size() && IsWhitespace(text[begin])) {
				++begin;
			}
			if (begin >= text.size()) {
				break;
			}

			auto end = FindChunkEnd(text, begin, maxChars);
			while (end > begin && IsWhitespace(text[end - 1])) {
				--end;
			}
			if (end <= begin) {
				end = std::min(text.size(), begin + maxChars);
			}

			ofxGgmlRagChunk chunk;
			chunk.source = normalizedSource;
			chunk.text = text.substr(begin, end - begin);
			chunk.index = chunks.size();
			chunk.start = begin;
			chunk.end = end;
			chunk.tags = normalizedTags;
			chunks.push_back(chunk);

			if (end >= text.size()) {
				break;
			}
			begin = end;
			if (overlapChars > 0 && begin > overlapChars) {
				begin -= overlapChars;
			}
		}

		return chunks;
	}

	std::vector<ofxGgmlRagSearchHit> searchChunks(
		const std::string & query,
		const std::vector<ofxGgmlRagChunk> & chunks,
		const ofxGgmlRagSearchOptions & options) {
		std::vector<ofxGgmlRagSearchHit> hits;
		const auto queryTerms = tokenize(query);
		if (queryTerms.empty() || options.topK == 0) {
			return hits;
		}
		std::vector<std::string> candidateQueries = { query };
		for (const auto & variant : options.queryVariants) {
			candidateQueries.push_back(variant);
		}
		if (options.allowQueryRefinement) {
			const auto refined = refinedQueries(query, options.maxRefinementQueries);
			candidateQueries.insert(candidateQueries.end(), refined.begin(), refined.end());
		}
		candidateQueries = DedupeQueries(candidateQueries);

		const auto queryPhrase = LowerAscii(trim(query));
		const auto minMatchedTerms = std::max<std::size_t>(1, options.minMatchedTerms);
		const auto requiredTags = NormalizeTags(options.requiredTags);
		const auto excludedTags = NormalizeTags(options.excludedTags);
		const auto excludedSourceRoots = NormalizeTags(options.excludedSourceRoots);
		for (const auto & chunk : chunks) {
			if (MatchesAnySourceRoot(excludedSourceRoots, chunk.source)) {
				continue;
			}
			if (!ContainsAllTags(chunk.tags, requiredTags)) {
				continue;
			}
			if (ContainsAnyTag(chunk.tags, excludedTags)) {
				continue;
			}

			const auto chunkTerms = tokenize(chunk.text);
			ofxGgmlRagSearchHit hit;
			hit.chunk = chunk;
			for (const auto & candidateQuery : candidateQueries) {
				const auto candidateTerms = ScoringTerms(candidateQuery, options.ignoreStopWords);
				if (candidateTerms.empty()) {
					continue;
				}

				std::vector<std::string> matchedTerms;
				for (const auto & term : candidateTerms) {
					if (std::find(chunkTerms.begin(), chunkTerms.end(), term) != chunkTerms.end()) {
						matchedTerms.push_back(term);
					}
				}
				if (matchedTerms.empty() || matchedTerms.size() < minMatchedTerms) {
					continue;
				}
				const auto lexicalScore =
					static_cast<double>(matchedTerms.size()) / static_cast<double>(candidateTerms.size());
				if (lexicalScore > hit.lexicalScore) {
					hit.lexicalScore = lexicalScore;
					hit.matchedTerms = matchedTerms;
				}
			}
			if (hit.matchedTerms.empty()) {
				continue;
			}

			hit.qualityScore = ClampUnit(chunk.qualityHint);
			hit.score = hit.lexicalScore + (hit.qualityScore * std::max(0.0, options.qualityWeight));
			if (options.phraseBoost > 0.0 && !queryPhrase.empty() && LowerAscii(chunk.text).find(queryPhrase) != std::string::npos) {
				hit.score += options.phraseBoost;
			}
			if (hit.score < options.minScore) {
				continue;
			}
			hits.push_back(hit);
		}

		std::sort(hits.begin(), hits.end(), [](const ofxGgmlRagSearchHit & left, const ofxGgmlRagSearchHit & right) {
			if (left.score != right.score) {
				return left.score > right.score;
			}
			if (left.chunk.source != right.chunk.source) {
				return left.chunk.source < right.chunk.source;
			}
			return left.chunk.index < right.chunk.index;
		});

		if (hits.size() > options.topK) {
			hits.resize(options.topK);
		}
		return hits;
	}

	std::vector<ofxGgmlRagVectorSearchHit> searchEmbeddedChunks(
		const std::vector<float> & queryEmbedding,
		const std::vector<ofxGgmlRagEmbeddedChunk> & chunks,
		const ofxGgmlRagVectorSearchOptions & options) {
		std::vector<ofxGgmlRagVectorSearchHit> hits;
		if (queryEmbedding.empty() || chunks.empty() || options.topK == 0) {
			return hits;
		}

		for (const auto & embedded : chunks) {
			const auto score = static_cast<double>(cosineSimilarity(queryEmbedding, embedded.embedding));
			if (score < options.minScore) {
				continue;
			}
			ofxGgmlRagVectorSearchHit hit;
			hit.chunk = embedded.chunk;
			hit.score = score;
			hits.push_back(hit);
		}

		std::sort(hits.begin(), hits.end(), [](const ofxGgmlRagVectorSearchHit & left, const ofxGgmlRagVectorSearchHit & right) {
			if (left.score != right.score) {
				return left.score > right.score;
			}
			if (left.chunk.source != right.chunk.source) {
				return left.chunk.source < right.chunk.source;
			}
			return left.chunk.index < right.chunk.index;
		});

		if (hits.size() > options.topK) {
			hits.resize(options.topK);
		}
		return hits;
	}

	std::string cleanMarkdownForCitations(const std::string & markdown) {
		std::vector<std::string> lines;
		std::istringstream reader(markdown);
		std::string line;
		while (std::getline(reader, line)) {
			lines.push_back(line);
		}

		bool frontMatter = !lines.empty() && trim(lines.front()) == "---";
		if (frontMatter) {
			bool metadata = false;
			for (std::size_t i = 1; i < lines.size(); ++i) {
				const auto cleaned = trim(lines[i]);
				if (cleaned.empty()) {
					continue;
				}
				if (cleaned == "---") {
					frontMatter = metadata;
					break;
				}
				if (cleaned.find(':') != std::string::npos) {
					metadata = true;
				}
			}
		}

		std::ostringstream cleaned;
		bool inFrontMatter = frontMatter;
		bool previousWasBlank = true;
		bool sawBody = false;
		const std::unordered_set<std::string> stopHeadings = {
			"references", "citations", "bibliography", "external links", "navigation menu"
		};

		for (const auto & rawLine : lines) {
			const auto text = trim(rawLine);
			if (inFrontMatter) {
				if (text == "---" && sawBody) {
					inFrontMatter = false;
				}
				if (!sawBody) {
					sawBody = true;
				}
				continue;
			}
			if (!text.empty() && text.front() == '#') {
				auto heading = text;
				while (!heading.empty() && heading.front() == '#') {
					heading.erase(heading.begin());
				}
				if (stopHeadings.find(NormalizeEvidenceText(heading)) != stopHeadings.end()) {
					break;
				}
			}
			if (text == "contents" || text == "toc" || text == "menu" || text == "navigation") {
				continue;
			}
			if (text.empty()) {
				if (!previousWasBlank) {
					cleaned << "\n\n";
				}
				previousWasBlank = true;
				continue;
			}
			if (!previousWasBlank) {
				cleaned << "\n";
			}
			cleaned << text;
			previousWasBlank = false;
		}

		const auto output = trim(cleaned.str());
		return output.empty() ? trim(markdown) : output;
	}

	ofxGgmlRagCitationSearchInputMatch detectCitationIntent(
		const std::string & input,
		const ofxGgmlRagCitationSearchInputSettings & settings) {
		ofxGgmlRagCitationSearchInputMatch match;
		const auto cleaned = trim(input);
		const auto lowered = LowerAscii(cleaned);
		std::size_t bestFound = std::string::npos;
		std::string bestTrigger;
		std::string bestTopic;
		for (const auto & trigger : settings.triggerWords) {
			const auto normalizedTrigger = LowerAscii(trim(trigger));
			if (normalizedTrigger.empty()) {
				continue;
			}
			std::size_t searchFrom = 0;
			while (searchFrom < lowered.size()) {
				const auto found = lowered.find(normalizedTrigger, searchFrom);
				if (found == std::string::npos) {
					break;
				}
				const auto leftOk = found == 0 || !IsTokenChar(lowered[found - 1]);
				const auto right = found + normalizedTrigger.size();
				const auto rightOk = right >= lowered.size() || !IsTokenChar(lowered[right]);
				if (leftOk && rightOk) {
					const auto topic = StripCitationLeadIn(cleaned.substr(right));
					if (topic.size() >= settings.minTopicLength && found < bestFound) {
						bestFound = found;
						bestTrigger = normalizedTrigger;
						bestTopic = topic;
					}
				}
				searchFrom = found + normalizedTrigger.size();
			}
		}
		if (bestFound != std::string::npos) {
			match.matched = true;
			match.triggerWord = bestTrigger;
			match.topic = bestTopic;
		}
		return match;
	}

	std::vector<std::string> extractQuoteCandidates(const std::string & text) {
		std::vector<std::string> candidates;
		const auto cleaned = trim(text);
		if (cleaned.empty()) {
			return candidates;
		}
		auto appendCandidate = [&candidates](std::string candidate) {
			candidate = trim(candidate);
			if (candidate.size() < 24 || candidate.size() > 1100) {
				return;
			}
			candidates.push_back(candidate);
		};

		std::istringstream paragraphReader(cleaned);
		std::ostringstream paragraph;
		std::string line;
		auto flushParagraph = [&]() {
			const auto value = trim(paragraph.str());
			if (!value.empty()) {
				appendCandidate(value);
			}
			paragraph.str("");
			paragraph.clear();
		};
		while (std::getline(paragraphReader, line)) {
			const auto trimmedLine = trim(line);
			if (trimmedLine.empty()) {
				flushParagraph();
				continue;
			}
			if (!paragraph.str().empty()) {
				paragraph << " ";
			}
			paragraph << trimmedLine;
		}
		flushParagraph();

		std::vector<std::string> sentences;
		std::size_t start = 0;
		auto pushSentence = [&](std::size_t endExclusive) {
			if (endExclusive <= start) {
				return;
			}
			auto candidate = trim(cleaned.substr(start, endExclusive - start));
			start = endExclusive;
			if (candidate.size() >= 24 && candidate.size() <= 900) {
				sentences.push_back(candidate);
			}
		};
		for (std::size_t i = 0; i < cleaned.size(); ++i) {
			const auto ch = cleaned[i];
			if ((ch == '.' || ch == '!' || ch == '?' || ch == '\n') &&
				(i + 1 == cleaned.size() || IsWhitespace(cleaned[i + 1]))) {
				pushSentence(i + 1);
			}
		}
		pushSentence(cleaned.size());

		for (const auto & sentence : sentences) {
			appendCandidate(sentence);
		}
		for (std::size_t i = 0; i + 1 < sentences.size(); ++i) {
			appendCandidate(sentences[i] + " " + sentences[i + 1]);
		}
		for (std::size_t i = 0; i + 2 < sentences.size(); ++i) {
			appendCandidate(sentences[i] + " " + sentences[i + 1] + " " + sentences[i + 2]);
		}

		std::unordered_set<std::string> seen;
		std::vector<std::string> deduped;
		for (const auto & candidate : candidates) {
			const auto normalized = NormalizeEvidenceText(candidate);
			if (!normalized.empty() && seen.insert(normalized).second) {
				deduped.push_back(candidate);
			}
		}
		return deduped;
	}

	double sourceCredibility(const std::string & sourceUri) {
		const auto lower = LowerAscii(sourceUri);
		double credibility = 0.5;
		if (lower.find(".edu") != std::string::npos) {
			credibility += 0.3;
		}
		if (lower.find(".gov") != std::string::npos) {
			credibility += 0.3;
		}
		if (lower.find(".org") != std::string::npos) {
			credibility += 0.15;
		}
		if (lower.find("wikipedia.org") != std::string::npos) {
			credibility += 0.2;
		}
		if (lower.find("github.com") != std::string::npos) {
			credibility += 0.15;
		}
		if (lower.find("arxiv.org") != std::string::npos) {
			credibility += 0.25;
		}
		if (lower.find("ieee.org") != std::string::npos || lower.find("acm.org") != std::string::npos) {
			credibility += 0.25;
		}
		return ClampUnit(credibility);
	}

	double confidenceScore(
		const ofxGgmlRagCitationItem & item,
		bool isExactMatch,
		double relevanceScore,
		double sourceCredibilityValue) {
		double confidence = isExactMatch ? 0.4 : 0.15;
		confidence += ClampUnit(relevanceScore) * 0.35;
		confidence += ClampUnit(sourceCredibilityValue) * 0.25;
		const auto length = item.quote.size();
		if (length >= 30 && length <= 500) {
			confidence += 0.05;
		} else if (length > 500 && length <= 800) {
			confidence += 0.03;
		}
		if (!item.note.empty()) {
			confidence += 0.02;
		}
		return ClampUnit(confidence);
	}

	double sourceDiversityScore(const std::vector<ofxGgmlRagCitationItem> & citations) {
		if (citations.empty()) {
			return 0.0;
		}
		std::unordered_map<int, std::size_t> counts;
		for (const auto & citation : citations) {
			if (citation.sourceIndex > 0) {
				++counts[citation.sourceIndex];
			}
		}
		if (counts.empty()) {
			return 0.0;
		}
		const auto uniqueSources = counts.size();
		const auto total = citations.size();
		const auto diversity = static_cast<double>(uniqueSources) / static_cast<double>(std::max<std::size_t>(total, 1));
		double evenness = 0.0;
		if (uniqueSources > 1) {
			const auto ideal = static_cast<double>(total) / static_cast<double>(uniqueSources);
			double deviation = 0.0;
			for (const auto & pair : counts) {
				deviation += std::abs(static_cast<double>(pair.second) - ideal);
			}
			evenness = 1.0 - (deviation / (static_cast<double>(total) * static_cast<double>(uniqueSources)));
		}
		return ClampUnit((diversity * 0.6) + (evenness * 0.4));
	}

	ofxGgmlRagCitationSearchResult findCitations(
		const std::string & topic,
		const std::vector<ofxGgmlRagDocument> & documents,
		const ofxGgmlRagCitationSearchOptions & options) {
		ofxGgmlRagCitationSearchResult result;
		result.topic = trim(topic);
		if (result.topic.empty()) {
			result.error = "citation topic is empty";
			return result;
		}
		if (result.topic.size() < 3) {
			result.error = "citation topic is too short";
			return result;
		}
		if (documents.empty()) {
			result.error = "no citation documents";
			return result;
		}
		if (options.maxCitations == 0) {
			result.error = "maxCitations must be at least 1";
			return result;
		}
		if (options.minimumConfidence < 0.0 || options.minimumConfidence > 1.0) {
			result.error = "minimumConfidence must be between 0.0 and 1.0";
			return result;
		}

		struct RankedCitation {
			double score = 0.0;
			ofxGgmlRagCitationItem item;
		};
		const auto terms = TopicTerms(result.topic);
		std::vector<RankedCitation> ranked;
		for (std::size_t sourceIndex = 0; sourceIndex < documents.size(); ++sourceIndex) {
			const auto & document = documents[sourceIndex];
			const auto text = options.cleanMarkdown
				? cleanMarkdownForCitations(document.text)
				: trim(document.text);
			if (text.empty()) {
				continue;
			}
			for (const auto & quote : extractQuoteCandidates(text)) {
				const auto relevance = ScoreQuoteCandidate(quote, document.source, document.source, result.topic, terms);
				if (relevance <= 0.0) {
					continue;
				}
				RankedCitation candidate;
				candidate.score = relevance;
				candidate.item.quote = quote;
				candidate.item.note = "Exact local source span.";
				candidate.item.sourceLabel = document.source.empty()
					? ("Source " + std::to_string(sourceIndex + 1))
					: document.source;
				candidate.item.sourceUri = document.source;
				candidate.item.sourceIndex = static_cast<int>(sourceIndex + 1);
				candidate.item.isExactMatch = true;
				candidate.item.relevanceScore = ClampUnit(relevance / 10.0);
				candidate.item.sourceCredibility = sourceCredibility(document.source);
				candidate.item.confidenceScore = confidenceScore(
					candidate.item,
					true,
					candidate.item.relevanceScore,
					candidate.item.sourceCredibility);
				ranked.push_back(candidate);
			}
		}

		std::sort(ranked.begin(), ranked.end(), [](const RankedCitation & left, const RankedCitation & right) {
			if (left.item.confidenceScore != right.item.confidenceScore) {
				return left.item.confidenceScore > right.item.confidenceScore;
			}
			if (left.score != right.score) {
				return left.score > right.score;
			}
			return left.item.quote.size() < right.item.quote.size();
		});

		std::unordered_set<std::string> seen;
		std::unordered_map<int, std::size_t> sourceCounts;
		for (const auto & candidate : ranked) {
			if (candidate.item.confidenceScore < options.minimumConfidence) {
				continue;
			}
			if (sourceCounts[candidate.item.sourceIndex] >= options.maxQuotesPerSource) {
				continue;
			}
			const auto key = NormalizeEvidenceText(candidate.item.quote) + "\n---\n" + candidate.item.sourceUri;
			if (!seen.insert(key).second) {
				continue;
			}
			++sourceCounts[candidate.item.sourceIndex];
			result.citations.push_back(candidate.item);
			if (result.citations.size() >= options.maxCitations) {
				break;
			}
		}
		FinalizeCitationStats(result);
		result.success = !result.citations.empty();
		if (!result.success) {
			result.error = "no citation candidates";
		}
		return result;
	}

	ofxGgmlRagCitationSearchResult findCitationsFromInput(
		const std::string & input,
		const std::vector<ofxGgmlRagDocument> & documents,
		const ofxGgmlRagCitationSearchOptions & options,
		const ofxGgmlRagCitationSearchInputSettings & inputSettings) {
		const auto match = detectCitationIntent(input, inputSettings);
		ofxGgmlRagCitationSearchResult result;
		if (!match.matched) {
			result.error = "input did not request citations";
			return result;
		}
		result = findCitations(match.topic, documents, options);
		result.inputTriggerWord = match.triggerWord;
		return result;
	}

	std::string excerptForHit(
		const ofxGgmlRagSearchHit & hit,
		const ofxGgmlRagExcerptOptions & options) {
		const auto text = trim(hit.chunk.text);
		if (text.empty()) {
			return "";
		}

		std::size_t matchBegin = 0;
		std::size_t matchEnd = 0;
		const auto loweredText = LowerAscii(text);
		for (const auto & term : hit.matchedTerms) {
			const auto normalizedTerm = LowerAscii(trim(term));
			if (normalizedTerm.empty()) {
				continue;
			}
			const auto found = loweredText.find(normalizedTerm);
			if (found != std::string::npos) {
				matchBegin = found;
				matchEnd = found + normalizedTerm.size();
				break;
			}
		}

		std::size_t begin = 0;
		std::size_t end = text.size();
		if (matchEnd > matchBegin) {
			begin = matchBegin > options.contextChars ? matchBegin - options.contextChars : 0;
			end = std::min(text.size(), matchEnd + options.contextChars);
		} else {
			end = std::min(text.size(), options.contextChars * 2);
		}

		while (begin < end && IsWhitespace(text[begin])) {
			++begin;
		}
		while (end > begin && IsWhitespace(text[end - 1])) {
			--end;
		}

		auto excerpt = text.substr(begin, end - begin);
		if (options.includeEllipses && begin > 0) {
			excerpt = "..." + excerpt;
		}
		if (options.includeEllipses && end < text.size()) {
			excerpt += "...";
		}
		return excerpt;
	}

	bool hasCitations(const ofxGgmlRagResult & result) {
		return !result.citations.empty() || !result.references.empty();
	}

	ofxGgmlRagCitation citationFromChunk(const ofxGgmlRagChunk & chunk, const std::string & label) {
		ofxGgmlRagCitation citation;
		citation.source = trim(chunk.source);
		citation.label = trim(label);
		if (citation.label.empty()) {
			if (!citation.source.empty()) {
				std::ostringstream generatedLabel;
				generatedLabel << citation.source << "#" << chunk.index;
				citation.label = generatedLabel.str();
			} else {
				std::ostringstream generatedLabel;
				generatedLabel << "chunk #" << chunk.index;
				citation.label = generatedLabel.str();
			}
		}
		citation.start = chunk.start;
		citation.end = chunk.end;
		return citation;
	}

	ofxGgmlRagContext contextFromHits(
		const std::string & query,
		const std::vector<ofxGgmlRagSearchHit> & hits,
		const ofxGgmlRagContextOptions & options) {
		ofxGgmlRagContext context;
		if (hits.empty() || options.maxChars == 0) {
			context.truncated = !hits.empty();
			return context;
		}

		const auto normalizedQuery = trim(query);
		if (options.includeQuery && !normalizedQuery.empty()) {
			std::ostringstream header;
			header << "Query: " << normalizedQuery << "\n\n";
			if (!AppendWithinLimit(context.text, header.str(), options.maxChars)) {
				context.truncated = true;
				return context;
			}
		}

		for (const auto & hit : hits) {
			const auto citation = citationFromChunk(hit.chunk);
			std::ostringstream block;
			block << "[" << (context.includedHitCount + 1) << "] " << formatCitation(citation);
			if (options.includeScores) {
				block << " score=" << hit.score;
			}
			if (!hit.matchedTerms.empty()) {
				block << " terms=";
				for (std::size_t i = 0; i < hit.matchedTerms.size(); ++i) {
					if (i > 0) {
						block << ",";
					}
					block << hit.matchedTerms[i];
				}
			}
			block << "\n" << trim(hit.chunk.text) << "\n\n";

			if (!AppendWithinLimit(context.text, block.str(), options.maxChars)) {
				context.truncated = true;
				break;
			}
			context.citations.push_back(citation);
			++context.includedHitCount;
		}

		return context;
	}

	ofxGgmlRagResult resultFromHits(const std::string & query, const std::vector<ofxGgmlRagSearchHit> & hits) {
		ofxGgmlRagResult result;
		const auto normalizedQuery = trim(query);
		if (hits.empty()) {
			result.error = "no matching chunks";
			return result;
		}

		result.success = true;
		std::ostringstream text;
		text << "Found " << hits.size() << " matching chunk";
		if (hits.size() != 1) {
			text << "s";
		}
		if (!normalizedQuery.empty()) {
			text << " for \"" << normalizedQuery << "\"";
		}
		text << ".";
		result.text = text.str();

		for (const auto & hit : hits) {
			const auto citation = citationFromChunk(hit.chunk);
			result.citations.push_back(citation);
		}
		result.references = referencesFromCitations(result.citations);

		return result;
	}

	ofxGgmlRagPrompt buildPrompt(
		const std::string & query,
		const ofxGgmlRagRetrieval & retrieval,
		const ofxGgmlRagPromptOptions & options) {
		ofxGgmlRagPrompt prompt;
		prompt.question = trim(query);
		prompt.systemInstruction = trim(options.systemInstruction);
		prompt.context = trim(retrieval.context.text);
		prompt.citations = retrieval.context.citations.empty() ? retrieval.result.citations : retrieval.context.citations;
		prompt.references = retrieval.result.references;
		prompt.truncated = retrieval.context.truncated;

		if (!retrieval) {
			prompt.error = retrieval.result.error.empty() ? "retrieval has no cited context" : retrieval.result.error;
			return prompt;
		}
		if (prompt.question.empty()) {
			prompt.error = "query is required";
			return prompt;
		}
		if (prompt.context.empty()) {
			prompt.error = "retrieval context is empty";
			return prompt;
		}

		std::ostringstream text;
		if (!prompt.systemInstruction.empty()) {
			text << prompt.systemInstruction << "\n\n";
		}
		const auto contextHeading = trim(options.contextHeading);
		text << (contextHeading.empty() ? "Context" : contextHeading) << ":\n";
		text << prompt.context;
		if (!prompt.context.empty() && prompt.context.back() != '\n') {
			text << "\n";
		}

		if (options.includeReferences && !prompt.references.empty()) {
			text << "\nReferences:\n" << formatReferences(prompt.references) << "\n";
		}

		const auto questionHeading = trim(options.questionHeading);
		text << "\n" << (questionHeading.empty() ? "Question" : questionHeading) << ":\n";
		text << prompt.question << "\n\n";

		const auto answerHeading = trim(options.answerHeading);
		text << (answerHeading.empty() ? "Answer" : answerHeading) << ":\n";

		prompt.prompt = text.str();
		if (options.maxPromptChars > 0 && prompt.prompt.size() > options.maxPromptChars) {
			prompt.prompt.resize(options.maxPromptChars);
			prompt.truncated = true;
		}
		prompt.success = true;
		return prompt;
	}

	ofxGgmlRagAnswer draftAnswer(
		const std::string & query,
		const ofxGgmlRagRetrieval & retrieval,
		const ofxGgmlRagAnswerOptions & options) {
		ofxGgmlRagAnswer answer;
		answer.question = trim(query);

		if (!retrieval) {
			answer.error = retrieval.result.error.empty() ? "retrieval has no cited evidence" : retrieval.result.error;
			return answer;
		}
		if (answer.question.empty()) {
			answer.error = "query is required";
			return answer;
		}
		if (retrieval.hits.empty()) {
			answer.error = "no matching chunks";
			return answer;
		}
		if (options.maxHits == 0) {
			answer.error = "answer hit budget is zero";
			return answer;
		}
		if (options.maxAnswerChars == 0) {
			answer.error = "answer character budget is zero";
			return answer;
		}

		std::ostringstream header;
		header << "Extractive answer draft for: " << answer.question << "\n";
		header << "This is cited retrieval evidence, not a model-generated answer.\n\n";
		std::string text = header.str();
		if (text.size() > options.maxAnswerChars) {
			answer.error = "answer character budget is too small";
			answer.truncated = true;
			return answer;
		}

		const auto maxHits = std::min(options.maxHits, retrieval.hits.size());
		for (std::size_t i = 0; i < maxHits; ++i) {
			const auto & hit = retrieval.hits[i];
			const auto citation = citationFromChunk(hit.chunk);
			auto excerpt = excerptForHit(hit, options.excerpt);
			if (excerpt.empty()) {
				excerpt = trim(hit.chunk.text);
			}
			if (excerpt.empty()) {
				continue;
			}

			std::ostringstream line;
			line << "[" << (answer.citations.size() + 1) << "] " << excerpt << "\n";
			if (!AppendWithinLimit(text, line.str(), options.maxAnswerChars)) {
				answer.truncated = true;
				break;
			}
			answer.citations.push_back(citation);
		}

		if (answer.citations.empty()) {
			answer.error = answer.truncated ? "answer character budget is too small" : "retrieval evidence is empty";
			return answer;
		}

		answer.references = referencesFromCitations(answer.citations);
		if (options.includeReferences && !answer.references.empty()) {
			const auto references = "\nReferences:\n" + formatReferences(answer.references);
			if (!AppendWithinLimit(text, references, options.maxAnswerChars)) {
				answer.truncated = true;
			}
		}

		answer.text = text;
		answer.success = true;
		return answer;
	}

	ofxGgmlRagRetrieval retrieve(
		const ofxGgmlRagRequest & request,
		const std::vector<ofxGgmlRagDocument> & documents,
		const ofxGgmlRagRetrievalOptions & options) {
		ofxGgmlRagRetrieval retrieval;
		retrieval.stats.documentCount = documents.size();
		retrieval.validation = validate(request);
		if (!retrieval.validation) {
			retrieval.result.error = "invalid request";
			return retrieval;
		}
		if (documents.empty()) {
			retrieval.result.error = "no documents";
			return retrieval;
		}

		const auto requestTags = normalizedTags(request);
		const auto excludedSourceRoots = NormalizeTags(options.search.excludedSourceRoots);
		for (const auto & document : documents) {
			if (!sourceMatchesRoot(request.sourceRoot, document.source)) {
				++retrieval.stats.skippedDocumentCount;
				continue;
			}
			if (MatchesAnySourceRoot(excludedSourceRoots, document.source)) {
				++retrieval.stats.skippedDocumentCount;
				continue;
			}
			++retrieval.stats.scopedDocumentCount;

			auto documentTags = NormalizeTags(document.tags);
			for (const auto & tag : requestTags) {
				if (std::find(documentTags.begin(), documentTags.end(), tag) == documentTags.end()) {
					documentTags.push_back(tag);
				}
			}

			auto chunks = chunkText(document.source, document.text, options.chunk, documentTags);
			for (auto & chunk : chunks) {
				chunk.qualityHint = document.qualityHint;
			}
			retrieval.chunks.insert(retrieval.chunks.end(), chunks.begin(), chunks.end());
		}
		if (retrieval.chunks.empty()) {
			retrieval.result.error = "no documents in sourceRoot";
			return retrieval;
		}
		retrieval.stats.chunkCount = retrieval.chunks.size();

		retrieval.hits = searchChunks(request.query, retrieval.chunks, options.search);
		retrieval.context = contextFromHits(request.query, retrieval.hits, options.context);
		retrieval.result = resultFromHits(request.query, retrieval.hits);
		retrieval.stats.hitCount = retrieval.hits.size();
		retrieval.stats.citationCount = retrieval.result.citations.size();
		retrieval.stats.contextTruncated = retrieval.context.truncated;
		return retrieval;
	}

	ofxGgmlRagRetrieval retrieveTextCorpus(
		const ofxGgmlRagRequest & request,
		const ofxGgmlRagCorpusOptions & corpusOptions,
		const ofxGgmlRagRetrievalOptions & retrievalOptions) {
		ofxGgmlRagRetrieval retrieval;
		retrieval.validation = validate(request);
		if (!retrieval.validation) {
			retrieval.result.error = "invalid request";
			return retrieval;
		}

		const auto corpus = loadTextCorpus(request.sourceRoot, corpusOptions);
		retrieval.stats.documentCount = corpus.stats.loadedDocumentCount;
		if (!corpus) {
			retrieval.result.error = corpus.error.empty() ? "corpus load failed" : corpus.error;
			for (const auto & warning : corpus.warnings) {
				retrieval.validation.warnings.push_back(warning);
			}
			return retrieval;
		}

		auto scopedRequest = request;
		scopedRequest.sourceRoot = corpus.sourceRoot;
		retrieval = retrieve(scopedRequest, corpus.documents, retrievalOptions);
		for (const auto & warning : corpus.warnings) {
			retrieval.validation.warnings.push_back(warning);
		}
		return retrieval;
	}

	std::string formatCitation(const ofxGgmlRagCitation & citation) {
		std::ostringstream text;
		const auto label = trim(citation.label);
		const auto source = trim(citation.source);
		const auto url = trim(citation.url);

		if (!label.empty()) {
			text << label;
		} else if (!source.empty()) {
			text << source;
		} else if (!url.empty()) {
			text << url;
		} else {
			text << "citation";
		}

		if (!source.empty() && source != label) {
			text << " (" << source;
			if (citation.end > citation.start) {
				text << ":" << citation.start << "-" << citation.end;
			}
			text << ")";
		} else if (citation.end > citation.start) {
			text << " (" << citation.start << "-" << citation.end << ")";
		}

		if (!url.empty()) {
			text << " <" << url << ">";
		}

		return text.str();
	}

	std::vector<std::string> referencesFromCitations(const std::vector<ofxGgmlRagCitation> & citations) {
		std::vector<std::string> references;
		for (const auto & citation : citations) {
			const auto formatted = formatCitation(citation);
			if (std::find(references.begin(), references.end(), formatted) == references.end()) {
				references.push_back(formatted);
			}
		}
		return references;
	}

	std::string formatReferences(const std::vector<std::string> & references) {
		std::ostringstream text;
		std::size_t emitted = 0;
		for (const auto & value : references) {
			const auto reference = trim(value);
			if (reference.empty()) {
				continue;
			}
			if (emitted > 0) {
				text << "\n";
			}
			++emitted;
			text << "[" << emitted << "] " << reference;
		}
		return text.str();
	}

	std::string formatHit(
		const ofxGgmlRagSearchHit & hit,
		const ofxGgmlRagExcerptOptions & options) {
		std::ostringstream text;
		text << formatCitation(citationFromChunk(hit.chunk)) << " score=" << hit.score;
		if (!hit.matchedTerms.empty()) {
			text << " terms=";
			for (std::size_t i = 0; i < hit.matchedTerms.size(); ++i) {
				if (i > 0) {
					text << ",";
				}
				text << hit.matchedTerms[i];
			}
		}
		const auto excerpt = excerptForHit(hit, options);
		if (!excerpt.empty()) {
			text << " :: " << excerpt;
		}
		return text.str();
	}

	std::string formatStats(const ofxGgmlRagRetrievalStats & stats) {
		std::ostringstream text;
		text << "documents=" << stats.documentCount
			<< " scoped=" << stats.scopedDocumentCount
			<< " skipped=" << stats.skippedDocumentCount
			<< " chunks=" << stats.chunkCount
			<< " hits=" << stats.hitCount
			<< " citations=" << stats.citationCount
			<< " contextTruncated=" << (stats.contextTruncated ? "true" : "false");
		return text.str();
	}

	std::string formatRetrieval(
		const ofxGgmlRagRetrieval & retrieval,
		const ofxGgmlRagReportOptions & options) {
		std::ostringstream report;
		report << summarize(retrieval) << "\n";
		if (retrieval.hits.empty()) {
			return report.str();
		}

		const auto hitCount = std::min(options.maxHits, retrieval.hits.size());
		for (std::size_t i = 0; i < hitCount; ++i) {
			report << "hit[" << (i + 1) << "] " << formatHit(retrieval.hits[i], options.excerpt) << "\n";
		}
		if (hitCount < retrieval.hits.size()) {
			report << "hitsTruncated=" << (retrieval.hits.size() - hitCount) << "\n";
		}

		if (options.includeReferences && !retrieval.result.references.empty()) {
			report << "references:\n" << formatReferences(retrieval.result.references) << "\n";
		}

		if (options.includeContext && retrieval.context) {
			report << "context:\n" << retrieval.context.text;
			if (!retrieval.context.text.empty() && retrieval.context.text.back() != '\n') {
				report << "\n";
			}
		}

		return report.str();
	}

	std::string formatRetrievalJson(
		const ofxGgmlRagRetrieval & retrieval,
		const ofxGgmlRagReportOptions & options) {
		std::ostringstream json;
		const auto hitCount = std::min(options.maxHits, retrieval.hits.size());

		json << "{";
		json << "\"success\":" << (retrieval.result.success ? "true" : "false");
		json << ",\"summary\":";
		AppendJsonString(json, summarize(retrieval));
		json << ",\"error\":";
		AppendJsonString(json, retrieval.result.error);
		json << ",\"stats\":{";
		json << "\"documents\":" << retrieval.stats.documentCount;
		json << ",\"scoped\":" << retrieval.stats.scopedDocumentCount;
		json << ",\"skipped\":" << retrieval.stats.skippedDocumentCount;
		json << ",\"chunks\":" << retrieval.stats.chunkCount;
		json << ",\"hits\":" << retrieval.stats.hitCount;
		json << ",\"citations\":" << retrieval.stats.citationCount;
		json << ",\"contextTruncated\":" << (retrieval.stats.contextTruncated ? "true" : "false");
		json << "}";

		json << ",\"hits\":[";
		for (std::size_t i = 0; i < hitCount; ++i) {
			if (i > 0) {
				json << ",";
			}
			const auto & hit = retrieval.hits[i];
			const auto citation = citationFromChunk(hit.chunk);
			json << "{";
			json << "\"source\":";
			AppendJsonString(json, hit.chunk.source);
			json << ",\"index\":" << hit.chunk.index;
			json << ",\"start\":" << hit.chunk.start;
			json << ",\"end\":" << hit.chunk.end;
			json << ",\"tags\":";
			AppendJsonStringArray(json, hit.chunk.tags);
			json << ",\"score\":" << hit.score;
			json << ",\"matchedTerms\":";
			AppendJsonStringArray(json, hit.matchedTerms);
			json << ",\"excerpt\":";
			AppendJsonString(json, excerptForHit(hit, options.excerpt));
			json << ",\"citation\":";
			AppendJsonString(json, formatCitation(citation));
			json << "}";
		}
		json << "]";
		json << ",\"hitsTruncated\":" << (retrieval.hits.size() - hitCount);
		json << ",\"citations\":";
		AppendJsonCitations(json, retrieval.result.citations);

		if (options.includeReferences) {
			json << ",\"references\":";
			AppendJsonStringArray(json, retrieval.result.references);
		}
		if (options.includeContext) {
			json << ",\"context\":";
			AppendJsonString(json, retrieval.context.text);
		}

		json << "}";
		const auto compact = json.str();
		if (options.prettyJson) {
			return PrettyPrintJson(compact);
		}
		return compact;
	}

	std::string summarize(const ofxGgmlRagResult & result) {
		if (!result.success) {
			const auto error = trim(result.error);
			if (!error.empty()) {
				return "rag result: error: " + error;
			}
			return "rag result: error";
		}

		std::ostringstream summary;
		summary << "rag result: ok";
		const auto text = trim(result.text);
		if (!text.empty()) {
			summary << ": " << text;
		}
		const auto citationCount = result.citations.size() + result.references.size();
		if (citationCount > 0) {
			summary << " (" << citationCount << " citation";
			if (citationCount != 1) {
				summary << "s";
			}
			summary << ")";
		}
		return summary.str();
	}

	std::string summarize(const ofxGgmlRagRetrieval & retrieval) {
		std::ostringstream summary;
		summary << summarize(retrieval.result) << "; " << formatStats(retrieval.stats);
		if (retrieval.validation.errors.size() > 0) {
			summary << "; validationErrors=" << retrieval.validation.errors.size();
		}
		if (retrieval.validation.warnings.size() > 0) {
			summary << "; validationWarnings=" << retrieval.validation.warnings.size();
		}
		return summary.str();
	}
}
