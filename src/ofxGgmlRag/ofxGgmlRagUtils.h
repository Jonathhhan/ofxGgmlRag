#pragma once

#include "ofxGgmlRagTypes.h"

#include <string>
#include <vector>

namespace ofxGgmlRagUtils {
	std::string trim(const std::string & text);
	std::vector<std::string> tokenize(const std::string & text);
	bool sourceMatchesRoot(const std::string & sourceRoot, const std::string & source);
	bool hasInput(const ofxGgmlRagRequest & request);
	ofxGgmlRagValidation validate(const ofxGgmlRagRequest & request);
	std::vector<std::string> normalizedTags(const ofxGgmlRagRequest & request);
	std::string describe(const ofxGgmlRagRequest & request);
	ofxGgmlRagCorpus loadTextCorpus(
		const std::string & sourceRoot,
		const ofxGgmlRagCorpusOptions & options = ofxGgmlRagCorpusOptions());
	std::vector<ofxGgmlRagChunk> chunkText(
		const std::string & source,
		const std::string & text,
		const ofxGgmlRagChunkOptions & options = ofxGgmlRagChunkOptions(),
		const std::vector<std::string> & tags = std::vector<std::string>());
	std::vector<ofxGgmlRagSearchHit> searchChunks(
		const std::string & query,
		const std::vector<ofxGgmlRagChunk> & chunks,
		const ofxGgmlRagSearchOptions & options = ofxGgmlRagSearchOptions());
	std::string excerptForHit(
		const ofxGgmlRagSearchHit & hit,
		const ofxGgmlRagExcerptOptions & options = ofxGgmlRagExcerptOptions());
	bool hasCitations(const ofxGgmlRagResult & result);
	ofxGgmlRagCitation citationFromChunk(const ofxGgmlRagChunk & chunk, const std::string & label = "");
	ofxGgmlRagContext contextFromHits(
		const std::string & query,
		const std::vector<ofxGgmlRagSearchHit> & hits,
		const ofxGgmlRagContextOptions & options = ofxGgmlRagContextOptions());
	ofxGgmlRagResult resultFromHits(const std::string & query, const std::vector<ofxGgmlRagSearchHit> & hits);
	ofxGgmlRagPrompt buildPrompt(
		const std::string & query,
		const ofxGgmlRagRetrieval & retrieval,
		const ofxGgmlRagPromptOptions & options = ofxGgmlRagPromptOptions());
	ofxGgmlRagRetrieval retrieve(
		const ofxGgmlRagRequest & request,
		const std::vector<ofxGgmlRagDocument> & documents,
		const ofxGgmlRagRetrievalOptions & options = ofxGgmlRagRetrievalOptions());
	ofxGgmlRagRetrieval retrieveTextCorpus(
		const ofxGgmlRagRequest & request,
		const ofxGgmlRagCorpusOptions & corpusOptions = ofxGgmlRagCorpusOptions(),
		const ofxGgmlRagRetrievalOptions & retrievalOptions = ofxGgmlRagRetrievalOptions());
	std::string formatCitation(const ofxGgmlRagCitation & citation);
	std::vector<std::string> referencesFromCitations(const std::vector<ofxGgmlRagCitation> & citations);
	std::string formatReferences(const std::vector<std::string> & references);
	std::string formatHit(
		const ofxGgmlRagSearchHit & hit,
		const ofxGgmlRagExcerptOptions & options = ofxGgmlRagExcerptOptions());
	std::string formatStats(const ofxGgmlRagRetrievalStats & stats);
	std::string formatRetrieval(
		const ofxGgmlRagRetrieval & retrieval,
		const ofxGgmlRagReportOptions & options = ofxGgmlRagReportOptions());
	std::string formatRetrievalJson(
		const ofxGgmlRagRetrieval & retrieval,
		const ofxGgmlRagReportOptions & options = ofxGgmlRagReportOptions());
	std::string summarize(const ofxGgmlRagResult & result);
	std::string summarize(const ofxGgmlRagRetrieval & retrieval);
}
