param(
	[string]$Query = $(if ($env:OFXGGML_RAG_QUERY) { $env:OFXGGML_RAG_QUERY } else { "" }),
	[string]$SourceRoot = $(if ($env:OFXGGML_RAG_SOURCE_ROOT) { $env:OFXGGML_RAG_SOURCE_ROOT } else { "" }),
	[string]$Index = $(if ($env:OFXGGML_RAG_INDEX) { $env:OFXGGML_RAG_INDEX } else { "" }),
	[string]$EmbeddingModel = $(if ($env:OFXGGML_RAG_EMBEDDING_MODEL) { $env:OFXGGML_RAG_EMBEDDING_MODEL } else { "" }),
	[switch]$Json,
	[switch]$Strict
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$addonsRoot = Split-Path -Parent $addonRoot
$script:Warnings = 0

function New-Check {
	param(
		[string]$State,
		[string]$Name,
		[string]$Detail = ""
	)
	if ($State -eq "WARN") {
		$script:Warnings++
	}
	return [pscustomobject]@{
		State = $State
		Name = $Name
		Detail = $Detail
	}
}

function Test-CommandAvailable {
	param([string]$Name)
	return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Test-PathCheck {
	param(
		[string]$Path,
		[string]$Name,
		[string]$MissingDetail,
		[switch]$Directory
	)
	$exists = if ($Directory) {
		Test-Path -LiteralPath $Path -PathType Container
	} else {
		Test-Path -LiteralPath $Path -PathType Leaf
	}
	if ($exists) {
		return New-Check "OK" $Name $Path
	}
	return New-Check "WARN" $Name $MissingDetail
}

function Test-ConfiguredFile {
	param(
		[string]$Path,
		[string]$Name,
		[string]$Hint
	)
	if ([string]::IsNullOrWhiteSpace($Path)) {
		return New-Check "WARN" $Name $Hint
	}
	$expanded = [Environment]::ExpandEnvironmentVariables($Path)
	if (Test-Path -LiteralPath $expanded -PathType Leaf) {
		return New-Check "OK" $Name $expanded
	}
	return New-Check "WARN" $Name "configured path was not found: $expanded"
}

function Test-ConfiguredDirectory {
	param(
		[string]$Path,
		[string]$Name,
		[string]$Hint
	)
	if ([string]::IsNullOrWhiteSpace($Path)) {
		return New-Check "WARN" $Name $Hint
	}
	$expanded = [Environment]::ExpandEnvironmentVariables($Path)
	if (Test-Path -LiteralPath $expanded -PathType Container) {
		return New-Check "OK" $Name $expanded
	}
	return New-Check "WARN" $Name "configured path was not found: $expanded"
}

function Get-TextCorpusSummary {
	param([string]$Path)
	$expanded = [Environment]::ExpandEnvironmentVariables($Path)
	if ([string]::IsNullOrWhiteSpace($expanded) -or !(Test-Path -LiteralPath $expanded -PathType Container)) {
		return $null
	}

	$supportedExtensions = @(".md", ".txt")
	$files = @(Get-ChildItem -LiteralPath $expanded -Recurse -File -ErrorAction SilentlyContinue)
	$supported = @($files | Where-Object { $supportedExtensions -contains $_.Extension.ToLowerInvariant() })
	return [pscustomobject]@{
		FileCount = $files.Count
		SupportedFileCount = $supported.Count
		SkippedFileCount = [Math]::Max(0, $files.Count - $supported.Count)
		Extensions = $supportedExtensions -join ", "
	}
}

function Test-ForbiddenPath {
	param([string]$RelativePath)
	$path = Join-Path $addonRoot $RelativePath
	if (Test-Path -LiteralPath $path) {
		return New-Check "WARN" "artifact hygiene" "generated/local path exists: $RelativePath"
	}
	return $null
}

$checks = @()
$checks += New-Check "OK" "addon root" $addonRoot.Path

foreach ($tool in @("git", "cmake")) {
	if (Test-CommandAvailable $tool) {
		$checks += New-Check "OK" $tool ((Get-Command $tool).Source)
	} else {
		$checks += New-Check "WARN" $tool "not found in PATH"
	}
}

$checks += Test-PathCheck `
	-Path (Join-Path $addonsRoot "ofxGgmlCore") `
	-Name "ofxGgmlCore sibling" `
	-MissingDetail "clone beside ofxGgmlRag" `
	-Directory

$checks += Test-PathCheck `
	-Path (Join-Path $addonsRoot "ofxImGui") `
	-Name "ofxImGui" `
	-MissingDetail "install beside ofxGgmlRag before building the search example" `
	-Directory

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "ofxGgmlRagSearchExample\addons.make") `
	-Name "search example" `
	-MissingDetail "ofxGgmlRagSearchExample skeleton is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "ofxGgmlRagSearchExample\src\ofApp.cpp") `
	-Name "search example source" `
	-MissingDetail "search example source is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "src\ofxGgmlRag\ofxGgmlRagTypes.h") `
	-Name "RAG request types" `
	-MissingDetail "RAG request types header is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "src\ofxGgmlRag\ofxGgmlRagUtils.cpp") `
	-Name "RAG utilities" `
	-MissingDetail "RAG utility implementation is missing"

if (![string]::IsNullOrWhiteSpace($Query)) {
	$checks += New-Check "OK" "RAG query" $Query
} else {
	$checks += New-Check "WARN" "RAG query" "set OFXGGML_RAG_QUERY or pass -Query"
}

$checks += Test-ConfiguredDirectory `
	-Path $SourceRoot `
	-Name "RAG source root" `
	-Hint "set OFXGGML_RAG_SOURCE_ROOT or pass -SourceRoot"

$corpusSummary = Get-TextCorpusSummary -Path $SourceRoot
if ($null -ne $corpusSummary) {
	if ($corpusSummary.SupportedFileCount -gt 0) {
		$checks += New-Check "OK" "RAG text corpus" "$($corpusSummary.SupportedFileCount) supported text file(s), $($corpusSummary.SkippedFileCount) skipped; extensions: $($corpusSummary.Extensions)"
	} else {
		$checks += New-Check "WARN" "RAG text corpus" "source root contains no supported text files; extensions: $($corpusSummary.Extensions)"
	}
}

$checks += Test-ConfiguredFile `
	-Path $Index `
	-Name "RAG index" `
	-Hint "set OFXGGML_RAG_INDEX or pass -Index when an index backend is available"

$checks += Test-ConfiguredFile `
	-Path $EmbeddingModel `
	-Name "embedding model" `
	-Hint "set OFXGGML_RAG_EMBEDDING_MODEL or pass -EmbeddingModel when an embedding backend is available"

$artifactWarnings = @()
foreach ($relative in @(
	"build",
	".vs",
	"ofxGgmlRagSearchExample\bin",
	"ofxGgmlRagSearchExample\obj",
	"ofxGgmlRagSearchExample\.vs",
	"models"
)) {
	$warning = Test-ForbiddenPath -RelativePath $relative
	if ($null -ne $warning) {
		$artifactWarnings += $warning
	}
}
if ($artifactWarnings.Count -eq 0) {
	$checks += New-Check "OK" "artifact hygiene" "no generated/local paths detected"
} else {
	$checks += $artifactWarnings
}

if ($Json) {
	[pscustomobject]@{
		Root = $addonRoot.Path
		Warnings = $script:Warnings
		Checks = $checks
	} | ConvertTo-Json -Depth 5
} else {
	Write-Host "ofxGgmlRag doctor"
	Write-Host "Root  $addonRoot"
	Write-Host ""
	foreach ($check in $checks) {
		$line = "{0,-5} {1}" -f $check.State, $check.Name
		if (![string]::IsNullOrWhiteSpace($check.Detail)) {
			$line += " - $($check.Detail)"
		}
		Write-Host $line
	}
	Write-Host ""
	if ($script:Warnings -eq 0) {
		Write-Host "Doctor passed."
	} else {
		Write-Host "Doctor found $script:Warnings warning(s)."
	}
}

if ($Strict -and $script:Warnings -gt 0) {
	exit 1
}
