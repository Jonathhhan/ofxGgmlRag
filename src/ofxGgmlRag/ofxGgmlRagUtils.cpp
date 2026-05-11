#include "ofxGgmlRagUtils.h"

namespace ofxGgmlRagUtils {
	bool hasInput(const ofxGgmlRagRequest & request) {
		return !request.query.empty();
	}

	std::string describe(const ofxGgmlRagRequest & request) {
		if (!hasInput(request)) {
			return "rag: empty request";
		}
		return "rag: " + request.query;
	}
}