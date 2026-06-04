#include "ofxGgmlRagUtils.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <system_error>

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
			for (const auto & term : queryTerms) {
				if (std::find(chunkTerms.begin(), chunkTerms.end(), term) != chunkTerms.end()) {
					hit.matchedTerms.push_back(term);
				}
			}
			if (hit.matchedTerms.empty()) {
				continue;
			}
			if (hit.matchedTerms.size() < minMatchedTerms) {
				continue;
			}

			hit.score = static_cast<double>(hit.matchedTerms.size()) / static_cast<double>(queryTerms.size());
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
