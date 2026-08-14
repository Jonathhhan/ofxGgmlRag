#pragma once

#include "ofMain.h"
#include "ofxGgmlRag.h"
#include "ofxImGui.h"
#include "WebSearchRunner.h"

#include <atomic>
#include <future>

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void exit() override;

private:
	struct LocalRetrievalJob {
		std::string query;
		std::string queryVariants;
		std::string sourceRoot;
		bool includeContext = true;
		bool useQualityRanking = true;
		int topK = 3;
	};

	struct LocalRetrievalResult {
		std::string status;
		std::string report;
		std::string prompt;
		std::string answer;
		std::string citations;
		std::size_t documentCount = 0;
		std::size_t scopedDocumentCount = 0;
		std::size_t skippedDocumentCount = 0;
		std::size_t chunkCount = 0;
		std::size_t hitCount = 0;
		bool cacheHit = false;
		double elapsedMs = 0.0;
	};

	class LocalRetrievalWorker : public ofThread {
	public:
		void start();
		void stop();
		bool submit(LocalRetrievalJob job);
		bool tryReceive(LocalRetrievalResult & result);
		bool isBusy() const;

	private:
		void threadedFunction() override;

		ofThreadChannel<LocalRetrievalJob> jobs;
		ofThreadChannel<LocalRetrievalResult> results;
		ofxGgmlRag rag;
		std::atomic<bool> busy{ false };
		bool builtInDocumentsReady = false;
	};

	void runRetrieval();
	void runWebRetrieval();
	void browseForLocalModel();
	void browseForLocalEmbeddingModel();
	void startLocalModelServer();
	void startLocalEmbeddingServer();
	bool inputTextWithPaste(const char * label, std::string & value);

	std::string queryInput;
	std::string queryVariantsInput;
	std::string sourceRootInput;
	std::string status;
	std::string report;
	std::string promptText;
	std::string answerText;
	std::string citationsText;
	LocalRetrievalWorker localRetrievalWorker;
	LocalRetrievalResult localRetrievalResult;
	bool includeContext = true;
	bool useQualityRanking = true;
	int topK = 3;
	ofxImGui::Gui gui;
	WebSearchConfig webConfig;
	WebSearchRun webResult;
	std::string localModelPath;
	std::string localEmbeddingModelPath;
	std::string modelServerOutput;
	std::string embeddingServerOutput;
	std::future<std::string> modelServerLaunch;
	std::future<std::string> embeddingServerLaunch;
	std::future<WebSearchRun> webSearchLaunch;
	int localModelPort = 8092;
	int localEmbeddingPort = 8093;
	bool modelServerStarting = false;
	bool embeddingServerStarting = false;
	bool webSearchRunning = false;
	bool webMode = false;
};
