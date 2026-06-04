#include "ofMain.h"
#include "ofApp.h"

#include <fstream>

namespace {
	void TraceStartup(const std::string & message) {
		std::ofstream trace("ofxGgmlRagSearchExample-startup.log", std::ios::app);
		trace << message << "\n";
	}
}

int main() {
	TraceStartup("main: before ofSetupOpenGL");
	ofSetupOpenGL(960, 540, OF_WINDOW);
	TraceStartup("main: before ofRunApp");
	ofRunApp(new ofApp());
	TraceStartup("main: after ofRunApp");
}
