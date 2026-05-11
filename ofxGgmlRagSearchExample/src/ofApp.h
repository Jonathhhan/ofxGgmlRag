#pragma once

#include "ofMain.h"
#include "ofxGgmlRag.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void draw() override;

private:
	ofxGgmlRagRequest request;
	std::string status;
};