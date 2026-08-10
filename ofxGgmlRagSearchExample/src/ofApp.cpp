#include "ofApp.h"

#include "imgui_stdlib.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace {
	std::string GetEnvText(const char * name) {
#if defined(_WIN32)
		char * value = nullptr;
		std::size_t valueSize = 0;
		if (_dupenv_s(&value, &valueSize, name) != 0 || value == nullptr) {
			return "";
		}
		std::string text(value, valueSize > 0 ? valueSize - 1 : 0);
		std::free(value);
		return ofxGgmlRagUtils::trim(text);
#else
		const auto value = std::getenv(name);
		if (value == nullptr) {
			return "";
		}
		return ofxGgmlRagUtils::trim(value);
#endif
	}

	std::vector<ofxGgmlRagDocument> BuiltInDocuments() {
		return {
			{
				"example/in-memory.md",
				"RAG citation memory stays local until a user source root is configured.",
				{ "example" },
				0.9
			},
			{
				"example/workflow.md",
				"Local text corpus retrieval can load markdown and text files without embeddings or indexes.",
				{ "example", "workflow" },
				0.65
			}
		};
	}

	std::vector<std::string> SplitVariants(const std::string & value) {
		std::vector<std::string> variants;
		std::string current;
		for (const auto ch : value) {
			if (ch == ',' || ch == ';' || ch == '\n') {
				const auto cleaned = ofxGgmlRagUtils::trim(current);
				if (!cleaned.empty()) {
					variants.push_back(cleaned);
				}
				current.clear();
				continue;
			}
			current.push_back(ch);
		}
		const auto cleaned = ofxGgmlRagUtils::trim(current);
		if (!cleaned.empty()) {
			variants.push_back(cleaned);
		}
		return variants;
	}

	std::string FormatCitationSearch(const ofxGgmlRagCitationSearchResult & result) {
		if (!result) {
			return result.error;
		}
		std::ostringstream out;
		out << "citations=" << result.citations.size()
			<< " averageConfidence=" << result.averageConfidence
			<< " sourceDiversity=" << result.sourceDiversityScore << "\n\n";
		for (std::size_t i = 0; i < result.citations.size(); ++i) {
			const auto & citation = result.citations[i];
			out << "[" << (i + 1) << "] " << citation.sourceLabel
				<< " confidence=" << citation.confidenceScore
				<< " relevance=" << citation.relevanceScore << "\n"
				<< citation.quote << "\n\n";
		}
		return out.str();
	}
}

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlRag search example");
	gui.setup(nullptr, false);

	queryInput = GetEnvText("OFXGGML_RAG_QUERY");
	if (queryInput.empty()) {
		queryInput = "citation memory";
	}
	sourceRootInput = GetEnvText("OFXGGML_RAG_SOURCE_ROOT");
	rag.getRetrievalOptions().context.includeScores = true;
	runRetrieval();
}

void ofApp::runRetrieval() {
	rag.setQuery(queryInput);
	rag.getRetrievalOptions().search.topK = static_cast<std::size_t>(std::max(1, topK));
	rag.getRetrievalOptions().search.queryVariants = SplitVariants(queryVariantsInput);
	rag.getRetrievalOptions().search.qualityWeight = useQualityRanking ? 0.15 : 0.0;
	rag.getRetrievalOptions().context.includeQuery = true;

	useBuiltInDocument = ofxGgmlRagUtils::trim(sourceRootInput).empty();
	if (useBuiltInDocument) {
		rag.setDocuments(BuiltInDocuments(), "example");
	} else {
		rag.clearDocuments();
		rag.setSourceRoot(sourceRootInput);
	}
	rag.retrieve();

	ofxGgmlRagReportOptions reportOptions;
	reportOptions.includeContext = includeContext;
	reportOptions.maxHits = rag.getRetrievalOptions().search.topK;
	report = rag.format(reportOptions);
	status = rag.summarize();
	const auto prompt = rag.buildPrompt();
	promptText = prompt ? prompt.prompt : prompt.error;
	const auto answer = rag.draftAnswer();
	answerText = answer ? answer.text : answer.error;
	citationsText = FormatCitationSearch(rag.findCitations());
	if (useBuiltInDocument) {
		status += "; using built-in documents";
	}
}

void ofApp::runWebRetrieval() {
	webResult = runWebSearch(webConfig);
	status = webResult.success ? "Live web pipeline completed" : webResult.error;
	if (!webResult.success) {
		ofLogError("ofxGgmlRagSearchExample") << webResult.error;
	}
}

bool ofApp::inputTextWithPaste(const char * label, std::string & value) {
	bool changed = ImGui::InputText(label, &value);
	if (ImGui::IsItemActive() && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_V)) {
		value = ofGetWindowPtr()->getClipboardString();
		changed = true;
	}
	ImGui::SameLine();
	const std::string buttonLabel = std::string("Paste##") + label;
	if (ImGui::SmallButton(buttonLabel.c_str())) {
		value = ofGetWindowPtr()->getClipboardString();
		changed = true;
	}
	return changed;
}

void ofApp::draw() {
	ofBackground(18);

	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(760.0f, 500.0f), ImGuiCond_Once);
	if (ImGui::Begin("ofxGgmlRag Search Example")) {
		ImGui::TextUnformatted("Retrieval Request");
		ImGui::Separator();
		if (ImGui::RadioButton("Local corpus", !webMode)) webMode = false;
		ImGui::SameLine();
		if (ImGui::RadioButton("Live web", webMode)) webMode = true;
		if (!webMode) {
			ImGui::InputText("Query", &queryInput);
			ImGui::InputText("Variants", &queryVariantsInput);
			inputTextWithPaste("Source root", sourceRootInput);
			ImGui::SliderInt("Top K", &topK, 1, 10);
			ImGui::Checkbox("Context", &includeContext);
			ImGui::Checkbox("Quality rank", &useQualityRanking);
		} else {
			ImGui::Checkbox("Person / quotes", &webConfig.quoteMode);
			if (webConfig.quoteMode) inputTextWithPaste("Person", webConfig.person);
			else ImGui::InputText("Web query", &webConfig.query);
			inputTextWithPaste("Search URL template", webConfig.searchUrlTemplate);
			inputTextWithPaste("User-Agent", webConfig.userAgent);
			ImGui::SliderInt("Timeout seconds", &webConfig.timeoutSeconds, 2, 30);
			int results = static_cast<int>(webConfig.limits.maxSearchResults), pages = static_cast<int>(webConfig.limits.maxPages), depth = static_cast<int>(webConfig.limits.maxDepth);
			if (ImGui::SliderInt("Search results", &results, 1, 10)) webConfig.limits.maxSearchResults = results;
			if (ImGui::SliderInt("Pages", &pages, 1, 10)) webConfig.limits.maxPages = pages;
			if (ImGui::SliderInt("Same-origin depth", &depth, 0, 2)) webConfig.limits.maxDepth = depth;
			ImGui::Checkbox("Generate via OpenAI-compatible endpoint", &webConfig.useModel);
			inputTextWithPaste("Model alias / path", webConfig.model);
			inputTextWithPaste("Chat completions endpoint", webConfig.modelEndpoint);
		}
		if (ImGui::Button("Run")) {
			if (webMode) runWebRetrieval(); else runRetrieval();
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("Status");
		ImGui::Separator();
		ImGui::TextWrapped("%s", status.c_str());
		if (!webMode) ImGui::Text("documents=%zu scoped=%zu skipped=%zu chunks=%zu hits=%zu cache=%s",
			rag.getLastRetrieval().stats.documentCount,
			rag.getLastRetrieval().stats.scopedDocumentCount,
			rag.getLastRetrieval().stats.skippedDocumentCount,
			rag.getLastRetrieval().stats.chunkCount,
			rag.getLastRetrieval().stats.hitCount,
			rag.getLastRetrieval().stats.cacheHit ? "hit" : "miss");

		ImGui::Spacing();
		if (webMode) {
			ImGui::BeginChild("web-evidence", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
			ImGui::TextUnformatted(webResult.report.c_str());
			ImGui::EndChild();
		} else if (ImGui::BeginTabBar("rag-output")) {
			if (ImGui::BeginTabItem("Retrieval")) {
				ImGui::BeginChild("report", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
				ImGui::TextWrapped("%s", report.c_str());
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("LLM Prompt")) {
				ImGui::BeginChild("prompt", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
				ImGui::TextWrapped("%s", promptText.c_str());
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Answer Draft")) {
				ImGui::BeginChild("answer", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
				ImGui::TextWrapped("%s", answerText.c_str());
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Citations")) {
				ImGui::BeginChild("citations", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
				ImGui::TextWrapped("%s", citationsText.c_str());
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
	gui.end();
	gui.draw();
}
