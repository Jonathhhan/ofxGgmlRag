#include "WebSearchRunner.h"
#include "ofMain.h"
#include "ofxGgmlRag.h"

#include <deque>
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

std::string modelAnswer(const WebSearchConfig & c, const std::string & prompt, std::string & error) {
	static ofURLFileLoader loader;
	ofJson body = { {"model", c.model}, {"messages", {{{"role", "user"}, {"content", prompt}}}}, {"temperature", 0} };
	ofHttpRequest request(c.modelEndpoint, "model", false);
	request.method = ofHttpRequest::POST; request.timeoutSeconds = c.timeoutSeconds;
	request.headers["Content-Type"] = "application/json"; request.body = body.dump();
	auto response = loader.handleRequest(request);
	if (response.status < 200 || response.status >= 300) { error = "HTTP " + ofToString(response.status) + ": " + response.error; return ""; }
	try { return ofJson::parse(response.data.getText())["choices"][0]["message"]["content"].get<std::string>(); }
	catch (const std::exception & e) { error = e.what(); return ""; }
}
}

WebSearchRun runWebSearch(const WebSearchConfig & c) {
	WebSearchRun run; std::ostringstream log;
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
	std::vector<ofxGgmlRagDocument> documents; std::vector<ragWebExample::QuoteHit> structuredQuotes; std::size_t totalBytes = 0;
	while (!queue.empty() && documents.size() < c.limits.maxPages) {
		auto item = queue.front(); queue.pop_front(); const auto site = origin(item.first);
		if (!robotsLoaded.count(site)) { auto rr = get(site + "/robots.txt", c); robots[site] = rr.status == 200 ? rr.data.getText() : ""; robotsLoaded.insert(site); log << "[SCRAPE] robots " << site << " status=" << rr.status << "\n"; }
		if (!ofxGgmlRagUtils::robotsTxtAllows(item.first, robots[site], c.userAgent)) { log << "[SCRAPE] blocked by robots.txt " << item.first << "\n"; continue; }
		auto page = get(item.first, c); if (page.status < 200 || page.status >= 300) { log << "[SCRAPE] HTTP " << page.status << " " << item.first << "\n"; continue; }
		const auto html = page.data.getText(); std::string reason;
		if (!ragWebExample::withinBounds(documents.size(), totalBytes, item.second, html.size(), c.limits, reason)) { log << "[SCRAPE] skipped " << reason << " " << item.first << "\n"; continue; }
		ofxGgmlRagHtmlOptions options; options.maxHtmlBytes = c.limits.maxBytesPerPage;
		auto converted = ofxGgmlRagUtils::documentFromHtml(item.first, html, options);
		if (!converted) { log << "[SCRAPE] skipped " << converted.error << " " << item.first << "\n"; continue; }
		totalBytes += html.size(); documents.push_back(converted.document); log << "[SCRAPE] accepted bytes=" << html.size() << " url=" << item.first << "\n";
		if (c.quoteMode) {
			const auto pageQuotes = ragWebExample::extractStructuredQuotes(item.first, html, c.person, 2);
			structuredQuotes.insert(structuredQuotes.end(), pageQuotes.begin(), pageQuotes.end());
			log << "[SCRAPE] structuredQuotes=" << pageQuotes.size() << " url=" << item.first << "\n";
		}
		if (item.second < c.limits.maxDepth) { ofxGgmlRagHtmlLinkOptions links; links.robotsTxt = robots[site]; links.robotsTxtUserAgent = c.userAgent; links.maxLinks = c.limits.maxPages; for (const auto & url : ofxGgmlRagUtils::extractHtmlLinks(item.first, html, links)) if (queued.insert(url).second) queue.push_back({url, item.second + 1}); }
	}
	if (documents.empty()) { run.error = "no page passed HTTP, robots, byte, and HTML ingestion checks"; run.report = log.str(); return run; }
	ofxGgmlRagRequest request; request.query = c.quoteMode ? c.person : c.query;
	ofxGgmlRagRetrievalOptions options; options.search.topK = std::min<std::size_t>(3, documents.size()); options.search.allowQueryRefinement = false;
	auto retrieval = ofxGgmlRagUtils::retrieve(request, documents, options);
	log << "[RETRIEVAL] documents=" << documents.size() << " hits=" << retrieval.hits.size() << "\n";
	if (c.quoteMode) {
		if (structuredQuotes.empty()) { run.error = "no explicitly structured quotations found in the fetched pages (no generic sentence, model, or fixture fallback)"; run.report = log.str(); return run; }
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
		auto prompt = ofxGgmlRagUtils::buildPrompt(request.query, retrieval, promptOptions); std::string modelError; auto answer = modelAnswer(c, prompt.prompt, modelError);
		if (answer.empty()) { run.error = "model generation failed: " + modelError; run.report = log.str(); return run; }
		log << (c.quoteMode ? "[MODEL SUMMARY — NOT A QUOTE]" : "[MODEL]") << " endpoint=" << c.modelEndpoint << " model=" << c.model << "\n" << answer << "\nSources:\n";
		std::set<std::string> modelSources;
		for (const auto & citation : retrieval.context.citations) modelSources.insert(citation.source);
		for (const auto & source : modelSources) log << "- " << source << "\n";
	} else log << "[MODEL] skipped (enable explicitly and provide a model)\n";
	run.success = true; run.report = log.str(); return run;
}
