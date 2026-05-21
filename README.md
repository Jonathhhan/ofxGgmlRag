# ofxGgmlRag

`ofxGgmlRag` is the companion addon for document ingestion, web crawl, chunking, embeddings, vector search, citations, and project memory workflows on top of `ofxGgmlCore`.

`ofxGgmlCore` stays the dependency. This addon owns rag-specific workflow code so core can stay small and boring.

Family map: https://jonathhhan.github.io/ofxGgmlCore/

Current addon API version: `1.0.1`.

## Features

- document ingestion workflow boundary
- local search and retrieval
- citation-aware result planning
- project memory boundaries
- runtime smoke validation entrypoint

## First Milestone

- define small request/result types
- keep one root-level smoke example
- keep generated models, media, builds, and IDE files out of git
- validate the addon with local headless tests

## Example

`ofxGgmlRagSearchExample` is a root-level citation search request smoke test. Generate it with the openFrameworks projectGenerator using addons `ofxGgmlRag`, `ofxGgmlCore`, and `ofxImGui`.

For RAG-lane planning, citation boundaries, and generated index rules, see
[docs/RAG_WORKFLOWS.md](docs/RAG_WORKFLOWS.md).

## Dependencies

- openFrameworks
- `ofxGgmlCore`
- `ofxImGui` for examples

## Validate

```powershell
scripts\doctor-rag.bat
scripts\run-rag-runtime-smoke.bat -Json -SummaryOnly
scripts\validate-local.bat
```

On macOS/Linux:

```sh
./scripts/doctor-rag.sh
./scripts/run-rag-runtime-smoke.sh -Json -SummaryOnly
./scripts/validate-local.sh
```

`scripts\run-rag-runtime-smoke.*` is the lane-owned runtime-smoke entrypoint
for ecosystem planning and CI rollouts. It currently proves the deterministic
RAG request/helper boundary and doctor readiness without claiming model-backed
embedding generation, crawler/index persistence, vector search, citation
extraction, or project-memory runtime support. Add backend checks here only
after corpus inputs, generated index locations, embedding model paths, citation
outputs, and cleanup rules are explicit.

## Boundary

Keep rag-specific preprocessing, postprocessing, model launch, media handling, and examples here. Move code down into `ofxGgmlCore` only when it becomes a stable, domain-neutral primitive with focused tests.
