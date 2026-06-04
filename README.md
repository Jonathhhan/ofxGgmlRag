# ofxGgmlRag

`ofxGgmlRag` is the companion addon for document ingestion, web crawl, chunking, embeddings, vector search, citations, and project memory workflows on top of `ofxGgmlCore`.

`ofxGgmlCore` stays the dependency. This addon owns rag-specific workflow code so core can stay small and boring.

Family map: https://jonathhhan.github.io/ofxGgmlCore/

Current addon API version: `1.0.1`.

## Features

- document ingestion workflow boundary
- deterministic local text-corpus loading bridge
- direct source-root text-corpus retrieval helper
- local search and retrieval
- deterministic request validation
- deterministic text chunking with source offsets
- deterministic in-memory chunk search
- minimum-score retrieval filtering
- minimum matched-term retrieval filtering
- excluded-tag retrieval filtering
- phrase-aware retrieval scoring
- deterministic search-hit excerpts
- citation-aware search result assembly
- deterministic retrieval context assembly
- deterministic in-memory retrieval pipeline
- source-root scoped retrieval boundaries
- pre-chunk excluded source-root retrieval filtering
- retrieval diagnostics and counters
- deterministic retrieval summary formatting
- deterministic retrieval report formatting
- deterministic JSON retrieval reports
- structured hit metadata in JSON reports
- structured citation JSON output
- pretty JSON retrieval reports
- reference-aware retrieval reports
- deterministic reference bibliography formatting
- citation and source-span helpers
- citation-aware result planning
- project memory boundaries
- runtime smoke validation entrypoint

## First Milestone

- define small request/result types
- define validation, chunking, search, context, citation, retrieval, and result assembly helper contracts
- keep one root-level smoke example
- keep generated models, media, builds, and IDE files out of git
- validate the addon with local headless tests

## Example

`ofxGgmlRagSearchExample` is a root-level citation search request smoke test. Generate it with the openFrameworks projectGenerator using addons `ofxGgmlRag`, `ofxGgmlCore`, and `ofxImGui`.

For RAG-lane planning, citation boundaries, and generated index rules, see
[docs/RAG_WORKFLOWS.md](docs/RAG_WORKFLOWS.md).

For the fresh clone, validation, local corpus smoke, and example launch path,
see [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md).

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

`ofxGgmlRagUtils::loadTextCorpus(...)` is the first local bridge: it reads
user-provided `.md` and `.txt` files from a source root into
`ofxGgmlRagDocument` values for the existing deterministic retrieval helper.
It does not create indexes, download models, run embeddings, or write generated
artifacts.

`ofxGgmlRagUtils::retrieveTextCorpus(...)` combines source-root loading and
deterministic retrieval for apps and examples that do not need to inspect the
intermediate document list.

Pass `-SourceRoot` to `scripts\doctor-rag.ps1` to report supported local text
corpus files before wiring an app or example to the bridge.

Pass `-Query` and `-SourceRoot` to `scripts\run-rag-runtime-smoke.ps1` to carry
the same corpus bridge signal through runtime smoke output and run a
deterministic text-corpus retrieval probe while keeping `ModelBacked=false` and
`IndexBacked=false`.

## Boundary

Keep rag-specific preprocessing, postprocessing, model launch, media handling, and examples here. Move code down into `ofxGgmlCore` only when it becomes a stable, domain-neutral primitive with focused tests.
