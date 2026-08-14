#include "ofApp.h"

#include "imgui_stdlib.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <sstream>
#include <utility>

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

	bool IsGgufPath(const std::string & path) {
		const auto dot = path.find_last_of('.');
		if (dot == std::string::npos) return false;
		auto extension = path.substr(dot + 1);
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		return extension == "gguf";
	}

	std::string CommandQuote(const std::string & value) {
		return "\"" + value + "\"";
	}

	std::string FindLlamaServerLauncher() {
		const auto candidate = (ofFilePath::getCurrentExeDirFS() /
			".." / ".." / ".." / "ofxGgmlLlama" / "scripts" / "start-llama-server.ps1").lexically_normal();
		return of::filesystem::exists(candidate) ? ofPathToString(candidate) : std::string();
	}
}

void ofApp::LocalRetrievalWorker::start() {
	if (!isThreadRunning()) {
		startThread();
	}
}

void ofApp::LocalRetrievalWorker::stop() {
	jobs.close();
	waitForThread(true);
	results.close();
}

bool ofApp::LocalRetrievalWorker::submit(LocalRetrievalJob job) {
	if (busy.exchange(true)) {
		return false;
	}
	const bool sent = jobs.send(std::move(job));
	if (!sent) {
		busy.store(false);
	}
	return sent;
}

bool ofApp::LocalRetrievalWorker::tryReceive(LocalRetrievalResult & result) {
	return results.tryReceive(result);
}

bool ofApp::LocalRetrievalWorker::isBusy() const {
	return busy.load();
}

void ofApp::LocalRetrievalWorker::threadedFunction() {
	LocalRetrievalJob job;
	while (jobs.receive(job)) {
		LocalRetrievalResult completed;
		const auto startedAt = std::chrono::steady_clock::now();
		try {
			rag.setQuery(job.query);
			rag.getRetrievalOptions().search.topK = static_cast<std::size_t>(std::max(1, job.topK));
			rag.getRetrievalOptions().search.queryVariants = SplitVariants(job.queryVariants);
			rag.getRetrievalOptions().search.qualityWeight = job.useQualityRanking ? 0.15 : 0.0;
			rag.getRetrievalOptions().search.phraseBoost = 0.25;
			rag.getRetrievalOptions().search.allowQueryRefinement = true;
			rag.getRetrievalOptions().search.maxRefinementQueries = 2;
			rag.getRetrievalOptions().context.includeQuery = true;
			rag.getRetrievalOptions().context.includeScores = true;

			const bool useBuiltInDocument = ofxGgmlRagUtils::trim(job.sourceRoot).empty();
			if (useBuiltInDocument) {
				if (!builtInDocumentsReady) {
					rag.setDocuments(BuiltInDocuments(), "example");
					builtInDocumentsReady = true;
				}
			} else {
				builtInDocumentsReady = false;
				rag.clearDocuments();
				rag.setSourceRoot(job.sourceRoot);
			}

			ofLogNotice("ofxGgmlRagSearchExample") << "local retrieval executing on ofThread worker";
			rag.retrieve();
			ofxGgmlRagReportOptions reportOptions;
			reportOptions.includeContext = job.includeContext;
			reportOptions.maxHits = rag.getRetrievalOptions().search.topK;
			completed.report = rag.format(reportOptions);
			completed.status = rag.summarize();
			const auto prompt = rag.buildPrompt();
			completed.prompt = prompt ? prompt.prompt : prompt.error;
			const auto answer = rag.draftAnswer();
			completed.answer = answer ? answer.text : answer.error;
			completed.citations = FormatCitationSearch(rag.findCitations());
			if (useBuiltInDocument) {
				completed.status += "; using built-in documents";
			}

			const auto & stats = rag.getLastRetrieval().stats;
			completed.documentCount = stats.documentCount;
			completed.scopedDocumentCount = stats.scopedDocumentCount;
			completed.skippedDocumentCount = stats.skippedDocumentCount;
			completed.chunkCount = stats.chunkCount;
			completed.hitCount = stats.hitCount;
			completed.cacheHit = stats.cacheHit;
		} catch (const std::exception & error) {
			completed.status = std::string("local retrieval worker failed: ") + error.what();
			ofLogError("ofxGgmlRagSearchExample") << completed.status;
		} catch (...) {
			completed.status = "local retrieval worker failed";
			ofLogError("ofxGgmlRagSearchExample") << completed.status;
		}
		completed.elapsedMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - startedAt).count();
		ofLogNotice("ofxGgmlRagSearchExample")
			<< "local retrieval completed on ofThread worker: documents=" << completed.documentCount
			<< " hits=" << completed.hitCount
			<< " cache=" << (completed.cacheHit ? "hit" : "miss")
			<< " elapsed=" << ofToString(completed.elapsedMs, 1) << " ms";
		results.send(std::move(completed));
		busy.store(false);
	}
	busy.store(false);
}

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlRag search example");
	gui.setup(nullptr, false);

	queryInput = GetEnvText("OFXGGML_RAG_QUERY");
	if (queryInput.empty()) {
		queryInput = "citation memory";
	}
	sourceRootInput = GetEnvText("OFXGGML_RAG_SOURCE_ROOT");
	localModelPath = GetEnvText("OFXGGML_TEXT_MODEL");
	if (!localModelPath.empty()) webConfig.model = ragWebExample::localModelAlias(localModelPath);
	localEmbeddingModelPath = GetEnvText("OFXGGML_EMBEDDING_MODEL");
	if (!localEmbeddingModelPath.empty()) webConfig.embeddingModel = ragWebExample::localModelAlias(localEmbeddingModelPath);
	auto embeddingServerUrl = GetEnvText("OFXGGML_EMBEDDING_SERVER_URL");
	while (!embeddingServerUrl.empty() && embeddingServerUrl.back() == '/') embeddingServerUrl.pop_back();
	if (!embeddingServerUrl.empty()) webConfig.embeddingEndpoint = embeddingServerUrl + "/v1/embeddings";
	localRetrievalWorker.start();
	runRetrieval();
}

void ofApp::update() {
	LocalRetrievalResult completed;
	while (localRetrievalWorker.tryReceive(completed)) {
		localRetrievalResult = std::move(completed);
		status = localRetrievalResult.status;
		report = localRetrievalResult.report;
		promptText = localRetrievalResult.prompt;
		answerText = localRetrievalResult.answer;
		citationsText = localRetrievalResult.citations;
	}

	if (modelServerStarting && modelServerLaunch.valid() &&
		modelServerLaunch.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		modelServerOutput = modelServerLaunch.get();
		modelServerStarting = false;
		if (modelServerOutput.find("OFXGGML_LLAMA_SERVER_READY=1") != std::string::npos) {
			status = "Selected local model is ready on llama-server port " + ofToString(localModelPort);
			webConfig.useModel = true;
		} else {
			status = "llama-server launch failed or did not report verified readiness; inspect Local model output";
			ofLogWarning("ofxGgmlRagSearchExample") << status << "\n" << modelServerOutput;
		}
	}
	if (embeddingServerStarting && embeddingServerLaunch.valid() &&
		embeddingServerLaunch.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		embeddingServerOutput = embeddingServerLaunch.get();
		embeddingServerStarting = false;
		if (embeddingServerOutput.find("OFXGGML_LLAMA_SERVER_READY=1") != std::string::npos) {
			status = "Selected local embedding model is ready on llama-server port " + ofToString(localEmbeddingPort);
			webConfig.useEmbeddings = true;
		} else {
			status = "embedding llama-server launch failed or did not report verified readiness; inspect Local embedding output";
			ofLogWarning("ofxGgmlRagSearchExample") << status << "\n" << embeddingServerOutput;
		}
	}
	if (webSearchRunning && webSearchLaunch.valid() &&
		webSearchLaunch.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		webResult = webSearchLaunch.get();
		webSearchRunning = false;
		status = webResult.success ? "Live web pipeline completed" : webResult.error;
		if (!webResult.success) ofLogError("ofxGgmlRagSearchExample") << webResult.error;
	}
}

void ofApp::exit() {
	localRetrievalWorker.stop();
}

void ofApp::runRetrieval() {
	LocalRetrievalJob job;
	job.query = queryInput;
	job.queryVariants = queryVariantsInput;
	job.sourceRoot = sourceRootInput;
	job.includeContext = includeContext;
	job.useQualityRanking = useQualityRanking;
	job.topK = topK;
	status = "Local retrieval running on ofThread worker...";
	if (!localRetrievalWorker.submit(std::move(job))) {
		status = "Local retrieval is already running on the ofThread worker.";
	}
}

void ofApp::runWebRetrieval() {
	if (webSearchRunning) return;
	const auto config = webConfig;
	webResult = WebSearchRun();
	webSearchRunning = true;
	status = "Searching and retrieving web sources...";
	webSearchLaunch = std::async(std::launch::async, [config]() { return runWebSearch(config); });
}

void ofApp::browseForLocalModel() {
	auto result = ofSystemLoadDialog("Select a local GGUF text model");
	if (!result.bSuccess) return;
	if (!IsGgufPath(result.getPath())) {
		status = "Choose a local .gguf text model";
		return;
	}
	localModelPath = result.getPath();
	webConfig.model = ragWebExample::localModelAlias(localModelPath);
	status = "Selected local model: " + ofFilePath::getFileName(localModelPath);
}

void ofApp::browseForLocalEmbeddingModel() {
	auto result = ofSystemLoadDialog("Select a local GGUF embedding model");
	if (!result.bSuccess) return;
	if (!IsGgufPath(result.getPath())) {
		status = "Choose a local .gguf embedding model";
		return;
	}
	localEmbeddingModelPath = result.getPath();
	webConfig.embeddingModel = ragWebExample::localModelAlias(localEmbeddingModelPath);
	status = "Selected local embedding model: " + ofFilePath::getFileName(localEmbeddingModelPath);
}

void ofApp::startLocalModelServer() {
	if (modelServerStarting) return;
	if (!IsGgufPath(localModelPath) || !ofFile::doesFileExist(localModelPath)) {
		status = "Select an existing local .gguf text model first";
		return;
	}
	const auto launcher = FindLlamaServerLauncher();
	if (launcher.empty()) {
		status = "Could not find sibling ofxGgmlLlama/scripts/start-llama-server.ps1";
		return;
	}
	webConfig.model = ragWebExample::localModelAlias(localModelPath);
	localModelPort = ofClamp(localModelPort, 1024, 65535);
	webConfig.modelEndpoint = "http://127.0.0.1:" + ofToString(localModelPort) + "/v1/chat/completions";
	const std::string command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File " +
		CommandQuote(launcher) + " -ModelPath " + CommandQuote(localModelPath) +
		" -Alias " + CommandQuote(webConfig.model) + " -Port " + ofToString(localModelPort) + " -Detached";
	modelServerStarting = true;
	modelServerOutput.clear();
	status = "Starting selected local model with llama-server...";
	modelServerLaunch = std::async(std::launch::async, [command]() { return ofSystem(command); });
}

void ofApp::startLocalEmbeddingServer() {
	if (embeddingServerStarting) return;
	if (!IsGgufPath(localEmbeddingModelPath) || !ofFile::doesFileExist(localEmbeddingModelPath)) {
		status = "Select an existing local .gguf embedding model first";
		return;
	}
	const auto launcher = FindLlamaServerLauncher();
	if (launcher.empty()) {
		status = "Could not find sibling ofxGgmlLlama/scripts/start-llama-server.ps1";
		return;
	}
	webConfig.embeddingModel = ragWebExample::localModelAlias(localEmbeddingModelPath);
	localEmbeddingPort = ofClamp(localEmbeddingPort, 1024, 65535);
	webConfig.embeddingEndpoint = "http://127.0.0.1:" + ofToString(localEmbeddingPort) + "/v1/embeddings";
	const std::string command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File " +
		CommandQuote(launcher) + " -ModelPath " + CommandQuote(localEmbeddingModelPath) +
		" -Alias " + CommandQuote(webConfig.embeddingModel) + " -Port " + ofToString(localEmbeddingPort) + " -Embeddings -Detached";
	embeddingServerStarting = true;
	embeddingServerOutput.clear();
	status = "Starting selected local embedding model with llama-server...";
	embeddingServerLaunch = std::async(std::launch::async, [command]() { return ofSystem(command); });
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
	ImGui::SetNextWindowSize(ImVec2(800.0f, 620.0f), ImGuiCond_Once);
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
			ImGui::SliderInt("Page timeout seconds", &webConfig.timeoutSeconds, 2, 30);
			ImGui::SliderInt("Total fetch seconds", &webConfig.totalFetchTimeoutSeconds, 5, 120);
			int results = static_cast<int>(webConfig.limits.maxSearchResults), pages = static_cast<int>(webConfig.limits.maxPages), depth = static_cast<int>(webConfig.limits.maxDepth);
			if (ImGui::SliderInt("Search results", &results, 1, 10)) webConfig.limits.maxSearchResults = results;
			if (ImGui::SliderInt("Pages", &pages, 1, 10)) webConfig.limits.maxPages = pages;
			if (ImGui::SliderInt("Same-origin depth", &depth, 0, 2)) webConfig.limits.maxDepth = depth;
			ImGui::SeparatorText("Local model (optional answer generation)");
			inputTextWithPaste("GGUF model", localModelPath);
			ImGui::SameLine();
			if (ImGui::SmallButton("Browse...##local-model")) browseForLocalModel();
			ImGui::InputInt("Local server port", &localModelPort);
			const bool modelServerStartDisabled = modelServerStarting;
			if (modelServerStartDisabled) ImGui::BeginDisabled();
			if (ImGui::Button("Start selected model locally")) startLocalModelServer();
			if (modelServerStartDisabled) ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::Checkbox("Generate answer", &webConfig.useModel);
			ImGui::Checkbox("Strict JSON answer", &webConfig.strictJsonAnswer);
			ImGui::SliderInt("Model timeout seconds", &webConfig.modelTimeoutSeconds, 5, 180);
			ImGui::SliderInt("Maximum answer tokens", &webConfig.maxModelTokens, 32, 1024);
			ImGui::TextWrapped("Starts the sibling ofxGgmlLlama llama-server on a dedicated port without stopping other servers. Choose another port if it is already occupied.");
			ImGui::SeparatorText("Local embeddings (optional hybrid reranking)");
			inputTextWithPaste("Embedding GGUF model", localEmbeddingModelPath);
			ImGui::SameLine();
			if (ImGui::SmallButton("Browse...##embedding-model")) browseForLocalEmbeddingModel();
			ImGui::InputInt("Embedding server port", &localEmbeddingPort);
			const bool embeddingServerStartDisabled = embeddingServerStarting;
			if (embeddingServerStartDisabled) ImGui::BeginDisabled();
			if (ImGui::Button("Start embedding model locally")) startLocalEmbeddingServer();
			if (embeddingServerStartDisabled) ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::Checkbox("Hybrid reranking", &webConfig.useEmbeddings);
			float embeddingWeight = static_cast<float>(webConfig.embeddingWeight);
			if (ImGui::SliderFloat("Semantic weight", &embeddingWeight, 0.0f, 1.0f, "%.2f")) webConfig.embeddingWeight = embeddingWeight;
			ImGui::TextWrapped("Embeds only the bounded lexical candidate set in memory, then combines lexical and cosine ranks. No index or fetched content is written.");
			if (ImGui::TreeNode("Compatible server settings")) {
				inputTextWithPaste("Model alias", webConfig.model);
				inputTextWithPaste("Chat completions endpoint", webConfig.modelEndpoint);
				inputTextWithPaste("Embedding model alias", webConfig.embeddingModel);
				inputTextWithPaste("Embeddings endpoint", webConfig.embeddingEndpoint);
				ImGui::TreePop();
			}
		}
		const bool operationRunning = webMode ? webSearchRunning : localRetrievalWorker.isBusy();
		if (operationRunning) ImGui::BeginDisabled();
		const char * runLabel = webMode
			? (webSearchRunning ? "Searching..." : "Run")
			: (localRetrievalWorker.isBusy() ? "Retrieving on ofThread..." : "Run");
		if (ImGui::Button(runLabel)) {
			if (webMode) runWebRetrieval(); else runRetrieval();
		}
		if (operationRunning) ImGui::EndDisabled();

		ImGui::Spacing();
		ImGui::TextUnformatted("Status");
		ImGui::Separator();
		ImGui::TextWrapped("%s", status.c_str());
		if (!modelServerOutput.empty() && ImGui::TreeNode("Local model output")) {
			ImGui::TextWrapped("%s", modelServerOutput.c_str());
			ImGui::TreePop();
		}
		if (!embeddingServerOutput.empty() && ImGui::TreeNode("Local embedding output")) {
			ImGui::TextWrapped("%s", embeddingServerOutput.c_str());
			ImGui::TreePop();
		}
		if (!webMode) ImGui::Text("documents=%zu scoped=%zu skipped=%zu chunks=%zu hits=%zu cache=%s elapsed=%.1fms",
			localRetrievalResult.documentCount,
			localRetrievalResult.scopedDocumentCount,
			localRetrievalResult.skippedDocumentCount,
			localRetrievalResult.chunkCount,
			localRetrievalResult.hitCount,
			localRetrievalResult.cacheHit ? "hit" : "miss",
			localRetrievalResult.elapsedMs);

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
