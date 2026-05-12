#pragma once

#define OFXGGML_RAG_VERSION_MAJOR 1
#define OFXGGML_RAG_VERSION_MINOR 0
#define OFXGGML_RAG_VERSION_PATCH 1
#define OFXGGML_RAG_VERSION_STRING "1.0.1"

inline const char * ofxGgmlRagGetVersionString() {
	return OFXGGML_RAG_VERSION_STRING;
}
