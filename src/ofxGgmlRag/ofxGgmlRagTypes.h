#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct ofxGgmlRagRequest {
	std::string query;
	std::string sourceRoot;
	std::vector<std::string> tags;
};

struct ofxGgmlRagDocument {
	std::string source;
	std::string text;
	std::vector<std::string> tags;
};

struct ofxGgmlRagCorpusOptions {
	std::vector<std::string> extensions = { ".md", ".txt" };
	std::vector<std::string> tags;
	std::size_t maxFileBytes = 1024 * 1024;
	bool recursive = true;
	bool includeEmptyFiles = false;
};

struct ofxGgmlRagCorpusStats {
	std::size_t discoveredFileCount = 0;
	std::size_t loadedDocumentCount = 0;
	std::size_t skippedFileCount = 0;
	std::size_t skippedByteCount = 0;
};

struct ofxGgmlRagCorpus {
	bool success = false;
	std::string sourceRoot;
	std::string error;
	std::vector<std::string> warnings;
	ofxGgmlRagCorpusStats stats;
	std::vector<ofxGgmlRagDocument> documents;

	explicit operator bool() const {
		return success;
	}
};

struct ofxGgmlRagValidation {
	bool valid = false;
	std::vector<std::string> errors;
	std::vector<std::string> warnings;

	explicit operator bool() const {
		return valid;
	}
};

struct ofxGgmlRagCitation {
	std::string source;
	std::string label;
	std::string url;
	std::size_t start = 0;
	std::size_t end = 0;
};

struct ofxGgmlRagChunkOptions {
	std::size_t maxChars = 800;
	std::size_t overlapChars = 80;
};

struct ofxGgmlRagChunk {
	std::string source;
	std::string text;
	std::size_t index = 0;
	std::size_t start = 0;
	std::size_t end = 0;
	std::vector<std::string> tags;
};

struct ofxGgmlRagSearchOptions {
	std::size_t topK = 5;
	std::size_t minMatchedTerms = 1;
	double minScore = 0.0;
	double phraseBoost = 0.0;
	std::vector<std::string> requiredTags;
	std::vector<std::string> excludedTags;
	std::vector<std::string> excludedSourceRoots;
};

struct ofxGgmlRagSearchHit {
	ofxGgmlRagChunk chunk;
	double score = 0.0;
	std::vector<std::string> matchedTerms;
};

struct ofxGgmlRagExcerptOptions {
	std::size_t contextChars = 80;
	bool includeEllipses = true;
};

struct ofxGgmlRagContextOptions {
	std::size_t maxChars = 4000;
	bool includeQuery = true;
	bool includeScores = true;
};

struct ofxGgmlRagContext {
	std::string text;
	std::vector<ofxGgmlRagCitation> citations;
	std::size_t includedHitCount = 0;
	bool truncated = false;

	explicit operator bool() const {
		return !text.empty();
	}
};

struct ofxGgmlRagPromptOptions {
	std::string systemInstruction = "Answer the question using only the cited context. If the context is insufficient, say so.";
	std::string contextHeading = "Cited context";
	std::string questionHeading = "Question";
	std::string answerHeading = "Answer";
	bool includeReferences = true;
	std::size_t maxPromptChars = 6000;
};

struct ofxGgmlRagPrompt {
	bool success = false;
	std::string error;
	std::string question;
	std::string systemInstruction;
	std::string context;
	std::string prompt;
	std::vector<std::string> references;
	std::vector<ofxGgmlRagCitation> citations;
	bool truncated = false;

	explicit operator bool() const {
		return success;
	}
};

struct ofxGgmlRagRetrievalOptions {
	ofxGgmlRagChunkOptions chunk;
	ofxGgmlRagSearchOptions search;
	ofxGgmlRagContextOptions context;
};

struct ofxGgmlRagReportOptions {
	std::size_t maxHits = 5;
	bool includeReferences = true;
	bool includeContext = false;
	bool prettyJson = false;
	ofxGgmlRagExcerptOptions excerpt;
};

struct ofxGgmlRagRetrievalStats {
	std::size_t documentCount = 0;
	std::size_t scopedDocumentCount = 0;
	std::size_t skippedDocumentCount = 0;
	std::size_t chunkCount = 0;
	std::size_t hitCount = 0;
	std::size_t citationCount = 0;
	bool contextTruncated = false;
};

struct ofxGgmlRagResult {
	bool success = false;
	std::string text;
	std::string error;
	std::vector<std::string> references;
	std::vector<ofxGgmlRagCitation> citations;

	explicit operator bool() const {
		return success;
	}
};

struct ofxGgmlRagRetrieval {
	ofxGgmlRagValidation validation;
	ofxGgmlRagRetrievalStats stats;
	std::vector<ofxGgmlRagChunk> chunks;
	std::vector<ofxGgmlRagSearchHit> hits;
	ofxGgmlRagContext context;
	ofxGgmlRagResult result;

	explicit operator bool() const {
		return result.success;
	}
};
