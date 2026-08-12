#pragma once

#include "ofMain.h"
#include "ofxGgmlRag.h"
#include "ofxImGui.h"
#include "WebSearchRunner.h"

#include <future>

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;

private:
	void runRetrieval();
	void runWebRetrieval();
	void browseForLocalModel();
	void startLocalModelServer();
	bool inputTextWithPaste(const char * label, std::string & value);

	std::string queryInput;
	std::string queryVariantsInput;
	std::string sourceRootInput;
	std::string status;
	std::string report;
	std::string promptText;
	std::string answerText;
	std::string citationsText;
	ofxGgmlRag rag;
	bool useBuiltInDocument = false;
	bool includeContext = true;
	bool useQualityRanking = true;
	int topK = 3;
	ofxImGui::Gui gui;
	WebSearchConfig webConfig;
	WebSearchRun webResult;
	std::string localModelPath;
	std::string modelServerOutput;
	std::future<std::string> modelServerLaunch;
	std::future<WebSearchRun> webSearchLaunch;
	int localModelPort = 8092;
	bool modelServerStarting = false;
	bool webSearchRunning = false;
	bool webMode = false;
};
