# RAG Workflow Boundaries

`ofxGgmlRag` owns retrieval-augmented generation support workflows for the
ofxGgml ecosystem: document ingestion, chunking, indexing, retrieval,
citations, and project memory handoff. This document is for Codex, GitHub
Copilot, Hermes Agent, and human contributors planning RAG-lane work before
changing runtime behavior.

This guide follows the split rule from the legacy/reference `ofxGgml` docs:
domain workflows, generated indexes, network retrieval, project memory, and
heavy optional dependencies belong in companion addons. Shared code should move
down only when it is stable, domain-neutral, dependency-light, and covered by
focused tests.

## Owned workflow surface

This addon may define:

- document ingestion and normalization plans
- text chunking, source spans, metadata, and citation request/result shapes
- local index and vector-store handoff contracts
- retrieval and reranking workflow documentation
- project-memory boundaries and persistence rules
- focused citation-search examples
- handoff notes for `ofxGgmlLlama` generation and `ofxGgmlAgents` tool use

## Not owned here

Keep these responsibilities out of `ofxGgmlRag`:

- ggml setup, backend selection, and runtime discovery owned by `ofxGgmlCore`
- concrete llama.cpp text generation owned by `ofxGgmlLlama`
- generic agent planning loops owned by `ofxGgmlAgents`
- web UI, browser automation, or product-level research app workflows
- committed document corpora, generated indexes, embeddings caches, crawl
  output, model files, or generated openFrameworks project files
- reusable GitHub Actions policy owned by `ofxGgmlWorkflows`

## Planning handoff

Before changing RAG behavior, write down:

```text
Workflow:
Input corpus:
Chunking strategy:
Embedding or retrieval backend:
Generated local artifacts:
Citation output:
Out of scope:
Validation:
```

Runtime changes should name whether the path changes ingestion, chunking,
embedding, index persistence, retrieval, reranking, citation formatting, or
agent/tool handoff.

## Validation ladder

Use the smallest command that proves the changed layer:

| Change type | Suggested validation |
| --- | --- |
| Docs or planning only | `scripts\validate-local.bat` |
| Local setup diagnosis | `scripts\doctor-rag.bat` |
| Request/result/helper changes | `scripts\test-addon.bat` |
| Ecosystem runtime smoke evidence | `scripts\run-rag-runtime-smoke.bat -Json -SummaryOnly` |
| Example layout changes | `scripts\validate-local.bat` |

`scripts\run-rag-runtime-smoke.*` is intentionally request-boundary-only until
this addon owns a real local ingestion, embedding, or retrieval backend. It
compiles and runs the deterministic helper tests, checks doctor readiness, and
emits JSON for Core planning without downloading models, crawling data,
creating vector indexes, or committing generated memory artifacts.

## Safe first tasks

Good early RAG-lane tasks are:

- documenting corpus/index artifact rules
- defining citation and source-span contracts
- clarifying which generation steps belong in `ofxGgmlLlama`
- describing how RAG tools can be exposed to `ofxGgmlAgents`
- adding deterministic tests around chunking and citation formatting

Avoid broadening runtime behavior until corpus inputs, generated artifacts,
retrieval backend expectations, citation outputs, and validation commands are
explicit.

## First bridge handoff

Workflow:
Load a user-provided local text corpus into `ofxGgmlRagDocument` values, then
feed the existing deterministic `retrieve(...)` helper. This bridge is a
practical input path, not an embedding, vector-store, crawler, or generation
backend.

Input corpus:
The first bridge accepts files under a caller-provided source root. Supported
extensions are plain text formats such as `.md` and `.txt`. File traversal must
be deterministic, source-root scoped, and bounded by an explicit max file size.
Binary-looking files, unsupported extensions, empty files, and unreadable files
are skipped with stats or warnings instead of becoming committed artifacts.

Chunking strategy:
Use `ofxGgmlRagChunkOptions` and the existing chunking helper. Do not add a
new chunk format until a backend requires one and tests cover the contract.

Embedding or retrieval backend:
The first bridge remains `request-boundary` and `IndexBacked=false`. It should
not download models, run embeddings, persist vectors, or require network
access. A future embedding adapter must name its model path, index path,
cleanup rules, and validation command before source changes.

Generated local artifacts:
No corpus, index, embedding cache, crawl output, model file, openFrameworks
build output, or sample media dump should be committed. Generated runtime data
belongs under user-selected paths outside git-tracked addon content, or under
ignored example build/cache folders when an example owns cleanup.

Citation output:
Loaded documents should preserve stable source paths and byte offsets through
chunking. Citation labels should continue to come from `citationFromChunk(...)`
unless a later adapter defines a tested source-label mapping.

Out of scope:
Web crawling, persistent indexes, vector search, reranking models, llama.cpp
generation, generic agent loops, Core reverse dependencies, and reusable CI
policy.

Validation:
Use `scripts\validate-local.ps1` for the full local handoff. Focused helper
changes should also pass `scripts\test-addon.bat`. Runtime-smoke output should
continue to report the bridge truthfully until a real model or index backend
exists.
