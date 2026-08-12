meta:
	ADDON_NAME = ofxGgmlRag
	ADDON_DESCRIPTION = Standalone addon for local retrieval, web crawl, and citation workflows
	ADDON_AUTHOR = Jonathan Frank
	ADDON_TAGS = "ggml,ai,rag,citations,search"
	ADDON_URL = https://github.com/Jonathhhan/ofxGgmlRag

common:
	ADDON_INCLUDES += src
	ADDON_SOURCES_EXCLUDE += build/%
	ADDON_SOURCES_EXCLUDE += libs/*/build/%
	ADDON_SOURCES_EXCLUDE += libs/*/build*/%
	ADDON_INCLUDES_EXCLUDE += build/%
	ADDON_INCLUDES_EXCLUDE += libs/*/build/%
	ADDON_INCLUDES_EXCLUDE += libs/*/build*/%
