#pragma once

#include "WebSearchSupport.h"
#include <string>

struct WebSearchConfig {
	std::string query = "openFrameworks creative coding framework";
	std::string searchUrlTemplate = "https://html.duckduckgo.com/html/?q={query}";
	std::string userAgent = "ofxGgmlRagWebSearchExample/1.0 (+https://github.com/jonathhhan/ofxGgmlRag)";
	std::string modelEndpoint = "http://127.0.0.1:8080/v1/chat/completions";
	std::string model = "";
	ragWebExample::Limits limits;
	int timeoutSeconds = 12;
	bool useModel = false;
};

struct WebSearchRun { bool success = false; std::string report; std::string error; };
WebSearchRun runWebSearch(const WebSearchConfig & config);
