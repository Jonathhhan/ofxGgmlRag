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
| Example layout changes | `scripts\validate-local.bat` |

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
