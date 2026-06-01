param()

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Assert-Path {
	param(
		[string]$Path,
		[string]$Label,
		[switch]$Directory
	)

	if ($Directory) {
		if (!(Test-Path -LiteralPath $Path -PathType Container)) {
			throw "$Label was not found: $Path"
		}
	} elseif (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
		throw "$Label was not found: $Path"
	}
}

function Assert-FileContains {
	param(
		[string]$Path,
		[string]$Pattern,
		[string]$Label
	)

	$content = Get-Content -LiteralPath $Path -Raw
	if ($content -notmatch $Pattern) {
		throw "$Label did not contain expected pattern: $Pattern"
	}
}
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Split-Path -Parent $scriptRoot
$addonsRoot = Split-Path -Parent $addonRoot

Write-Step "Checking addon skeleton"
Assert-Path (Join-Path $addonRoot "addon_config.mk") "addon config"
Assert-Path (Join-Path $addonRoot "README.md") "README"
Assert-Path (Join-Path $addonRoot "LICENSE") "license"
Assert-Path (Join-Path $addonRoot "docs\RAG_WORKFLOWS.md") "RAG workflow docs"
Assert-FileContains (Join-Path $addonRoot "README.md") "docs/RAG_WORKFLOWS.md" "README"
Assert-FileContains (Join-Path $addonRoot "docs\RAG_WORKFLOWS.md") "Planning handoff" "RAG workflow docs"
Assert-FileContains (Join-Path $addonRoot "docs\RAG_WORKFLOWS.md") "Validation ladder" "RAG workflow docs"
Assert-FileContains (Join-Path $addonRoot "docs\RAG_WORKFLOWS.md") "generated artifacts" "RAG workflow docs"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlRag.h") "public header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlRagVersion.h") "version header"
Assert-FileContains (Join-Path $addonRoot "src\ofxGgmlRag.h") "ofxGgmlRagVersion.h" "public header"
Assert-FileContains (Join-Path $addonRoot "src\ofxGgmlRagVersion.h") "OFXGGML_RAG_VERSION_STRING" "version header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlRag\ofxGgmlRagTypes.h") "types header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlRag\ofxGgmlRagUtils.h") "utility header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlRag\ofxGgmlRagUtils.cpp") "utility source"

Write-Step "Checking dependency layout"
Assert-Path (Join-Path $addonsRoot "ofxGgmlCore") "sibling ofxGgmlCore addon" -Directory
Assert-Path (Join-Path $addonsRoot "ofxImGui") "sibling ofxImGui addon for examples" -Directory

Write-Step "Checking example layout"
$exampleRoot = Join-Path $addonRoot "ofxGgmlRagSearchExample"
Assert-Path $exampleRoot "root-level smoke example" -Directory
Assert-Path (Join-Path $exampleRoot "addons.make") "smoke example addons.make"
Assert-FileContains (Join-Path $exampleRoot "addons.make") "(?m)^ofxImGui\s*$" "smoke example addons.make"
Assert-Path (Join-Path $exampleRoot "src\main.cpp") "smoke example main.cpp"
Assert-Path (Join-Path $exampleRoot "src\ofApp.h") "smoke example ofApp.h"
Assert-Path (Join-Path $exampleRoot "src\ofApp.cpp") "smoke example ofApp.cpp"
Assert-Path (Join-Path $addonRoot "tests\CMakeLists.txt") "test CMakeLists"
Assert-Path (Join-Path $addonRoot "tests\test_main.cpp") "test source"
Assert-Path (Join-Path $scriptRoot "doctor-rag.ps1") "RAG doctor script"
Assert-Path (Join-Path $scriptRoot "doctor-rag.bat") "RAG doctor Windows wrapper"
Assert-Path (Join-Path $scriptRoot "doctor-rag.sh") "RAG doctor shell wrapper"
Assert-Path (Join-Path $scriptRoot "test-doctor-rag.ps1") "RAG doctor smoke test"
Assert-Path (Join-Path $scriptRoot "run-rag-runtime-smoke.ps1") "RAG runtime smoke script"
Assert-Path (Join-Path $scriptRoot "run-rag-runtime-smoke.bat") "RAG runtime smoke Windows wrapper"
Assert-Path (Join-Path $scriptRoot "run-rag-runtime-smoke.sh") "RAG runtime smoke shell wrapper"
Assert-Path (Join-Path $scriptRoot "test-rag-runtime-smoke.ps1") "RAG runtime smoke contract test"

$nestedExamples = Join-Path $addonRoot "examples"
if (Test-Path -LiteralPath $nestedExamples -PathType Container) {
	throw "Examples should live at the addon root, not under: $nestedExamples"
}

Write-Step "Checking generated artifact hygiene"
$forbidden = @(
	"build",
	".vs",
	"ofxGgmlRagSearchExample\bin",
	"ofxGgmlRagSearchExample\obj",
	"ofxGgmlRagSearchExample\.vs",
	"models"
)

foreach ($relative in $forbidden) {
	$pathspec = $relative -replace "\\", "/"
	$tracked = git -C $addonRoot ls-files -- $pathspec
	if ($LASTEXITCODE -ne 0) {
		throw "Failed to inspect generated artifact path: $relative"
	}
	if ($tracked) {
		throw "Generated or local-only path should not be committed here: $relative"
	}
}

Write-Step "Checking RAG doctor"
& (Join-Path $scriptRoot "test-doctor-rag.ps1")
if (!$?) {
	throw "RAG doctor smoke test failed"
}

Write-Step "Checking RAG runtime smoke contract"
& (Join-Path $scriptRoot "test-rag-runtime-smoke.ps1")

Write-Step "Running headless tests"
& (Join-Path $scriptRoot "test-addon.ps1") -Clean
if ($LASTEXITCODE -ne 0) {
	throw "Headless tests failed with exit code $LASTEXITCODE"
}

Write-Step "ofxGgmlRag local validation passed"
