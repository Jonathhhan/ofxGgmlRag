#include "ofApp.h"

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlRag smoke example");
	gui.setup(nullptr, false);
	request.query = "where is this idea documented?";
	status = ofxGgmlRagUtils::describe(request);
	ofLogNotice("ofxGgmlRagSearchExample") << status;
}

void ofApp::draw() {
	ofBackground(18);
	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(560.0f, 220.0f), ImGuiCond_Once);
	if (ImGui::Begin("ofxGgmlRag Search Example")) {
		ImGui::TextUnformatted("Retrieval Request");
		ImGui::Separator();
		ImGui::TextWrapped("%s", status.c_str());
	}
	ImGui::End();
	gui.end();
	gui.draw();
}
