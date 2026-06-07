#include "../ofxGgmlRag.h"

#include <algorithm>
#include <exception>
#include <sstream>

void ofxGgmlRag::clear() {
	request = ofxGgmlRagRequest();
	corpusOptions = ofxGgmlRagCorpusOptions();
	retrievalOptions = ofxGgmlRagRetrievalOptions();
	lastCorpus = ofxGgmlRagCorpus();
	lastRetrieval = ofxGgmlRagRetrieval();
	documents.clear();
	promptGenerator = ofxGgmlRagPromptGenerator();
	invalidateRetrievalCache();
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

void ofxGgmlRag::clearRetrievalCache() {
	invalidateRetrievalCache();
}

bool ofxGgmlRag::hasCachedRetrievals() const {
	return !retrievalCache.empty();
}

std::size_t ofxGgmlRag::getRetrievalCacheSize() const {
	return retrievalCache.size();
}

bool ofxGgmlRag::loadTextCorpus(const std::string & sourceRoot) {
	request.sourceRoot = sourceRoot;
	lastCorpus = ofxGgmlRagUtils::loadTextCorpus(request.sourceRoot, corpusOptions);
	documents = lastCorpus.documents;
	invalidateRetrievalCache();
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
	invalidateRetrievalCache();
}

void ofxGgmlRag::addDocument(const ofxGgmlRagDocument & document) {
	documents.push_back(document);
	lastCorpus.success = true;
	lastCorpus.documents = documents;
	lastCorpus.stats.loadedDocumentCount = documents.size();
	if (lastCorpus.sourceRoot.empty()) {
		lastCorpus.sourceRoot = request.sourceRoot;
	}
	invalidateRetrievalCache();
}

void ofxGgmlRag::clearDocuments() {
	documents.clear();
	lastCorpus = ofxGgmlRagCorpus();
	invalidateRetrievalCache();
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
	const auto cacheKey = buildRetrievalCacheKey();
	if (retrievalOptions.enableCache) {
		for (std::size_t i = 0; i < retrievalCache.size(); ++i) {
			if (retrievalCache[i].key != cacheKey) {
				continue;
			}
			lastRetrieval = retrievalCache[i].retrieval;
			lastRetrieval.stats.cacheHit = true;
			const auto entry = retrievalCache[i];
			retrievalCache.erase(retrievalCache.begin() + static_cast<std::ptrdiff_t>(i));
			retrievalCache.insert(retrievalCache.begin(), entry);
			return lastRetrieval;
		}
	}
	lastRetrieval = ofxGgmlRagUtils::retrieve(request, documents, retrievalOptions);
	lastRetrieval.stats.cacheHit = false;
	if (!lastCorpus.warnings.empty()) {
		for (const auto & warning : lastCorpus.warnings) {
			lastRetrieval.validation.warnings.push_back(warning);
		}
	}
	if (retrievalOptions.enableCache && lastRetrieval) {
		storeRetrievalCacheEntry(cacheKey, lastRetrieval);
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

void ofxGgmlRag::setPromptGenerator(const ofxGgmlRagPromptGenerator & generator) {
	promptGenerator = generator;
}

void ofxGgmlRag::clearPromptGenerator() {
	promptGenerator = ofxGgmlRagPromptGenerator();
}

bool ofxGgmlRag::hasPromptGenerator() const {
	return static_cast<bool>(promptGenerator);
}

ofxGgmlRagAnswer ofxGgmlRag::generateAnswer(
	const ofxGgmlRagPromptOptions & promptOptions,
	const ofxGgmlRagAnswerOptions & answerOptions) const {
	ofxGgmlRagAnswer answer;
	answer.question = ofxGgmlRagUtils::trim(request.query);
	answer.extractive = false;

	if (!promptGenerator) {
		answer.error = "prompt generator is not configured";
		return answer;
	}
	if (answerOptions.maxAnswerChars == 0) {
		answer.error = "answer character budget is zero";
		return answer;
	}

	const auto prompt = buildPrompt(promptOptions);
	if (!prompt) {
		answer.error = prompt.error.empty() ? "prompt build failed" : prompt.error;
		answer.citations = prompt.citations;
		answer.references = prompt.references;
		answer.truncated = prompt.truncated;
		return answer;
	}

	std::string generated;
	try {
		generated = ofxGgmlRagUtils::trim(promptGenerator(prompt));
	} catch (const std::exception & error) {
		answer.error = std::string("prompt generator failed: ") + error.what();
		return answer;
	} catch (...) {
		answer.error = "prompt generator failed";
		return answer;
	}

	if (generated.empty()) {
		answer.error = "prompt generator returned empty text";
		return answer;
	}

	answer.citations = prompt.citations;
	answer.references = prompt.references;
	answer.text = generated;
	if (answerOptions.includeReferences && !answer.references.empty()) {
		answer.text += "\n\nReferences:\n" + ofxGgmlRagUtils::formatReferences(answer.references);
	}
	if (answer.text.size() > answerOptions.maxAnswerChars) {
		answer.text.resize(answerOptions.maxAnswerChars);
		answer.truncated = true;
	}
	answer.success = !answer.text.empty();
	if (!answer.success) {
		answer.error = "model answer is empty after truncation";
	}
	return answer;
}

ofxGgmlRagAnswer ofxGgmlRag::searchAndGenerateAnswer(
	const std::string & query,
	const ofxGgmlRagPromptOptions & promptOptions,
	const ofxGgmlRagAnswerOptions & answerOptions) {
	search(query);
	return generateAnswer(promptOptions, answerOptions);
}

ofxGgmlRagAnswer ofxGgmlRag::loadAndGenerateAnswer(
	const std::string & sourceRoot,
	const std::string & query,
	const ofxGgmlRagPromptOptions & promptOptions,
	const ofxGgmlRagAnswerOptions & answerOptions) {
	loadAndSearch(sourceRoot, query);
	return generateAnswer(promptOptions, answerOptions);
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

void ofxGgmlRag::invalidateRetrievalCache() {
	++documentRevision;
	retrievalCache.clear();
}

std::string ofxGgmlRag::buildRetrievalCacheKey() const {
	std::ostringstream key;
	key << documentRevision << "\n";
	key << ofxGgmlRagUtils::trim(request.query) << "\n";
	key << ofxGgmlRagUtils::trim(request.sourceRoot) << "\n";
	for (const auto & tag : request.tags) {
		key << "tag:" << tag << "\n";
	}
	key << retrievalOptions.chunk.maxChars << "\n";
	key << retrievalOptions.chunk.overlapChars << "\n";
	key << retrievalOptions.search.topK << "\n";
	key << retrievalOptions.search.minMatchedTerms << "\n";
	key << retrievalOptions.search.minScore << "\n";
	key << retrievalOptions.search.phraseBoost << "\n";
	key << retrievalOptions.search.qualityWeight << "\n";
	key << retrievalOptions.search.ignoreStopWords << "\n";
	key << retrievalOptions.search.allowQueryRefinement << "\n";
	key << retrievalOptions.search.maxRefinementQueries << "\n";
	for (const auto & variant : retrievalOptions.search.queryVariants) {
		key << "variant:" << variant << "\n";
	}
	for (const auto & tag : retrievalOptions.search.requiredTags) {
		key << "required:" << tag << "\n";
	}
	for (const auto & tag : retrievalOptions.search.excludedTags) {
		key << "excluded:" << tag << "\n";
	}
	for (const auto & root : retrievalOptions.search.excludedSourceRoots) {
		key << "excludedRoot:" << root << "\n";
	}
	key << retrievalOptions.context.maxChars << "\n";
	key << retrievalOptions.context.includeQuery << "\n";
	key << retrievalOptions.context.includeScores << "\n";
	return key.str();
}

void ofxGgmlRag::storeRetrievalCacheEntry(const std::string & key, const ofxGgmlRagRetrieval & retrieval) {
	if (retrievalOptions.maxCacheEntries == 0) {
		return;
	}
	for (auto & entry : retrievalCache) {
		if (entry.key == key) {
			entry.retrieval = retrieval;
			return;
		}
	}
	RetrievalCacheEntry entry;
	entry.key = key;
	entry.retrieval = retrieval;
	retrievalCache.insert(retrievalCache.begin(), entry);
	if (retrievalCache.size() > retrievalOptions.maxCacheEntries) {
		retrievalCache.resize(retrievalOptions.maxCacheEntries);
	}
}
