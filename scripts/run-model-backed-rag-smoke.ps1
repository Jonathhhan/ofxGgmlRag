param(
	[Parameter(Mandatory = $true)][string]$SourceRoot,
	[string]$Query = "What is the observatory launch code?",
	[string]$ExpectedAnswer = "ORBIT-42",
	[string]$ExpectedSource = "launch.md",
	[string]$BaseUrl = $(if ($env:OFXGGML_TEXT_SERVER_URL) { $env:OFXGGML_TEXT_SERVER_URL.TrimEnd("/") + "/v1" } else { "http://127.0.0.1:8080/v1" }),
	[string]$Model = $(if ($env:OFXGGML_TEXT_MODEL_ALIAS) { $env:OFXGGML_TEXT_MODEL_ALIAS } else { "local-model" }),
	[string]$BuildDir = "",
	[switch]$Json
)

$ErrorActionPreference = "Stop"
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Split-Path -Parent $scriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
	$BuildDir = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlRag-model-backed-smoke"
}

& (Join-Path $scriptRoot "test-addon.ps1") -Configuration Release -BuildDir $BuildDir | Out-Null
if ($LASTEXITCODE -ne 0) {
	throw "RAG test build failed with exit code $LASTEXITCODE"
}

$testExecutable = Join-Path $BuildDir "ofxGgmlRag_tests.exe"
if (!(Test-Path -LiteralPath $testExecutable -PathType Leaf)) {
	$testExecutable = Join-Path $BuildDir "ofxGgmlRag_tests"
}
$retrievalText = & $testExecutable --corpus-smoke --query $Query --source-root $SourceRoot --json
if ($LASTEXITCODE -ne 0) {
	throw "Text-corpus retrieval failed with exit code $LASTEXITCODE"
}
$retrieval = ($retrievalText -join "`n") | ConvertFrom-Json
if (!$retrieval.success -or $retrieval.stats.hits -lt 1 -or [string]::IsNullOrWhiteSpace($retrieval.context)) {
	throw "Text-corpus retrieval returned no cited context"
}

$sourceLeaf = [System.IO.Path]::GetFileName($ExpectedSource)
$body = @{
	model = $Model
	temperature = 0
	messages = @(
		@{ role = "system"; content = "Answer only from the supplied context. Return exactly two lines: ANSWER: <answer> and SOURCE: <source filename>." },
		@{ role = "user"; content = "Context:`n$($retrieval.context)`nQuestion: $Query" }
	)
} | ConvertTo-Json -Depth 6
$response = Invoke-RestMethod -Uri ($BaseUrl.TrimEnd("/") + "/chat/completions") -Method Post -ContentType "application/json" -Body $body -TimeoutSec 120
$answer = [string]$response.choices[0].message.content
$answerVerified = $answer -match [regex]::Escape($ExpectedAnswer)
$sourceVerified = [string]$retrieval.hits[0].source -match [regex]::Escape($sourceLeaf)
if (!$answerVerified -or !$sourceVerified) {
	throw "Grounded proof did not contain the expected model answer and retrieved source. Received answer: $answer; retrieved source: $($retrieval.hits[0].source)"
}

$summary = [ordered]@{
	Name = "ofxGgmlRag model-backed corpus smoke"
	Passed = $true
	ModelBacked = $true
	InferenceChecked = $true
	Model = $Model
	Query = $Query
	Answer = $answer.Trim()
	ExpectedAnswer = $ExpectedAnswer
	AnswerVerified = $answerVerified
	ExpectedSource = $sourceLeaf
	SourceVerified = $sourceVerified
	Documents = [int]$retrieval.stats.documents
	Hits = [int]$retrieval.stats.hits
	Citations = [int]$retrieval.stats.citations
	RetrievedSource = [string]$retrieval.hits[0].source
}
if ($Json) {
	$summary | ConvertTo-Json -Depth 5
} else {
	$summary.GetEnumerator() | ForEach-Object { Write-Host ("{0}: {1}" -f $_.Key, $_.Value) }
}
