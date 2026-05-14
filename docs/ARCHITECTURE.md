# Architecture

`ofxGgmlRag` owns rag-specific workflow code. It should use `ofxGgmlCore` for stable runtime primitives and keep model-family workflow details out of core.

## Dependency Direction

```text
openFrameworks app
  -> ofxGgmlRag
      -> ofxGgmlCore
```

No dependency should point from `ofxGgmlCore` back to `ofxGgmlRag`.

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
