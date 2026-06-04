# Changelog

## Unreleased

- Added a deterministic local text-corpus bridge for user-provided `.md` and
  `.txt` files.
- Added corpus loading options, stats, warnings, and result types.
- Added `ofxGgmlRagUtils::loadTextCorpus(...)` for feeding local files into the
  existing request-boundary retrieval helper.
- Added `ofxGgmlRagUtils::retrieveTextCorpus(...)` for one-call source-root
  retrieval.
- Added the stateful `ofxGgmlRag` facade for apps/examples to load documents,
  run retrieval, and format the latest result without manually stitching helper
  calls together.
- Added `ofxGgmlRagPrompt` and prompt-building helpers for citation-grounded
  model handoff.
- Added `ofxGgmlRagAnswer` and deterministic extractive answer drafts for
  citation-backed UI previews.
- Ported dependency-light RAG pipeline behavior from the legacy addon shape:
  stopword-aware query refinement, query variants, source quality hints, and
  cosine/vector search helpers.
- Added local citation search parity helpers: input intent detection, markdown
  cleanup, exact quote extraction, confidence scoring, source credibility, and
  source diversity metrics.
- Extended headless tests and runtime smoke coverage with a corpus retrieval
  probe.
- Updated `doctor-rag.ps1`, `run-rag-runtime-smoke.ps1`, and the ImGui-backed
  `ofxGgmlRagSearchExample` to expose local source-root readiness.
- Added getting-started docs for fresh clone validation, corpus smoke, and
  example launch.
- Kept embedding generation, vector indexes, web crawling, model downloads, and
  model-backed generation out of scope.

## 1.0.1 - 2026-05-12

- Added independent RAG addon version metadata.
- Exposed version metadata through the public umbrella header.
- Documented the release checklist, release policy, and `v1.0.1` scope.
- Kept document ingestion, web crawling, chunking, vector search, citations, and
  project memory runtime adapters as explicit future work.

## 1.0.0

- Started `ofxGgmlRag` as the companion addon for document ingestion, web crawl,
  chunking, embeddings, vector search, citations, and project memory workflows
  on top of `ofxGgmlCore`.
- Added the initial request/result helpers, root-level search example skeleton,
  local validation, and headless tests.
