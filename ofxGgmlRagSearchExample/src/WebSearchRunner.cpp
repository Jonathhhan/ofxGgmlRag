#include "WebSearchRunner.h"
#include "ofMain.h"
#include "ofxGgmlRag.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <map>
#include <set>
#include <sstream>

namespace {
ofHttpResponse get(const std::string & url, const WebSearchConfig & c) {
	static ofURLFileLoader loader;
	ofHttpRequest request(url, url, false);
	request.method = ofHttpRequest::GET;
	request.timeoutSeconds = c.timeoutSeconds;
	request.headers["User-Agent"] = c.userAgent;
	request.headers["Accept"] = "text/html,text/plain;q=0.9,*/*;q=0.1";
	return loader.handleRequest(request);
}

std::string origin(const std::string & url) {
	auto scheme = url.find("://"); if (scheme == std::string::npos) return "";
	auto slash = url.find('/', scheme + 3); return slash == std::string::npos ? url : url.substr(0, slash);
}

std::string modelAnswer(const WebSearchConfig & c, const std::string & prompt, std::size_t citationCount, std::string & error) {
	static ofURLFileLoader loader;
	ofJson body = {
		{"model", c.model},
		{"messages", {{{"role", "user"}, {"content", prompt}}}},
		{"temperature", 0},
		{"max_tokens", std::max(1, c.maxModelTokens)},
		{"stream", false}
	};
	if (c.strictJsonAnswer) {
		body["response_format"] = {
			{"type", "json_schema"},
			{"json_schema", {
				{"name", "grounded_answer"}, {"strict", true},
				{"schema", {
					{"type", "object"},
					{"properties", {
						{"answer", {{"type", "string"}}},
						{"citation_ids", {{"type", "array"}, {"items", {{"type", "integer"}}}}}
					}},
					{"required", ofJson::array({"answer", "citation_ids"})},
					{"additionalProperties", false}
				}}
			}}
		};
	}
	ofHttpRequest request(c.modelEndpoint, "model", false);
	request.method = ofHttpRequest::POST; request.timeoutSeconds = std::max(1, c.modelTimeoutSeconds);
	request.headers["Content-Type"] = "application/json"; request.body = body.dump();
	auto response = loader.handleRequest(request);
	if (response.status < 200 || response.status >= 300) {
		const auto details = ofxGgmlRagUtils::trim(response.data.getText());
		error = "HTTP " + ofToString(response.status) + ": " + (details.empty() ? response.error : details);
		return "";
	}
	try {
		const auto content = ofJson::parse(response.data.getText())["choices"][0]["message"]["content"].get<std::string>();
		if (!c.strictJsonAnswer) return content;
		const auto structured = ofJson::parse(content);
		if (!structured.is_object() || structured.size() != 2 || !structured.contains("answer") || !structured["answer"].is_string() ||
			!structured.contains("citation_ids") || !structured["citation_ids"].is_array()) {
			throw std::runtime_error("strict answer did not match the grounded_answer schema");
		}
		const auto answer = ofxGgmlRagUtils::trim(structured["answer"].get<std::string>());
		if (answer.empty()) throw std::runtime_error("strict answer was empty");
		std::ostringstream rendered;
		rendered << answer << "\nCitations:";
		for (const auto & idValue : structured["citation_ids"]) {
			if (!idValue.is_number_integer()) throw std::runtime_error("strict answer contained a non-integer citation id");
			const auto id = idValue.get<int>();
			if (id < 1 || static_cast<std::size_t>(id) > citationCount) throw std::runtime_error("strict answer cited an unknown context id");
			rendered << " [" << id << "]";
		}
		return rendered.str();
	}
	catch (const std::exception & e) { error = e.what(); return ""; }
}

bool requestEmbeddings(const WebSearchConfig & c, const std::vector<std::string> & inputs,
	std::vector<std::vector<float>> & embeddings, std::string & error) {
	if (c.embeddingEndpoint.empty() || c.embeddingModel.empty()) {
		error = "embedding endpoint and model alias are required";
		return false;
	}
	static ofURLFileLoader loader;
	ofJson input = ofJson::array();
	for (const auto & text : inputs) input.push_back(text);
	ofJson body = {{"model", c.embeddingModel}, {"input", input}};
	ofHttpRequest request(c.embeddingEndpoint, "embeddings", false);
	request.method = ofHttpRequest::POST;
	request.timeoutSeconds = std::max(1, c.modelTimeoutSeconds);
	request.headers["Content-Type"] = "application/json";
	request.body = body.dump();
	const auto response = loader.handleRequest(request);
	if (response.status < 200 || response.status >= 300) {
		const auto details = ofxGgmlRagUtils::trim(response.data.getText());
		error = "HTTP " + ofToString(response.status) + ": " + (details.empty() ? response.error : details);
		return false;
	}
	try {
		const auto data = ofJson::parse(response.data.getText()).at("data");
		if (!data.is_array() || data.size() != inputs.size()) throw std::runtime_error("embedding response count did not match the request");
		embeddings.assign(inputs.size(), {});
		for (std::size_t position = 0; position < data.size(); ++position) {
			const auto & item = data[position];
			const auto index = item.value("index", position);
			if (index >= embeddings.size()) throw std::runtime_error("embedding response contained an invalid index");
			embeddings[index] = item.at("embedding").get<std::vector<float>>();
			if (embeddings[index].empty()) throw std::runtime_error("embedding response contained an empty vector");
		}
		return true;
	} catch (const std::exception & e) {
		error = e.what();
		return false;
	}
}

bool hybridRerank(ofxGgmlRagRetrieval & retrieval, const std::string & query,
	const WebSearchConfig & c, std::string & error) {
	if (retrieval.hits.empty()) {
		error = "lexical candidate retrieval returned no hits";
		return false;
	}
	std::vector<std::string> inputs{query};
	for (const auto & hit : retrieval.hits) inputs.push_back(hit.chunk.text);
	std::vector<std::vector<float>> embeddings;
	if (!requestEmbeddings(c, inputs, embeddings, error)) return false;
	std::vector<ofxGgmlRagEmbeddedChunk> candidates;
	for (std::size_t i = 0; i < retrieval.hits.size(); ++i) candidates.push_back({retrieval.hits[i].chunk, embeddings[i + 1]});
	ofxGgmlRagVectorSearchOptions vectorOptions;
	vectorOptions.topK = candidates.size();
	vectorOptions.minScore = -1.0;
	const auto vectorHits = ofxGgmlRagUtils::searchEmbeddedChunks(embeddings.front(), candidates, vectorOptions);
	const auto keyFor = [](const ofxGgmlRagChunk & chunk) {
		return chunk.source + "\n" + ofToString(chunk.index) + "\n" + chunk.text;
	};
	std::map<std::string, std::size_t> vectorRanks;
	for (std::size_t i = 0; i < vectorHits.size(); ++i) vectorRanks[keyFor(vectorHits[i].chunk)] = i;
	const double semanticWeight = std::max(0.0, std::min(1.0, c.embeddingWeight));
	for (std::size_t i = 0; i < retrieval.hits.size(); ++i) {
		const auto found = vectorRanks.find(keyFor(retrieval.hits[i].chunk));
		const auto vectorRank = found == vectorRanks.end() ? vectorHits.size() : found->second;
		retrieval.hits[i].score = ((1.0 - semanticWeight) / (60.0 + static_cast<double>(i + 1))) +
			(semanticWeight / (60.0 + static_cast<double>(vectorRank + 1)));
	}
	std::stable_sort(retrieval.hits.begin(), retrieval.hits.end(), [](const auto & left, const auto & right) {
		return left.score > right.score;
	});
	retrieval.context = ofxGgmlRagUtils::contextFromHits(query, retrieval.hits);
	retrieval.result = ofxGgmlRagUtils::resultFromHits(query, retrieval.hits);
	return true;
}

void diversifyRetrieval(ofxGgmlRagRetrieval & retrieval, const std::string & query, std::size_t maxHits) {
	std::vector<ofxGgmlRagSearchHit> selected;
	std::set<std::string> sources;
	for (const auto & hit : retrieval.hits) {
		if (sources.insert(hit.chunk.source).second) selected.push_back(hit);
		if (selected.size() == maxHits) break;
	}
	if (selected.size() < maxHits) {
		for (const auto & hit : retrieval.hits) {
			const auto alreadySelected = std::find_if(selected.begin(), selected.end(), [&](const auto & value) {
				return value.chunk.source == hit.chunk.source && value.chunk.index == hit.chunk.index;
			});
			if (alreadySelected == selected.end()) selected.push_back(hit);
			if (selected.size() == maxHits) break;
		}
	}
	retrieval.hits = std::move(selected);
	retrieval.context = ofxGgmlRagUtils::contextFromHits(query, retrieval.hits);
	retrieval.result = ofxGgmlRagUtils::resultFromHits(query, retrieval.hits);
	retrieval.stats.hitCount = retrieval.hits.size();
	retrieval.stats.citationCount = retrieval.context.citations.size();
	retrieval.stats.contextTruncated = retrieval.context.truncated;
}
}

WebSearchRun runWebSearch(const WebSearchConfig & c) {
	WebSearchRun run; std::ostringstream log;
	const auto fetchDeadline = std::chrono::steady_clock::now() +
		std::chrono::seconds(std::max(1, c.totalFetchTimeoutSeconds));
	const auto effectiveQuery = c.quoteMode ? ragWebExample::quoteSearchQuery(c.person) : c.query;
	if (effectiveQuery.empty() || c.searchUrlTemplate.find("{query}") == std::string::npos) { run.error = c.quoteMode ? "a person is required for quote search" : "query and a search URL template containing {query} are required"; return run; }
	const auto searchUrl = ragWebExample::expandSearchUrl(c.searchUrlTemplate, effectiveQuery);
	if (c.quoteMode) log << "[QUOTE MODE] person=" << c.person << "\n";
	log << "[SEARCH] GET " << searchUrl << "\n";
	auto search = get(searchUrl, c);
	if (search.status < 200 || search.status >= 300) { run.error = "live search failed: HTTP " + ofToString(search.status) + " " + search.error; run.report = log.str(); return run; }
	auto hits = ragWebExample::parseSearchHtml(search.data.getText(), c.limits.maxSearchResults);
	log << "[SEARCH] status=" << search.status << " results=" << hits.size() << "\n";
	if (hits.empty()) { run.error = "live search returned no parseable result URLs (no fixture fallback)"; run.report = log.str(); return run; }

	std::deque<std::pair<std::string, std::size_t>> queue;
	for (const auto & hit : hits) queue.push_back({hit.url, 0});
	std::set<std::string> queued; for (const auto & hit : hits) queued.insert(hit.url);
	std::set<std::string> robotsLoaded; std::map<std::string, std::string> robots;
	std::map<std::string, double> searchQuality;
	for (std::size_t i = 0; i < hits.size(); ++i) {
		searchQuality[hits[i].url] = 1.0 - (0.5 * static_cast<double>(i) / std::max<std::size_t>(1, hits.size()));
	}
	std::vector<ofxGgmlRagDocument> documents; std::vector<ragWebExample::QuoteHit> structuredQuotes; std::size_t totalBytes = 0;
	while (!queue.empty() && documents.size() < c.limits.maxPages) {
		if (std::chrono::steady_clock::now() >= fetchDeadline) {
			log << "[SCRAPE] stopped after total fetch budget of " << c.totalFetchTimeoutSeconds << " seconds\n";
			break;
		}
		auto item = queue.front(); queue.pop_front(); const auto site = origin(item.first);
		if (!robotsLoaded.count(site)) { auto rr = get(site + "/robots.txt", c); robots[site] = rr.status == 200 ? rr.data.getText() : ""; robotsLoaded.insert(site); log << "[SCRAPE] robots " << site << " status=" << rr.status << "\n"; }
		if (!ofxGgmlRagUtils::robotsTxtAllows(item.first, robots[site], c.userAgent)) { log << "[SCRAPE] blocked by robots.txt " << item.first << "\n"; continue; }
		auto page = get(item.first, c); if (page.status < 200 || page.status >= 300) { log << "[SCRAPE] HTTP " << page.status << " " << item.first << "\n"; continue; }
		const auto html = page.data.getText(); std::string reason;
		if (!ragWebExample::withinBounds(documents.size(), totalBytes, item.second, html.size(), c.limits, reason)) { log << "[SCRAPE] skipped " << reason << " " << item.first << "\n"; continue; }
		ofxGgmlRagHtmlOptions options; options.maxHtmlBytes = c.limits.maxBytesPerPage;
		auto converted = ofxGgmlRagUtils::documentFromHtml(item.first, html, options);
		if (!converted) { log << "[SCRAPE] skipped " << converted.error << " " << item.first << "\n"; continue; }
		const auto quality = searchQuality.find(item.first);
		converted.document.qualityHint = quality == searchQuality.end() ? 0.4 : quality->second;
		totalBytes += html.size(); documents.push_back(converted.document); log << "[SCRAPE] accepted bytes=" << html.size() << " url=" << item.first << "\n";
		if (c.quoteMode) {
			const auto pageQuotes = ragWebExample::extractStructuredQuotes(item.first, html, c.person, 2);
			structuredQuotes.insert(structuredQuotes.end(), pageQuotes.begin(), pageQuotes.end());
			log << "[SCRAPE] structuredQuotes=" << pageQuotes.size() << " url=" << item.first << "\n";
		}
		if (item.second < c.limits.maxDepth) { ofxGgmlRagHtmlLinkOptions links; links.robotsTxt = robots[site]; links.robotsTxtUserAgent = c.userAgent; links.maxLinks = c.limits.maxPages; for (const auto & url : ofxGgmlRagUtils::extractHtmlLinks(item.first, html, links)) if (queued.insert(url).second) queue.push_back({url, item.second + 1}); }
	}
	if (documents.empty()) { run.error = "no page passed HTTP, robots, byte, and HTML ingestion checks"; run.report = log.str(); return run; }
	if (c.quoteMode) {
		if (structuredQuotes.empty()) { run.error = "no explicitly structured quotations found in the fetched pages (no generic sentence, model, or fixture fallback)"; run.report = log.str(); return run; }
		documents.clear();
		for (const auto & quote : structuredQuotes) {
			documents.push_back({ quote.url, "Quote attributed to " + c.person + ": " + quote.text, { "web", "quote" }, 1.0 });
		}
	}
	ofxGgmlRagRequest request; request.query = c.quoteMode ? c.person : c.query;
	const auto desiredHits = std::min<std::size_t>(5, documents.size());
	ofxGgmlRagRetrievalOptions options;
	options.search.topK = std::max<std::size_t>(desiredHits, std::min<std::size_t>(desiredHits * 4, 20));
	options.search.allowQueryRefinement = true;
	options.search.maxRefinementQueries = 2;
	options.search.phraseBoost = 0.25;
	options.search.qualityWeight = 0.15;
	auto retrieval = ofxGgmlRagUtils::retrieve(request, documents, options);
	if (!retrieval) { run.error = retrieval.result.error; run.report = log.str(); return run; }
	if (c.useEmbeddings) {
		std::string embeddingError;
		if (!hybridRerank(retrieval, request.query, c, embeddingError)) {
			run.error = "embedding reranking failed: " + embeddingError;
			run.report = log.str();
			return run;
		}
		log << "[EMBEDDING] hybrid rerank candidates=" << retrieval.hits.size()
			<< " endpoint=" << c.embeddingEndpoint << " model=" << c.embeddingModel
			<< " semanticWeight=" << c.embeddingWeight << "\n";
	}
	diversifyRetrieval(retrieval, request.query, desiredHits);
	log << "[RETRIEVAL] documents=" << documents.size() << " hits=" << retrieval.hits.size() << "\n";
	if (c.quoteMode) {
		log << "[VERBATIM SOURCE EXCERPTS — ATTRIBUTION REQUIRES SOURCE REVIEW]\n";
		for (std::size_t i = 0; i < structuredQuotes.size(); ++i) {
			log << "[QUOTE " << (i + 1) << "] " << structuredQuotes[i].text << "\nURL: " << structuredQuotes[i].url << "\n";
		}
	} else {
		for (std::size_t i = 0; i < retrieval.hits.size(); ++i) log << "[CITATION " << (i + 1) << "] " << retrieval.hits[i].chunk.source << "\n";
		log << ofxGgmlRagUtils::formatRetrieval(retrieval) << "\n";
	}
	if (c.useModel) {
		if (c.model.empty()) { run.error = "model generation requested but model is empty"; run.report = log.str(); return run; }
		ofxGgmlRagPromptOptions promptOptions;
		if (c.quoteMode) promptOptions.systemInstruction = "Summarize the themes in the cited source excerpts. Do not invent, reconstruct, or present any text as a quotation.";
		if (c.strictJsonAnswer) promptOptions.systemInstruction += " Return a JSON object with answer and citation_ids. Every citation id must refer to a numbered cited-context item.";
		auto prompt = ofxGgmlRagUtils::buildPrompt(request.query, retrieval, promptOptions); std::string modelError; auto answer = modelAnswer(c, prompt.prompt, retrieval.context.citations.size(), modelError);
		if (answer.empty()) { run.error = "model generation failed: " + modelError; run.report = log.str(); return run; }
		log << (c.quoteMode ? "[MODEL SUMMARY — NOT A QUOTE]" : "[MODEL]") << " endpoint=" << c.modelEndpoint << " model=" << c.model
			<< " structured=" << (c.strictJsonAnswer ? "strict-json-schema" : "text") << "\n" << answer << "\nSources:\n";
		std::set<std::string> modelSources;
		for (const auto & citation : retrieval.context.citations) modelSources.insert(citation.source);
		for (const auto & source : modelSources) log << "- " << source << "\n";
	} else log << "[MODEL] skipped (enable explicitly and provide a model)\n";
	run.success = true; run.report = log.str(); return run;
}
