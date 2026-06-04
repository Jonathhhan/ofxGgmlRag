param()

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Assert-Contains {
	param(
		[string]$Text,
		[string]$Needle,
		[string]$Label
	)
	if (!$Text.Contains($Needle)) {
		throw "$Label did not contain expected text: $Needle`n$Text"
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$script = Join-Path $scriptRoot "run-rag-runtime-smoke.ps1"

Write-Step "RAG runtime smoke dry-run"
$textOutput = & $script -DryRun 2>&1 6>&1 | Out-String
Assert-Contains $textOutput "ofxGgmlRag runtime smoke plan" "runtime smoke dry-run"
Assert-Contains $textOutput "Backend: request-boundary" "runtime smoke dry-run"
Assert-Contains $textOutput "ModelBacked: False" "runtime smoke dry-run"
Assert-Contains $textOutput "IndexBacked: False" "runtime smoke dry-run"
Assert-Contains $textOutput "Dry run complete; no files were changed" "runtime smoke dry-run"

Write-Step "RAG runtime smoke JSON dry-run"
$jsonOutput = & $script -DryRun -Json -SummaryOnly 2>&1 6>&1 | Out-String
$summary = $jsonOutput | ConvertFrom-Json
if ($summary.Name -ne "ofxGgmlRag runtime smoke") {
	throw "Unexpected runtime smoke name: $($summary.Name)"
}
if ($summary.Backend -ne "request-boundary") {
	throw "Unexpected runtime smoke backend: $($summary.Backend)"
}
if ($summary.ModelBacked -or $summary.IndexBacked) {
	throw "RAG runtime smoke should not claim model-backed or index-backed retrieval yet."
}
if (!($summary.NextCommands -contains "scripts\run-rag-runtime-smoke.bat -Json -SummaryOnly")) {
	throw "JSON dry-run did not include the runtime smoke command."
}
if ($summary.TextCorpusBridge -or $summary.SourceRootConfigured) {
	throw "Default JSON dry-run should not report a configured text corpus bridge."
}

$corpusRoot = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlRag-runtime-smoke-corpus"
if (Test-Path -LiteralPath $corpusRoot) {
	Remove-Item -LiteralPath $corpusRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $corpusRoot | Out-Null
Set-Content -LiteralPath (Join-Path $corpusRoot "a.md") -Value "citation memory" -Encoding UTF8
Set-Content -LiteralPath (Join-Path $corpusRoot "skip.json") -Value "{""text"":""citation""}" -Encoding UTF8

Write-Step "RAG runtime smoke corpus JSON dry-run"
$corpusJsonOutput = & $script -DryRun -Json -SummaryOnly -Query "citation memory" -SourceRoot $corpusRoot 2>&1 6>&1 | Out-String
$corpusSummary = $corpusJsonOutput | ConvertFrom-Json
if (!$corpusSummary.TextCorpusBridge -or !$corpusSummary.SourceRootConfigured) {
	throw "Corpus JSON dry-run did not report text corpus bridge readiness."
}
if ($corpusSummary.ModelBacked -or $corpusSummary.IndexBacked) {
	throw "Corpus bridge dry-run should not claim model-backed or index-backed retrieval."
}

Write-Step "RAG runtime smoke corpus JSON run"
$corpusRunOutput = & $script -Json -SummaryOnly -Query "citation memory" -SourceRoot $corpusRoot 2>&1 6>&1 | Out-String
$corpusRunSummary = $corpusRunOutput | ConvertFrom-Json
if (!$corpusRunSummary.Passed -or !$corpusRunSummary.TextCorpusBridge -or $corpusRunSummary.ModelBacked -or $corpusRunSummary.IndexBacked) {
	throw "Corpus JSON run did not preserve request-boundary bridge summary."
}
if ($corpusRunSummary.ResultCount -ne 3) {
	throw "Corpus JSON run did not include helper tests, corpus retrieval, and doctor steps."
}

Remove-Item -LiteralPath $corpusRoot -Recurse -Force

Write-Step "RAG runtime smoke contract passed"
