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
	double qualityHint = 0.0;
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

struct ofxGgmlRagHtmlOptions {
	std::vector<std::string> tags = { "web" };
	std::size_t maxHtmlBytes = 1024 * 1024;
	bool includeTitleInText = true;
	bool respectRobotsMeta = true;
	double qualityHint = 0.0;
};

struct ofxGgmlRagHtmlRobotsPolicy {
	bool noindex = false;
	bool nofollow = false;
};

struct ofxGgmlRagHtmlDocument {
	bool success = false;
	std::string sourceUrl;
	std::string title;
	std::string error;
	ofxGgmlRagDocument document;

	explicit operator bool() const {
		return success;
	}
};

struct ofxGgmlRagHtmlLinkOptions {
	bool sameOriginOnly = true;
	bool includeFragments = false;
	bool respectRobotsMeta = true;
	bool respectRobotsTxt = true;
	std::string robotsTxt;
	std::string robotsTxtUserAgent = "*";
	std::size_t maxLinks = 64;
	std::vector<std::string> allowedUrlPrefixes;
	std::vector<std::string> excludedUrlPrefixes;
};

struct ofxGgmlRagHtmlLinkDecision {
	bool accepted = false;
	std::string href;
	std::string url;
	std::string reason;
};

struct ofxGgmlRagHtmlLinkFrontierStats {
	std::size_t discoveredLinkCount = 0;
	std::size_t acceptedLinkCount = 0;
	std::size_t skippedLinkCount = 0;
	std::size_t duplicateLinkCount = 0;
	std::size_t invalidLinkCount = 0;
	std::size_t offOriginLinkCount = 0;
	std::size_t scopeBlockedLinkCount = 0;
	std::size_t robotsBlockedLinkCount = 0;
	bool robotsMetaNofollow = false;
	bool maxLinksReached = false;
};

struct ofxGgmlRagHtmlLinkFrontier {
	bool success = false;
	std::string sourceUrl;
	std::string error;
	ofxGgmlRagHtmlLinkFrontierStats stats;
	std::vector<std::string> links;
	std::vector<ofxGgmlRagHtmlLinkDecision> decisions;

	explicit operator bool() const {
		return success;
	}
};

struct ofxGgmlRagHtmlPage {
	std::string sourceUrl;
	std::string html;
};

struct ofxGgmlRagHtmlBatchOptions {
	ofxGgmlRagHtmlOptions document;
	ofxGgmlRagHtmlLinkOptions links;
	std::size_t maxPages = 32;
	bool collectLinks = true;
};

struct ofxGgmlRagHtmlBatchStats {
	std::size_t pageCount = 0;
	std::size_t loadedDocumentCount = 0;
	std::size_t skippedPageCount = 0;
	std::size_t linkCount = 0;
};

struct ofxGgmlRagHtmlBatch {
	bool success = false;
	std::string error;
	std::vector<std::string> warnings;
	ofxGgmlRagHtmlBatchStats stats;
	std::vector<ofxGgmlRagDocument> documents;
	std::vector<std::string> links;

	explicit operator bool() const {
		return success;
	}
};

struct ofxGgmlRagRobotsTxtRule {
	std::string pathPrefix;
	bool allow = false;
};

struct ofxGgmlRagRobotsTxtPolicy {
	bool success = false;
	std::string userAgent;
	std::vector<ofxGgmlRagRobotsTxtRule> rules;

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
	double qualityHint = 0.0;
};

struct ofxGgmlRagSearchOptions {
	std::size_t topK = 5;
	std::size_t minMatchedTerms = 1;
	double minScore = 0.0;
	double phraseBoost = 0.0;
	double qualityWeight = 0.0;
	bool ignoreStopWords = true;
	bool allowQueryRefinement = true;
	std::size_t maxRefinementQueries = 1;
	std::vector<std::string> queryVariants;
	std::vector<std::string> requiredTags;
	std::vector<std::string> excludedTags;
	std::vector<std::string> excludedSourceRoots;
};

struct ofxGgmlRagSearchHit {
	ofxGgmlRagChunk chunk;
	double score = 0.0;
	double lexicalScore = 0.0;
	double qualityScore = 0.0;
	std::vector<std::string> matchedTerms;
};

struct ofxGgmlRagEmbeddedChunk {
	ofxGgmlRagChunk chunk;
	std::vector<float> embedding;
};

struct ofxGgmlRagVectorSearchOptions {
	std::size_t topK = 5;
	double minScore = 0.0;
};

struct ofxGgmlRagVectorSearchHit {
	ofxGgmlRagChunk chunk;
	double score = 0.0;
};

struct ofxGgmlRagCitationSearchInputSettings {
	std::vector<std::string> triggerWords = {
		"search",
		"find",
		"query",
		"citation",
		"citations",
		"cite",
		"source",
		"sources",
		"quote",
		"quotes",
		"evidence"
	};
	std::size_t minTopicLength = 3;
};

struct ofxGgmlRagCitationSearchInputMatch {
	bool matched = false;
	std::string triggerWord;
	std::string topic;
};

struct ofxGgmlRagCitationItem {
	std::string quote;
	std::string note;
	std::string sourceLabel;
	std::string sourceUri;
	int sourceIndex = -1;
	double confidenceScore = 0.0;
	bool isExactMatch = false;
	double relevanceScore = 0.0;
	double sourceCredibility = 0.0;
};

struct ofxGgmlRagCitationSearchOptions {
	std::size_t maxCitations = 5;
	double minimumConfidence = 0.0;
	std::size_t maxQuotesPerSource = 4;
	bool cleanMarkdown = true;
};

struct ofxGgmlRagCitationSearchResult {
	bool success = false;
	std::string error;
	std::string topic;
	std::string inputTriggerWord;
	std::vector<ofxGgmlRagCitationItem> citations;
	double sourceDiversityScore = 0.0;
	double averageConfidence = 0.0;

	explicit operator bool() const {
		return success;
	}
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

struct ofxGgmlRagAnswerOptions {
	std::size_t maxHits = 3;
	std::size_t maxAnswerChars = 1200;
	bool includeReferences = true;
	ofxGgmlRagExcerptOptions excerpt;
};

struct ofxGgmlRagAnswer {
	bool success = false;
	std::string error;
	std::string question;
	std::string text;
	std::vector<std::string> references;
	std::vector<ofxGgmlRagCitation> citations;
	bool extractive = true;
	bool truncated = false;

	explicit operator bool() const {
		return success;
	}
};

struct ofxGgmlRagRetrievalOptions {
	ofxGgmlRagChunkOptions chunk;
	ofxGgmlRagSearchOptions search;
	ofxGgmlRagContextOptions context;
	bool enableCache = true;
	std::size_t maxCacheEntries = 64;
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
	bool cacheHit = false;
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
