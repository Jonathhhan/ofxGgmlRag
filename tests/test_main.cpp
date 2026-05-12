#include "ofxGgmlRag.h"

#include <iostream>

int main() {
	if (OFXGGML_RAG_VERSION_MAJOR != 1 ||
		OFXGGML_RAG_VERSION_MINOR != 0 ||
		OFXGGML_RAG_VERSION_PATCH != 1 ||
		std::string(OFXGGML_RAG_VERSION_STRING) != "1.0.1" ||
		std::string(ofxGgmlRagGetVersionString()) != "1.0.1") {
		std::cerr << "unexpected RAG addon version metadata\n";
		return 1;
	}

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
