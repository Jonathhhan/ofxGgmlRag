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

$corpusRoot = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlRag-doctor-corpus"
if (Test-Path -LiteralPath $corpusRoot) {
	Remove-Item -LiteralPath $corpusRoot -Recurse -Force
}
New-Item -ItemType Directory -Path (Join-Path $corpusRoot "nested") | Out-Null
Set-Content -LiteralPath (Join-Path $corpusRoot "a.md") -Value "citation memory" -Encoding UTF8
Set-Content -LiteralPath (Join-Path $corpusRoot "nested\b.txt") -Value "nested citation" -Encoding UTF8
Set-Content -LiteralPath (Join-Path $corpusRoot "skip.json") -Value "{""text"":""citation""}" -Encoding UTF8

$corpusOutput = & $doctorScript -SourceRoot $corpusRoot *>&1 | ForEach-Object { $_.ToString() }
if (!$?) {
	throw "doctor-rag.ps1 -SourceRoot failed."
}
$corpusText = $corpusOutput -join "`n"
foreach ($expected in @(
	"RAG source root",
	"RAG text corpus",
	"2 supported text file(s), 1 skipped"
)) {
	if ($corpusText -notmatch [regex]::Escape($expected)) {
		throw "doctor corpus output did not contain expected text: $expected`n$corpusText"
	}
}

$corpusJsonOutput = & $doctorScript -SourceRoot $corpusRoot -Json *>&1 | ForEach-Object { $_.ToString() }
if (!$?) {
	throw "doctor-rag.ps1 -SourceRoot -Json failed."
}
$corpusParsed = ($corpusJsonOutput -join "`n") | ConvertFrom-Json
$corpusCheck = @($corpusParsed.Checks | Where-Object { $_.Name -eq "RAG text corpus" })[0]
if ($null -eq $corpusCheck -or $corpusCheck.State -ne "OK" -or $corpusCheck.Detail -notmatch "2 supported text file") {
	throw "doctor JSON output did not include supported text corpus readiness."
}

Remove-Item -LiteralPath $corpusRoot -Recurse -Force
