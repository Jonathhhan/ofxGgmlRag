#include "ofApp.h"

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlRag smoke example");
	request.query = "where is this idea documented?";
	status = ofxGgmlRagUtils::describe(request);
	ofLogNotice("ofxGgmlRagSearchExample") << status;
}

void ofApp::draw() {
	ofBackground(18);
	ofSetColor(240);
	ofDrawBitmapString("ofxGgmlRag", 32, 48);
	ofDrawBitmapString(status, 32, 78);
}