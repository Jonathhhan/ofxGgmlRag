#include "ofxGgmlRag.h"

#include <iostream>

int main() {
	if (OFXGGML_RAG_VERSION_MAJOR != 1 ||
		OFXGGML_RAG_VERSION_MINOR != 0 ||
		OFXGGML_RAG_VERSION_PATCH != 1 ||
		std::string(OFXGGML_RAG_VERSION_STRING) != "1.0.1" ||
		std::string(ofxGgmlRagGetVersionString()) != "1.0.1") {
		std::cerr << "unexpected RAG addon version metadata\n";
		return 1;
	}

	ofxGgmlRagRequest request;
	if (ofxGgmlRagUtils::hasInput(request)) {
		std::cerr << "empty request reported as configured\n";
		return 1;
	}

	request.query = "   ";
	if (ofxGgmlRagUtils::hasInput(request)) {
		std::cerr << "whitespace request reported as configured\n";
		return 1;
	}

	const auto emptyValidation = ofxGgmlRagUtils::validate(request);
	if (emptyValidation || emptyValidation.errors.empty()) {
		std::cerr << "empty request validation did not report an error\n";
		return 1;
	}

	request.query = "  where is this idea documented?  ";
	request.sourceRoot = " docs ";
	request.tags = { " citations ", "", "memory", "citations" };
	if (!ofxGgmlRagUtils::hasInput(request)) {
		std::cerr << "configured request reported as empty\n";
		return 1;
	}

	const auto validation = ofxGgmlRagUtils::validate(request);
	if (!validation || !validation.errors.empty()) {
		std::cerr << "configured request validation reported an error\n";
		return 1;
	}

	const auto tags = ofxGgmlRagUtils::normalizedTags(request);
	if (tags.size() != 2 || tags[0] != "citations" || tags[1] != "memory") {
		std::cerr << "request tags were not normalized and deduplicated\n";
		return 1;
	}

	const auto description = ofxGgmlRagUtils::describe(request);
	if (description.find("where is this idea documented?") == std::string::npos ||
		description.find("docs") == std::string::npos ||
		description.find("citations, memory") == std::string::npos) {
		std::cerr << "description did not include request input\n";
		return 1;
	}

	ofxGgmlRagChunkOptions chunkOptions;
	chunkOptions.maxChars = 18;
	chunkOptions.overlapChars = 4;
	const auto chunks = ofxGgmlRagUtils::chunkText(
		" docs/RAG_WORKFLOWS.md ",
		"Alpha beta gamma delta epsilon zeta.",
		chunkOptions,
		request.tags);
	if (chunks.size() < 2) {
		std::cerr << "chunking did not split long text\n";
		return 1;
	}
	if (chunks[0].source != "docs/RAG_WORKFLOWS.md" ||
		chunks[0].index != 0 ||
		chunks[0].start != 0 ||
		chunks[0].text.empty()) {
		std::cerr << "first chunk metadata was not populated\n";
		return 1;
	}
	if (chunks[1].index != 1 ||
		chunks[1].start >= chunks[1].end ||
		chunks[1].start >= chunks[0].end ||
		chunks[1].tags.size() != 2 ||
		chunks[1].tags[0] != "citations") {
		std::cerr << "second chunk metadata was not normalized\n";
		return 1;
	}
	if (chunks.back().end != std::string("Alpha beta gamma delta epsilon zeta.").size()) {
		std::cerr << "final chunk did not preserve source end offset\n";
		return 1;
	}

	const auto tokens = ofxGgmlRagUtils::tokenize("Citation, citation! Memory_plan");
	if (tokens.size() != 2 || tokens[0] != "citation" || tokens[1] != "memory_plan") {
		std::cerr << "tokenization did not normalize unique terms\n";
		return 1;
	}
	if (!ofxGgmlRagUtils::sourceMatchesRoot("docs", "docs/a.md") ||
		!ofxGgmlRagUtils::sourceMatchesRoot("docs\\", "docs/a.md") ||
		ofxGgmlRagUtils::sourceMatchesRoot("docs", "docs-old/a.md")) {
		std::cerr << "source root matching did not preserve path boundaries\n";
		return 1;
	}

	ofxGgmlRagChunk firstSearchChunk;
	firstSearchChunk.source = "b.md";
	firstSearchChunk.index = 0;
	firstSearchChunk.text = "citation memory";
	firstSearchChunk.tags = { "citations" };
	ofxGgmlRagChunk secondSearchChunk;
	secondSearchChunk.source = "a.md";
	secondSearchChunk.index = 1;
	secondSearchChunk.text = "workflow citation memory";
	secondSearchChunk.tags = { "citations", "memory" };
	ofxGgmlRagChunk thirdSearchChunk;
	thirdSearchChunk.source = "c.md";
	thirdSearchChunk.index = 2;
	thirdSearchChunk.text = "unrelated";
	thirdSearchChunk.tags = { "memory" };
	const std::vector<ofxGgmlRagChunk> searchChunks = {
		firstSearchChunk,
		secondSearchChunk,
		thirdSearchChunk
	};

	ofxGgmlRagSearchOptions searchOptions;
	searchOptions.topK = 2;
	auto hits = ofxGgmlRagUtils::searchChunks("workflow citation memory", searchChunks, searchOptions);
	if (hits.size() != 2 ||
		hits[0].chunk.source != "a.md" ||
		hits[0].matchedTerms.size() != 3 ||
		hits[1].chunk.source != "b.md") {
		std::cerr << "chunk search did not rank by deterministic term overlap\n";
		return 1;
	}

	searchOptions.requiredTags = { " memory " };
	hits = ofxGgmlRagUtils::searchChunks("citation", searchChunks, searchOptions);
	if (hits.size() != 1 || hits[0].chunk.source != "a.md") {
		std::cerr << "chunk search did not filter by required tags\n";
		return 1;
	}

	searchOptions.requiredTags.clear();
	searchOptions.minScore = 0.75;
	hits = ofxGgmlRagUtils::searchChunks("workflow citation memory", searchChunks, searchOptions);
	if (hits.size() != 1 || hits[0].chunk.source != "a.md") {
		std::cerr << "chunk search did not filter by minimum score\n";
		return 1;
	}
	searchOptions.minScore = 0.0;

	ofxGgmlRagChunk phraseChunkA;
	phraseChunkA.source = "z.md";
	phraseChunkA.index = 0;
	phraseChunkA.text = "memory citation";
	ofxGgmlRagChunk phraseChunkB;
	phraseChunkB.source = "a.md";
	phraseChunkB.index = 0;
	phraseChunkB.text = "citation memory";
	ofxGgmlRagSearchOptions phraseOptions;
	phraseOptions.topK = 2;
	auto phraseHits = ofxGgmlRagUtils::searchChunks("memory citation", { phraseChunkA, phraseChunkB }, phraseOptions);
	if (phraseHits.size() != 2 || phraseHits[0].chunk.source != "a.md") {
		std::cerr << "baseline phrase fixture did not sort ties by source\n";
		return 1;
	}
	phraseOptions.phraseBoost = 0.25;
	phraseHits = ofxGgmlRagUtils::searchChunks("memory citation", { phraseChunkA, phraseChunkB }, phraseOptions);
	if (phraseHits.size() != 2 ||
		phraseHits[0].chunk.source != "z.md" ||
		phraseHits[0].score <= phraseHits[1].score) {
		std::cerr << "chunk search did not apply phrase boost\n";
		return 1;
	}

	searchOptions.topK = 0;
	if (!ofxGgmlRagUtils::searchChunks("citation", searchChunks, searchOptions).empty()) {
		std::cerr << "chunk search ignored topK zero\n";
		return 1;
	}

	ofxGgmlRagSearchHit excerptHit;
	excerptHit.chunk.source = "docs/excerpt.md";
	excerptHit.chunk.index = 3;
	excerptHit.chunk.text = "Alpha beta gamma delta epsilon zeta eta theta.";
	excerptHit.chunk.start = 4;
	excerptHit.chunk.end = 48;
	excerptHit.score = 0.5;
	excerptHit.matchedTerms = { "epsilon" };
	ofxGgmlRagExcerptOptions excerptOptions;
	excerptOptions.contextChars = 6;
	const auto excerpt = ofxGgmlRagUtils::excerptForHit(excerptHit, excerptOptions);
	if (excerpt != "...delta epsilon zeta...") {
		std::cerr << "search hit excerpt did not center the matched term\n";
		return 1;
	}
	excerptOptions.includeEllipses = false;
	if (ofxGgmlRagUtils::excerptForHit(excerptHit, excerptOptions) != "delta epsilon zeta") {
		std::cerr << "search hit excerpt did not honor ellipsis option\n";
		return 1;
	}
	excerptOptions.includeEllipses = true;
	const auto formattedHit = ofxGgmlRagUtils::formatHit(excerptHit, excerptOptions);
	if (formattedHit.find("docs/excerpt.md#3") == std::string::npos ||
		formattedHit.find("score=0.5") == std::string::npos ||
		formattedHit.find("terms=epsilon") == std::string::npos ||
		formattedHit.find("...delta epsilon zeta...") == std::string::npos) {
		std::cerr << "formatted search hit did not include evidence details\n";
		return 1;
	}

	const auto generatedCitation = ofxGgmlRagUtils::citationFromChunk(secondSearchChunk);
	if (generatedCitation.label != "a.md#1" ||
		generatedCitation.source != "a.md" ||
		generatedCitation.start != secondSearchChunk.start ||
		generatedCitation.end != secondSearchChunk.end) {
		std::cerr << "generated citation did not preserve chunk identity\n";
		return 1;
	}

	searchOptions.topK = 2;
	searchOptions.requiredTags.clear();
	hits = ofxGgmlRagUtils::searchChunks("workflow citation memory", searchChunks, searchOptions);
	const auto hitResult = ofxGgmlRagUtils::resultFromHits(" workflow citation memory ", hits);
	if (!hitResult ||
		hitResult.text.find("Found 2 matching chunks") == std::string::npos ||
		hitResult.text.find("workflow citation memory") == std::string::npos ||
		hitResult.citations.size() != 2 ||
		hitResult.references.size() != 2 ||
		hitResult.references[0].find("a.md#1") == std::string::npos) {
		std::cerr << "hit result did not include citation-aware search output\n";
		return 1;
	}

	ofxGgmlRagContextOptions contextOptions;
	contextOptions.maxChars = 512;
	const auto context = ofxGgmlRagUtils::contextFromHits(" workflow citation memory ", hits, contextOptions);
	if (!context ||
		context.text.find("Query: workflow citation memory") == std::string::npos ||
		context.text.find("[1] a.md#1") == std::string::npos ||
		context.text.find("score=1") == std::string::npos ||
		context.citations.size() != 2 ||
		context.includedHitCount != 2 ||
		context.truncated) {
		std::cerr << "context assembly did not preserve hit evidence\n";
		return 1;
	}

	contextOptions.maxChars = 48;
	const auto truncatedContext = ofxGgmlRagUtils::contextFromHits("workflow citation memory", hits, contextOptions);
	if (truncatedContext.includedHitCount != 0 || !truncatedContext.truncated) {
		std::cerr << "context assembly did not report budget truncation\n";
		return 1;
	}

	contextOptions.maxChars = 512;
	contextOptions.includeQuery = false;
	contextOptions.includeScores = false;
	const auto compactContext = ofxGgmlRagUtils::contextFromHits("workflow citation memory", hits, contextOptions);
	if (!compactContext ||
		compactContext.text.find("Query:") != std::string::npos ||
		compactContext.text.find("score=") != std::string::npos) {
		std::cerr << "context assembly did not honor compact options\n";
		return 1;
	}

	const auto noHitResult = ofxGgmlRagUtils::resultFromHits("missing", {});
	if (noHitResult || noHitResult.error != "no matching chunks") {
		std::cerr << "empty hit result did not report a deterministic failure\n";
		return 1;
	}

	std::vector<ofxGgmlRagDocument> documents;
	documents.push_back({ "docs/a.md", "Workflow citation preserves memory boundaries.", { "docs" } });
	documents.push_back({ "docs/b.md", "Rendering examples are unrelated.", { "examples" } });
	documents.push_back({ "notes/c.md", "citation memory citation memory citation memory", { "notes" } });
	ofxGgmlRagRequest retrievalRequest;
	retrievalRequest.query = "citation memory";
	retrievalRequest.sourceRoot = "docs";
	retrievalRequest.tags = { "retrieval" };
	ofxGgmlRagRetrievalOptions retrievalOptions;
	retrievalOptions.chunk.maxChars = 80;
	retrievalOptions.search.topK = 1;
	const auto retrieval = ofxGgmlRagUtils::retrieve(retrievalRequest, documents, retrievalOptions);
	if (!retrieval ||
		!retrieval.validation ||
		retrieval.stats.documentCount != 3 ||
		retrieval.stats.scopedDocumentCount != 2 ||
		retrieval.stats.skippedDocumentCount != 1 ||
		retrieval.stats.chunkCount != 2 ||
		retrieval.stats.hitCount != 1 ||
		retrieval.stats.citationCount != 1 ||
		retrieval.stats.contextTruncated ||
		retrieval.chunks.size() != 2 ||
		retrieval.hits.size() != 1 ||
		retrieval.context.includedHitCount != 1 ||
		retrieval.result.citations.size() != 1 ||
		retrieval.result.references.empty() ||
		retrieval.hits[0].chunk.source != "docs/a.md" ||
		retrieval.hits[0].chunk.tags.size() != 2 ||
		retrieval.hits[0].chunk.tags[1] != "retrieval") {
		std::cerr << "retrieval pipeline did not assemble deterministic evidence\n";
		return 1;
	}
	const auto statsText = ofxGgmlRagUtils::formatStats(retrieval.stats);
	if (statsText != "documents=3 scoped=2 skipped=1 chunks=2 hits=1 citations=1 contextTruncated=false") {
		std::cerr << "retrieval stats formatting changed unexpectedly\n";
		return 1;
	}
	const auto retrievalSummary = ofxGgmlRagUtils::summarize(retrieval);
	if (retrievalSummary.find("rag result: ok") == std::string::npos ||
		retrievalSummary.find(statsText) == std::string::npos) {
		std::cerr << "retrieval summary did not include result and stats\n";
		return 1;
	}
	ofxGgmlRagReportOptions reportOptions;
	reportOptions.maxHits = 1;
	reportOptions.excerpt.contextChars = 12;
	const auto retrievalReport = ofxGgmlRagUtils::formatRetrieval(retrieval, reportOptions);
	if (retrievalReport.find("rag result: ok") == std::string::npos ||
		retrievalReport.find("hit[1] docs/a.md#0") == std::string::npos ||
		retrievalReport.find("terms=citation,memory") == std::string::npos ||
		retrievalReport.find("references:\n[1] docs/a.md#0") == std::string::npos ||
		retrievalReport.find("context:") != std::string::npos ||
		retrievalReport.find("hitsTruncated=") != std::string::npos) {
		std::cerr << "retrieval report did not include compact hit evidence\n";
		return 1;
	}
	reportOptions.includeReferences = false;
	const auto noReferenceReport = ofxGgmlRagUtils::formatRetrieval(retrieval, reportOptions);
	if (noReferenceReport.find("references:") != std::string::npos) {
		std::cerr << "retrieval report did not honor reference option\n";
		return 1;
	}
	reportOptions.includeReferences = true;
	reportOptions.includeContext = true;
	const auto contextReport = ofxGgmlRagUtils::formatRetrieval(retrieval, reportOptions);
	if (contextReport.find("context:") == std::string::npos ||
		contextReport.find("Query: citation memory") == std::string::npos) {
		std::cerr << "retrieval report did not include optional context\n";
		return 1;
	}

	retrievalRequest.sourceRoot = "missing";
	const auto outOfRootRetrieval = ofxGgmlRagUtils::retrieve(retrievalRequest, documents, retrievalOptions);
	if (outOfRootRetrieval ||
		outOfRootRetrieval.result.error != "no documents in sourceRoot" ||
		outOfRootRetrieval.stats.documentCount != 3 ||
		outOfRootRetrieval.stats.scopedDocumentCount != 0 ||
		outOfRootRetrieval.stats.skippedDocumentCount != 3) {
		std::cerr << "retrieval pipeline did not report empty sourceRoot scope\n";
		return 1;
	}

	ofxGgmlRagRequest invalidRetrievalRequest;
	const auto invalidRetrieval = ofxGgmlRagUtils::retrieve(invalidRetrievalRequest, documents, retrievalOptions);
	if (invalidRetrieval ||
		invalidRetrieval.result.error != "invalid request" ||
		invalidRetrieval.stats.documentCount != 3) {
		std::cerr << "retrieval pipeline did not report invalid request\n";
		return 1;
	}

	const auto emptyCorpusRetrieval = ofxGgmlRagUtils::retrieve(retrievalRequest, {}, retrievalOptions);
	if (emptyCorpusRetrieval ||
		emptyCorpusRetrieval.result.error != "no documents" ||
		emptyCorpusRetrieval.stats.documentCount != 0) {
		std::cerr << "retrieval pipeline did not report empty corpus\n";
		return 1;
	}

	retrievalRequest.sourceRoot = "docs";
	retrievalOptions.search.minScore = 1.1;
	const auto strictRetrieval = ofxGgmlRagUtils::retrieve(retrievalRequest, documents, retrievalOptions);
	if (strictRetrieval ||
		strictRetrieval.result.error != "no matching chunks" ||
		strictRetrieval.stats.hitCount != 0 ||
		strictRetrieval.stats.chunkCount != 2) {
		std::cerr << "retrieval pipeline did not honor minimum score filtering\n";
		return 1;
	}

	ofxGgmlRagCitation citation;
	citation.source = "docs/RAG_WORKFLOWS.md";
	citation.label = "workflow boundaries";
	citation.start = 10;
	citation.end = 42;
	const auto formattedCitation = ofxGgmlRagUtils::formatCitation(citation);
	if (formattedCitation.find("workflow boundaries") == std::string::npos ||
		formattedCitation.find("docs/RAG_WORKFLOWS.md:10-42") == std::string::npos) {
		std::cerr << "citation formatting did not include label and span\n";
		return 1;
	}
	const auto citationReferences = ofxGgmlRagUtils::referencesFromCitations({ citation, citation });
	if (citationReferences.size() != 1 || citationReferences[0] != formattedCitation) {
		std::cerr << "citation references were not deduplicated\n";
		return 1;
	}
	const auto bibliography = ofxGgmlRagUtils::formatReferences({
		citationReferences[0],
		"  ",
		"another source"
	});
	if (bibliography != "[1] " + citationReferences[0] + "\n[2] another source") {
		std::cerr << "reference bibliography formatting changed unexpectedly\n";
		return 1;
	}

	ofxGgmlRagResult result;
	if (ofxGgmlRagUtils::hasCitations(result)) {
		std::cerr << "empty result reported citations\n";
		return 1;
	}
	result.success = true;
	result.text = "Use the RAG workflow docs.";
	result.citations.push_back(citation);
	if (!ofxGgmlRagUtils::hasCitations(result)) {
		std::cerr << "cited result did not report citations\n";
		return 1;
	}
	const auto summary = ofxGgmlRagUtils::summarize(result);
	if (summary.find("ok") == std::string::npos ||
		summary.find("1 citation") == std::string::npos) {
		std::cerr << "result summary did not include status and citation count\n";
		return 1;
	}

	ofxGgmlRagResult failed;
	failed.error = "missing corpus";
	if (ofxGgmlRagUtils::summarize(failed).find("missing corpus") == std::string::npos) {
		std::cerr << "error summary did not include failure reason\n";
		return 1;
	}

	return 0;
}
