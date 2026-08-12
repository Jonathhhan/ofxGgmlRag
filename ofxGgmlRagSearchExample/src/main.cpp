#include "ofMain.h"
#include "ofApp.h"

int main(int argc, char ** argv) {
	WebSearchConfig config;
	bool runOnce = false;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
		if (arg == "--run-once" || arg == "--headless") runOnce = true;
		else if (arg == "--query") config.query = next();
		else if (arg == "--person") { config.person = next(); config.quoteMode = true; }
		else if (arg == "--search-url-template") config.searchUrlTemplate = next();
		else if (arg == "--user-agent") config.userAgent = next();
		else if (arg == "--page-timeout") config.timeoutSeconds = std::stoi(next());
		else if (arg == "--total-fetch-timeout") config.totalFetchTimeoutSeconds = std::stoi(next());
		else if (arg == "--model") { config.model = next(); config.useModel = true; }
		else if (arg == "--model-endpoint") config.modelEndpoint = next();
		else if (arg == "--model-timeout") config.modelTimeoutSeconds = std::stoi(next());
		else if (arg == "--model-max-tokens") config.maxModelTokens = std::stoi(next());
		else if (arg == "--max-pages") config.limits.maxPages = static_cast<std::size_t>(std::stoul(next()));
	}
	if (runOnce) {
		auto result = runWebSearch(config);
		std::cout << result.report;
		if (!result.error.empty()) std::cerr << "ERROR: " << result.error << "\n";
		return result.success ? 0 : 1;
	}
	ofSetupOpenGL(960, 540, OF_WINDOW);
	ofRunApp(new ofApp());
}
