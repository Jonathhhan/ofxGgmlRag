#pragma once

#include "ofxGgmlRagVersion.h"
#include "ofxGgmlRag/ofxGgmlRagTypes.h"
#include "ofxGgmlRag/ofxGgmlRagUtils.h"

#include <functional>
#include <string>
#include <vector>

using ofxGgmlRagPromptGenerator = std::function<std::string(const ofxGgmlRagPrompt & prompt)>;

class ofxGgmlRag {
public:
	void clear();

	void setRequest(const ofxGgmlRagRequest & request);
	void setQuery(const std::string & query);
	void setSourceRoot(const std::string & sourceRoot);
	void setTags(const std::vector<std::string> & tags);
	const ofxGgmlRagRequest & getRequest() const;

	void setCorpusOptions(const ofxGgmlRagCorpusOptions & options);
	ofxGgmlRagCorpusOptions & getCorpusOptions();
	const ofxGgmlRagCorpusOptions & getCorpusOptions() const;

	void setRetrievalOptions(const ofxGgmlRagRetrievalOptions & options);
	ofxGgmlRagRetrievalOptions & getRetrievalOptions();
	const ofxGgmlRagRetrievalOptions & getRetrievalOptions() const;
	void clearRetrievalCache();
	bool hasCachedRetrievals() const;
	std::size_t getRetrievalCacheSize() const;

	bool loadTextCorpus(const std::string & sourceRoot);
	bool reloadTextCorpus();
	void setDocuments(const std::vector<ofxGgmlRagDocument> & documents, const std::string & sourceRoot = "");
	void addDocument(const ofxGgmlRagDocument & document);
	void clearDocuments();

	bool hasDocuments() const;
	const std::vector<ofxGgmlRagDocument> & getDocuments() const;
	const ofxGgmlRagCorpus & getLastCorpus() const;

	ofxGgmlRagRetrieval retrieve();
	ofxGgmlRagRetrieval search(const std::string & query);
	ofxGgmlRagRetrieval loadAndSearch(const std::string & sourceRoot, const std::string & query);
	const ofxGgmlRagRetrieval & getLastRetrieval() const;

	std::string summarize() const;
	std::string format(const ofxGgmlRagReportOptions & options = ofxGgmlRagReportOptions()) const;
	std::string formatJson(const ofxGgmlRagReportOptions & options = ofxGgmlRagReportOptions()) const;
	ofxGgmlRagPrompt buildPrompt(const ofxGgmlRagPromptOptions & options = ofxGgmlRagPromptOptions()) const;
	ofxGgmlRagAnswer draftAnswer(const ofxGgmlRagAnswerOptions & options = ofxGgmlRagAnswerOptions()) const;
	void setPromptGenerator(const ofxGgmlRagPromptGenerator & generator);
	void clearPromptGenerator();
	bool hasPromptGenerator() const;
	ofxGgmlRagAnswer generateAnswer(
		const ofxGgmlRagPromptOptions & promptOptions = ofxGgmlRagPromptOptions(),
		const ofxGgmlRagAnswerOptions & answerOptions = ofxGgmlRagAnswerOptions()) const;
	ofxGgmlRagAnswer searchAndGenerateAnswer(
		const std::string & query,
		const ofxGgmlRagPromptOptions & promptOptions = ofxGgmlRagPromptOptions(),
		const ofxGgmlRagAnswerOptions & answerOptions = ofxGgmlRagAnswerOptions());
	ofxGgmlRagAnswer loadAndGenerateAnswer(
		const std::string & sourceRoot,
		const std::string & query,
		const ofxGgmlRagPromptOptions & promptOptions = ofxGgmlRagPromptOptions(),
		const ofxGgmlRagAnswerOptions & answerOptions = ofxGgmlRagAnswerOptions());
	ofxGgmlRagCitationSearchResult findCitations(
		const ofxGgmlRagCitationSearchOptions & options = ofxGgmlRagCitationSearchOptions()) const;
	ofxGgmlRagCitationSearchResult findCitations(
		const std::string & topic,
		const ofxGgmlRagCitationSearchOptions & options = ofxGgmlRagCitationSearchOptions()) const;
	ofxGgmlRagCitationSearchResult findCitationsFromInput(
		const std::string & input,
		const ofxGgmlRagCitationSearchOptions & options = ofxGgmlRagCitationSearchOptions()) const;

private:
	struct RetrievalCacheEntry {
		std::string key;
		ofxGgmlRagRetrieval retrieval;
	};

	void invalidateRetrievalCache();
	std::string buildRetrievalCacheKey() const;
	void storeRetrievalCacheEntry(const std::string & key, const ofxGgmlRagRetrieval & retrieval);

	ofxGgmlRagRequest request;
	ofxGgmlRagCorpusOptions corpusOptions;
	ofxGgmlRagRetrievalOptions retrievalOptions;
	ofxGgmlRagCorpus lastCorpus;
	ofxGgmlRagRetrieval lastRetrieval;
	std::vector<ofxGgmlRagDocument> documents;
	std::vector<RetrievalCacheEntry> retrievalCache;
	ofxGgmlRagPromptGenerator promptGenerator;
	std::size_t documentRevision = 0;
};
