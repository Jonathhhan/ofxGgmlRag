#pragma once

#include "ofxGgmlRagTypes.h"

#include <string>

namespace ofxGgmlRagUtils {
	bool hasInput(const ofxGgmlRagRequest & request);
	std::string describe(const ofxGgmlRagRequest & request);
}