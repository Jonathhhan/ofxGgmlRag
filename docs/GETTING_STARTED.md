# Getting Started

Use this path from a fresh openFrameworks `addons` checkout to prove the
request-boundary RAG lane and the local text-corpus bridge.

## Layout

Clone `ofxGgmlCore` beside this addon. Install `ofxImGui` beside both addons
before building the openFrameworks example.

```text
addons/
  ofxGgmlCore/
  ofxGgmlRag/
  ofxImGui/
```

## Baseline Validation

From `ofxGgmlRag`:

```powershell
scripts\validate-local.bat
```

macOS/Linux:

```sh
./scripts/validate-local.sh
```

This checks addon layout, generated-artifact hygiene, doctor output, runtime
smoke contracts, and headless helper tests.

## Text Corpus Smoke

Create or choose a local folder of user-owned `.md` and `.txt` files. Keep
corpora, indexes, generated caches, model files, and crawl output outside git.

Check corpus readiness:

```powershell
scripts\doctor-rag.ps1 -Query "citation memory" -SourceRoot C:\path\to\notes
```

Run the request-boundary runtime smoke with deterministic corpus retrieval:

```powershell
scripts\run-rag-runtime-smoke.ps1 -Query "citation memory" -SourceRoot C:\path\to\notes -Json -SummaryOnly
```

Expected smoke scope:

- `Backend` remains `request-boundary`
- `TextCorpusBridge` is `true`
- `ModelBacked` is `false`
- `IndexBacked` is `false`

## Search Example

Set environment variables before launching `ofxGgmlRagSearchExample`:

```powershell
$env:OFXGGML_RAG_QUERY = "citation memory"
$env:OFXGGML_RAG_SOURCE_ROOT = "C:\path\to\notes"
```

The example uses `ofxGgmlRagUtils::retrieveTextCorpus(...)` to load supported
local text files and run deterministic retrieval. It does not download models,
run embeddings, create vector indexes, or write generated artifacts.
