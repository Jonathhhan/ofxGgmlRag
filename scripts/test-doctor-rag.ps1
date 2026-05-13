$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$doctorScript = Join-Path $scriptRoot "doctor-rag.ps1"

$output = & $doctorScript *>&1 | ForEach-Object { $_.ToString() }
if (!$?) {
	throw "doctor-rag.ps1 failed."
}

$text = $output -join "`n"
foreach ($expected in @(
	"ofxGgmlRag doctor",
	"ofxGgmlCore sibling",
	"search example",
	"RAG request types",
	"RAG query",
	"artifact hygiene"
)) {
	if ($text -notmatch [regex]::Escape($expected)) {
		throw "doctor output did not contain expected text: $expected"
	}
}

$jsonOutput = & $doctorScript -Json *>&1 | ForEach-Object { $_.ToString() }
if (!$?) {
	throw "doctor-rag.ps1 -Json failed."
}

$parsed = ($jsonOutput -join "`n") | ConvertFrom-Json
if ([string]::IsNullOrWhiteSpace([string]$parsed.Root)) {
	throw "doctor JSON output did not include Root."
}
if (!$parsed.Checks -or $parsed.Checks.Count -eq 0) {
	throw "doctor JSON output did not include checks."
}
