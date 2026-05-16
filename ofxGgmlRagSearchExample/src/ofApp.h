#pragma once

#include "ofMain.h"
#include "ofxGgmlRag.h"
#include "ofxImGui.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void draw() override;

private:
	ofxGgmlRagRequest request;
	std::string status;
	ofxImGui::Gui gui;
};
