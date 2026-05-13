# ofxGgmlRag

`ofxGgmlRag` is the companion addon for document ingestion, web crawl, chunking, embeddings, vector search, citations, and project memory workflows on top of `ofxGgmlCore`.

`ofxGgmlCore` stays the dependency. This addon owns rag-specific workflow code so core can stay small and boring.

Family map: https://jonathhhan.github.io/ofxGgmlCore/

Current addon API version: `1.0.1`.

## First Milestone

- define small request/result types
- keep one root-level smoke example
- keep generated models, media, builds, and IDE files out of git
- validate the addon with local headless tests

## Example

`ofxGgmlRagSearchExample` is a root-level citation search request smoke test. Generate it with the openFrameworks projectGenerator using addons `ofxGgmlRag`, `ofxGgmlCore`, and `ofxImGui`.

## Dependencies

- openFrameworks
- `ofxGgmlCore`
- `ofxImGui` for examples

## Validate

```powershell
scripts\doctor-rag.bat
scripts\validate-local.bat
```

On macOS/Linux:

```sh
./scripts/doctor-rag.sh
./scripts/validate-local.sh
```

## Boundary

Keep rag-specific preprocessing, postprocessing, model launch, media handling, and examples here. Move code down into `ofxGgmlCore` only when it becomes a stable, domain-neutral primitive with focused tests.
