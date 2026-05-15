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

Write-Step "RAG runtime smoke contract passed"
