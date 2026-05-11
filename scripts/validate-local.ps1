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
Assert-Path (Join-Path $addonRoot "src\ofxGgmlRag.h") "public header"
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
	$path = Join-Path $addonRoot $relative
	if (Test-Path -LiteralPath $path) {
		throw "Generated or local-only path should not be committed here: $relative"
	}
}

Write-Step "Running headless tests"
& (Join-Path $scriptRoot "test-addon.ps1")
if ($LASTEXITCODE -ne 0) {
	throw "Headless tests failed with exit code $LASTEXITCODE"
}

Write-Step "ofxGgmlRag local validation passed"