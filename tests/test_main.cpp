#include "ofxGgmlRag.h"

#include <iostream>

int main() {
	ofxGgmlRagRequest request;
	if (ofxGgmlRagUtils::hasInput(request)) {
		std::cerr << "empty request reported as configured\n";
		return 1;
	}

	request.query = "where is this idea documented?";
	if (!ofxGgmlRagUtils::hasInput(request)) {
		std::cerr << "configured request reported as empty\n";
		return 1;
	}

	const auto description = ofxGgmlRagUtils::describe(request);
	if (description.find(request.query) == std::string::npos) {
		std::cerr << "description did not include request input\n";
		return 1;
	}

	return 0;
}