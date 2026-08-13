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
Corpus loading, chunking, retrieval, prompt construction, and citation scoring
run on a dedicated `ofThread` worker; the UI applies completed result snapshots
in `update()` and remains responsive for large source folders.
The `LLM Prompt` tab shows the citation-grounded prompt. The `Answer Draft` tab shows the current
extractive citation-backed answer preview without claiming model generation.
The `Citations` tab shows exact local quote candidates with confidence and
source diversity metrics.

Live-web mode uses a configurable HTML search URL template, explicit User-Agent, timeouts, result/page/depth/byte limits, robots.txt and robots-meta checks, the existing HTML conversion utilities, deterministic retrieval, and exact URL citations. Search failures remain visible and never fall back to fixtures. Fetched content remains in memory and is never persisted by default.

Enable `Person / quotes` and enter a name to search specifically for quotation sources. Results under `VERBATIM SOURCE EXCERPTS` are copied exactly from the fetched page and always include their URL; the label explicitly warns that attribution still requires reviewing that source. Optional model output appears separately as `MODEL SUMMARY — NOT A QUOTE` and must not be treated as wording by the person.

Editable path/provider/model fields support normal Ctrl+V and also include a field-local `Paste` button. Pasting replaces the full field, which is intentional for paths, aliases, and URLs. The live-web panel also has a `Browse...` button for a local `.gguf` text model. `Start selected model locally` launches the existing sibling `ofxGgmlLlama` `llama-server` on configurable port 8092 by default, derives its `local/<filename>` alias, waits for readiness asynchronously, and then enables grounded answer generation. It does not stop or replace other local model servers; choose another port in the UI if the selected port is occupied.

Search ranking combines lexical coverage, exact-phrase boost, bounded query refinement, and search-result quality. Web results are reduced to source-diverse cited hits, and quote mode builds the model context from the verified structured quote excerpts instead of unrelated surrounding page text.

Generated answers are bounded to 256 tokens by default. Page requests, the complete fetch phase, and model generation have separate time budgets so several slow sites cannot consume the model's response window. Adjust these controls in the UI or pass `--page-timeout`, `--total-fetch-timeout`, `--model-timeout`, and `--model-max-tokens` to the headless runner.

Both local retrieval and the complete web pipeline run asynchronously in the GUI. The window remains responsive while corpus loading, search, scraping, retrieval, and optional local generation are in progress; the Run control exposes the active operation until the result is ready.

Run the complete web path without a window:

```powershell
bin\ofxGgmlRagSearchExample.exe --run-once --query "What is openFrameworks used for?"
```

For a headless quotation-source search:

```powershell
bin\ofxGgmlRagSearchExample.exe --run-once --person "Alan Kay" --max-pages 5
```

Optional generation remains a generic OpenAI-compatible request. The canonical tested backend is CUDA-enabled `llama-server` from `ofxGgmlLlama` on port 8080:

```powershell
bin\ofxGgmlRagSearchExample.exe --run-once --query "What is openFrameworks used for?" --model local-model
```

Use `--model-endpoint` for another compatible endpoint. This addon contains no provider-specific server lifecycle, credentials, or configuration path. Provider availability and HTML markup can change; replace `--search-url-template` with another endpoint containing `{query}` when appropriate.

`config.make` keeps ofxImGui on openFrameworks event-listener mode for this
example (`OFXIMGUI_GLFW_EVENTS_REPLACE_OF_CALLBACKS=0`).
