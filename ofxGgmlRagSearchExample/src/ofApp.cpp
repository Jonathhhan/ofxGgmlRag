#include "ofApp.h"

#include "imgui_stdlib.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace {
	void TraceStartup(const std::string & message) {
		std::ofstream trace("ofxGgmlRagSearchExample-startup.log", std::ios::app);
		trace << message << "\n";
	}

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
				{ "example" }
			},
			{
				"example/workflow.md",
				"Local text corpus retrieval can load markdown and text files without embeddings or indexes.",
				{ "example", "workflow" }
			}
		};
	}
}

void ofApp::setup() {
	TraceStartup("setup: begin");
	ofSetWindowTitle("ofxGgmlRag search example");
	TraceStartup("setup: before gui");
	gui.setup(nullptr, false);
	TraceStartup("setup: after gui");

	queryInput = GetEnvText("OFXGGML_RAG_QUERY");
	TraceStartup("setup: after query env");
	if (queryInput.empty()) {
		queryInput = "citation memory";
	}
	sourceRootInput = GetEnvText("OFXGGML_RAG_SOURCE_ROOT");
	retrievalOptions.context.includeScores = true;
	TraceStartup("setup: before runRetrieval");
	runRetrieval();
	TraceStartup("setup: end");
}

void ofApp::runRetrieval() {
	TraceStartup("runRetrieval: begin");
	ofxGgmlRagRequest request;
	request.query = queryInput;
	request.sourceRoot = sourceRootInput;
	retrievalOptions.search.topK = static_cast<std::size_t>(std::max(1, topK));
	retrievalOptions.context.includeQuery = true;

	useBuiltInDocument = ofxGgmlRagUtils::trim(sourceRootInput).empty();
	if (useBuiltInDocument) {
		TraceStartup("runRetrieval: before built-in retrieve");
		request.sourceRoot = "example";
		retrieval = ofxGgmlRagUtils::retrieve(request, BuiltInDocuments(), retrievalOptions);
		TraceStartup("runRetrieval: after built-in retrieve");
	} else {
		TraceStartup("runRetrieval: before text corpus retrieve");
		retrieval = ofxGgmlRagUtils::retrieveTextCorpus(request, ofxGgmlRagCorpusOptions(), retrievalOptions);
		TraceStartup("runRetrieval: after text corpus retrieve");
	}

	ofxGgmlRagReportOptions reportOptions;
	reportOptions.includeContext = includeContext;
	reportOptions.maxHits = retrievalOptions.search.topK;
	report = ofxGgmlRagUtils::formatRetrieval(retrieval, reportOptions);
	status = ofxGgmlRagUtils::summarize(retrieval);
	if (useBuiltInDocument) {
		status += "; using built-in documents";
	}
	TraceStartup("runRetrieval: end");
}

void ofApp::draw() {
	static bool tracedDraw = false;
	if (!tracedDraw) {
		TraceStartup("draw: begin");
		tracedDraw = true;
	}
	ofBackground(18);

	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(760.0f, 500.0f), ImGuiCond_Once);
	if (ImGui::Begin("ofxGgmlRag Search Example")) {
		ImGui::TextUnformatted("Retrieval Request");
		ImGui::Separator();
		ImGui::InputText("Query", &queryInput);
		ImGui::InputText("Source root", &sourceRootInput);
		ImGui::SliderInt("Top K", &topK, 1, 10);
		ImGui::Checkbox("Context", &includeContext);
		if (ImGui::Button("Run")) {
			runRetrieval();
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("Status");
		ImGui::Separator();
		ImGui::TextWrapped("%s", status.c_str());
		ImGui::Text("documents=%zu scoped=%zu skipped=%zu chunks=%zu hits=%zu",
			retrieval.stats.documentCount,
			retrieval.stats.scopedDocumentCount,
			retrieval.stats.skippedDocumentCount,
			retrieval.stats.chunkCount,
			retrieval.stats.hitCount);

		ImGui::Spacing();
		ImGui::TextUnformatted("Retrieval Report");
		ImGui::Separator();
		ImGui::BeginChild("report", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::TextWrapped("%s", report.c_str());
		ImGui::EndChild();
	}
	ImGui::End();
	gui.end();
	gui.draw();
}
