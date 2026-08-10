# ofxGgmlRagSearchExample

The single focused root-level example for local and web-backed `ofxGgmlRag` retrieval.

By default, the ImGui panel runs a deterministic in-memory retrieval request.
Use the query, variants, source-root, top-k, context, quality-rank, and run
controls to inspect a local text corpus. To prefill user-provided local text
files, set:

```powershell
$env:OFXGGML_RAG_QUERY = "citation memory"
$env:OFXGGML_RAG_SOURCE_ROOT = "C:\path\to\notes"
```

The local-corpus mode uses the stateful `ofxGgmlRag` addon facade to read
supported `.md` and `.txt` files and run deterministic retrieval. It does not
create indexes, download models, run embeddings, or write generated artifacts.
The `LLM Prompt` tab shows the citation-grounded prompt. The `Answer Draft` tab shows the current
extractive citation-backed answer preview without claiming model generation.
The `Citations` tab shows exact local quote candidates with confidence and
source diversity metrics.

Live-web mode uses a configurable HTML search URL template, explicit User-Agent, timeouts, result/page/depth/byte limits, robots.txt and robots-meta checks, the existing HTML conversion utilities, deterministic retrieval, and exact URL citations. Search failures remain visible and never fall back to fixtures. Fetched content remains in memory and is never persisted by default.

Run the complete web path without a window:

```powershell
bin\ofxGgmlRagSearchExample.exe --run-once --query "What is openFrameworks used for?"
```

Optional generation remains a generic OpenAI-compatible request. The canonical tested backend is CUDA-enabled `llama-server` from `ofxGgmlLlama` on port 8080:

```powershell
bin\ofxGgmlRagSearchExample.exe --run-once --query "What is openFrameworks used for?" --model local-model
```

Use `--model-endpoint` for another compatible endpoint. This addon contains no provider-specific server lifecycle, credentials, or configuration path. Provider availability and HTML markup can change; replace `--search-url-template` with another endpoint containing `{query}` when appropriate.

`config.make` keeps ofxImGui on openFrameworks event-listener mode for this
example (`OFXIMGUI_GLFW_EVENTS_REPLACE_OF_CALLBACKS=0`).
