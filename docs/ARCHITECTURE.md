# Architecture

`ofxGgmlRag` owns retrieval-specific workflow code and has no direct ggml
runtime dependency. Model-family workflow details stay in their companion
addons.

## Dependency Direction

```text
openFrameworks app
  -> ofxGgmlRag
  -> optional local model companion
```

The app-level generation callback is the handoff between retrieval and a local
model companion; RAG does not link that companion transitively.

## Owned Here

- rag-specific request/result helpers
- model-specific preprocessing and postprocessing
- focused root-level examples
- local media/model workflow documentation

## Not Owned Here

- ggml runtime setup and backend selection
- generic tensor, graph, model metadata, and result types
- unrelated companion workflows

See `docs/RAG_WORKFLOWS.md` before expanding this lane. It defines the planning
handoff, generated-index boundaries, citation expectations, and validation
ladder for ingestion, chunking, retrieval, memory, and agent/tool handoff work.
