# ofxGgmlRagSearchExample

Root-level smoke example for `ofxGgmlRag`. It keeps the first workflow
intentionally small while model-backed and index-backed integrations are
planned.

By default, the ImGui panel runs a deterministic in-memory retrieval request.
Use the query, source-root, top-k, context, and run controls to inspect a local
text corpus. To prefill user-provided local text files, set:

```powershell
$env:OFXGGML_RAG_QUERY = "citation memory"
$env:OFXGGML_RAG_SOURCE_ROOT = "C:\path\to\notes"
```

The source-root bridge uses the stateful `ofxGgmlRag` addon facade to read
supported `.md` and `.txt` files and run deterministic retrieval. It does not
create indexes, download models, run embeddings, or write generated artifacts.
The `LLM Prompt` tab shows the citation-grounded prompt that a future local
model backend should answer from.

`config.make` keeps ofxImGui on openFrameworks event-listener mode for this
example (`OFXIMGUI_GLFW_EVENTS_REPLACE_OF_CALLBACKS=0`).
