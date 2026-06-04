#include "ofApp.h"

#include "imgui_stdlib.h"

#include <algorithm>
#include <cstdlib>

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
	if (useBuiltInDocument) {
		status += "; using built-in documents";
	}
}

void ofApp::draw() {
	ofBackground(18);

	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(760.0f, 500.0f), ImGuiCond_Once);
	if (ImGui::Begin("ofxGgmlRag Search Example")) {
		ImGui::TextUnformatted("Retrieval Request");
		ImGui::Separator();
		ImGui::InputText("Query", &queryInput);
		ImGui::InputText("Variants", &queryVariantsInput);
		ImGui::InputText("Source root", &sourceRootInput);
		ImGui::SliderInt("Top K", &topK, 1, 10);
		ImGui::Checkbox("Context", &includeContext);
		ImGui::Checkbox("Quality rank", &useQualityRanking);
		if (ImGui::Button("Run")) {
			runRetrieval();
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("Status");
		ImGui::Separator();
		ImGui::TextWrapped("%s", status.c_str());
		ImGui::Text("documents=%zu scoped=%zu skipped=%zu chunks=%zu hits=%zu",
			rag.getLastRetrieval().stats.documentCount,
			rag.getLastRetrieval().stats.scopedDocumentCount,
			rag.getLastRetrieval().stats.skippedDocumentCount,
			rag.getLastRetrieval().stats.chunkCount,
			rag.getLastRetrieval().stats.hitCount);

		ImGui::Spacing();
		if (ImGui::BeginTabBar("rag-output")) {
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
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
	gui.end();
	gui.draw();
}
