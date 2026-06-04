#include "../ofxGgmlRag.h"

void ofxGgmlRag::clear() {
	request = ofxGgmlRagRequest();
	corpusOptions = ofxGgmlRagCorpusOptions();
	retrievalOptions = ofxGgmlRagRetrievalOptions();
	lastCorpus = ofxGgmlRagCorpus();
	lastRetrieval = ofxGgmlRagRetrieval();
	documents.clear();
}

void ofxGgmlRag::setRequest(const ofxGgmlRagRequest & value) {
	request = value;
}

void ofxGgmlRag::setQuery(const std::string & query) {
	request.query = query;
}

void ofxGgmlRag::setSourceRoot(const std::string & sourceRoot) {
	request.sourceRoot = sourceRoot;
}

void ofxGgmlRag::setTags(const std::vector<std::string> & tags) {
	request.tags = tags;
}

const ofxGgmlRagRequest & ofxGgmlRag::getRequest() const {
	return request;
}

void ofxGgmlRag::setCorpusOptions(const ofxGgmlRagCorpusOptions & options) {
	corpusOptions = options;
}

ofxGgmlRagCorpusOptions & ofxGgmlRag::getCorpusOptions() {
	return corpusOptions;
}

const ofxGgmlRagCorpusOptions & ofxGgmlRag::getCorpusOptions() const {
	return corpusOptions;
}

void ofxGgmlRag::setRetrievalOptions(const ofxGgmlRagRetrievalOptions & options) {
	retrievalOptions = options;
}

ofxGgmlRagRetrievalOptions & ofxGgmlRag::getRetrievalOptions() {
	return retrievalOptions;
}

const ofxGgmlRagRetrievalOptions & ofxGgmlRag::getRetrievalOptions() const {
	return retrievalOptions;
}

bool ofxGgmlRag::loadTextCorpus(const std::string & sourceRoot) {
	request.sourceRoot = sourceRoot;
	lastCorpus = ofxGgmlRagUtils::loadTextCorpus(request.sourceRoot, corpusOptions);
	documents = lastCorpus.documents;
	if (lastCorpus) {
		request.sourceRoot = lastCorpus.sourceRoot;
	}
	return static_cast<bool>(lastCorpus);
}

bool ofxGgmlRag::reloadTextCorpus() {
	return loadTextCorpus(request.sourceRoot);
}

void ofxGgmlRag::setDocuments(const std::vector<ofxGgmlRagDocument> & value, const std::string & sourceRoot) {
	documents = value;
	lastCorpus = ofxGgmlRagCorpus();
	lastCorpus.success = !documents.empty();
	lastCorpus.sourceRoot = sourceRoot.empty() ? request.sourceRoot : sourceRoot;
	lastCorpus.stats.loadedDocumentCount = documents.size();
	lastCorpus.documents = documents;
	request.sourceRoot = lastCorpus.sourceRoot;
}

void ofxGgmlRag::addDocument(const ofxGgmlRagDocument & document) {
	documents.push_back(document);
	lastCorpus.success = true;
	lastCorpus.documents = documents;
	lastCorpus.stats.loadedDocumentCount = documents.size();
	if (lastCorpus.sourceRoot.empty()) {
		lastCorpus.sourceRoot = request.sourceRoot;
	}
}

void ofxGgmlRag::clearDocuments() {
	documents.clear();
	lastCorpus = ofxGgmlRagCorpus();
}

bool ofxGgmlRag::hasDocuments() const {
	return !documents.empty();
}

const std::vector<ofxGgmlRagDocument> & ofxGgmlRag::getDocuments() const {
	return documents;
}

const ofxGgmlRagCorpus & ofxGgmlRag::getLastCorpus() const {
	return lastCorpus;
}

ofxGgmlRagRetrieval ofxGgmlRag::retrieve() {
	if (documents.empty() && !ofxGgmlRagUtils::trim(request.sourceRoot).empty()) {
		if (!loadTextCorpus(request.sourceRoot)) {
			lastRetrieval = ofxGgmlRagRetrieval();
			lastRetrieval.validation = ofxGgmlRagUtils::validate(request);
			lastRetrieval.stats.documentCount = lastCorpus.stats.loadedDocumentCount;
			lastRetrieval.result.error = lastCorpus.error.empty() ? "corpus load failed" : lastCorpus.error;
			for (const auto & warning : lastCorpus.warnings) {
				lastRetrieval.validation.warnings.push_back(warning);
			}
			return lastRetrieval;
		}
	}
	lastRetrieval = ofxGgmlRagUtils::retrieve(request, documents, retrievalOptions);
	if (!lastCorpus.warnings.empty()) {
		for (const auto & warning : lastCorpus.warnings) {
			lastRetrieval.validation.warnings.push_back(warning);
		}
	}
	return lastRetrieval;
}

ofxGgmlRagRetrieval ofxGgmlRag::search(const std::string & query) {
	request.query = query;
	return retrieve();
}

ofxGgmlRagRetrieval ofxGgmlRag::loadAndSearch(const std::string & sourceRoot, const std::string & query) {
	request.query = query;
	loadTextCorpus(sourceRoot);
	return retrieve();
}

const ofxGgmlRagRetrieval & ofxGgmlRag::getLastRetrieval() const {
	return lastRetrieval;
}

std::string ofxGgmlRag::summarize() const {
	return ofxGgmlRagUtils::summarize(lastRetrieval);
}

std::string ofxGgmlRag::format(const ofxGgmlRagReportOptions & options) const {
	return ofxGgmlRagUtils::formatRetrieval(lastRetrieval, options);
}

std::string ofxGgmlRag::formatJson(const ofxGgmlRagReportOptions & options) const {
	return ofxGgmlRagUtils::formatRetrievalJson(lastRetrieval, options);
}

ofxGgmlRagPrompt ofxGgmlRag::buildPrompt(const ofxGgmlRagPromptOptions & options) const {
	return ofxGgmlRagUtils::buildPrompt(request.query, lastRetrieval, options);
}

ofxGgmlRagAnswer ofxGgmlRag::draftAnswer(const ofxGgmlRagAnswerOptions & options) const {
	return ofxGgmlRagUtils::draftAnswer(request.query, lastRetrieval, options);
}

ofxGgmlRagCitationSearchResult ofxGgmlRag::findCitations(const ofxGgmlRagCitationSearchOptions & options) const {
	return ofxGgmlRagUtils::findCitations(request.query, documents, options);
}

ofxGgmlRagCitationSearchResult ofxGgmlRag::findCitations(
	const std::string & topic,
	const ofxGgmlRagCitationSearchOptions & options) const {
	return ofxGgmlRagUtils::findCitations(topic, documents, options);
}

ofxGgmlRagCitationSearchResult ofxGgmlRag::findCitationsFromInput(
	const std::string & input,
	const ofxGgmlRagCitationSearchOptions & options) const {
	return ofxGgmlRagUtils::findCitationsFromInput(input, documents, options);
}
