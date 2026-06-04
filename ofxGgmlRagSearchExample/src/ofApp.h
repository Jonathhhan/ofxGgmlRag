#pragma once

#include "ofMain.h"
#include "ofxGgmlRag.h"
#include "ofxImGui.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void draw() override;

private:
	void runRetrieval();

	std::string queryInput;
	std::string sourceRootInput;
	std::string status;
	std::string report;
	std::string promptText;
	ofxGgmlRag rag;
	bool useBuiltInDocument = false;
	bool includeContext = true;
	int topK = 3;
	ofxImGui::Gui gui;
};
