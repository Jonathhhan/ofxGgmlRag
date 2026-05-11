#pragma once

#include <string>
#include <vector>

struct ofxGgmlRagRequest {
	std::string query;
	std::string sourceRoot;
	std::vector<std::string> tags;
};

struct ofxGgmlRagResult {
	bool success = false;
	std::string text;
	std::string error;
	std::vector<std::string> references;

	explicit operator bool() const {
		return success;
	}
};