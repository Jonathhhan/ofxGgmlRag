# ofxGgmlRag

`ofxGgmlRag` is the companion addon for document ingestion, web crawl, chunking, embeddings, vector search, citations, and project memory workflows on top of `ofxGgmlCore`.

`ofxGgmlCore` stays the dependency. This addon owns rag-specific workflow code so core can stay small and boring.

Family map: https://jonathhhan.github.io/ofxGgmlCore/

Current addon API version: `1.0.1`.

## Features

- stateful `ofxGgmlRag` addon facade for apps and examples
- citation-grounded LLM prompt handoff builder
- pluggable model-backed answer generation callback
- one-call retrieval and model-backed answer helpers
- deterministic extractive answer draft builder
- document ingestion workflow boundary
- deterministic local text-corpus loading bridge
- deterministic offline HTML-to-document ingestion bridge
- deterministic offline HTML link-frontier extraction
- scoped offline HTML link-frontier filtering
- robots-meta-aware offline HTML ingestion policy
- deterministic offline robots.txt policy checks
- deterministic offline HTML page-batch ingestion snapshots
- direct source-root text-corpus retrieval helper
- local search and retrieval
- deterministic request validation
- deterministic text chunking with source offsets
- deterministic in-memory chunk search
- stopword-aware query refinement and query-variant retrieval
- source quality hints for retrieval ranking
- cosine similarity and embedded chunk vector search helpers
- local citation intent detection and exact-quote citation search
- citation confidence, source credibility, and source diversity metrics
- facade-level retrieval cache with cache-hit diagnostics
- facade-level retrieval cache introspection
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

`ofxGgmlRagUtils::htmlToText(...)` and `documentFromHtml(...)` provide the
offline web-ingestion boundary: pass already-fetched HTML plus its source URL to
produce a citation-ready `ofxGgmlRagDocument`. Live crawling/fetching remains an
opt-in app or future scraper layer.
Use `extractHtmlLinks(...)` on the same already-fetched HTML to build a
deterministic same-origin URL frontier for that future scraper layer.
Use `planHtmlLinkFrontier(...)` when UI or logs need per-link accept/skip
diagnostics for that same frontier.
Configure `allowedUrlPrefixes` and `excludedUrlPrefixes` to keep that frontier
inside known-safe paths such as docs pages while skipping private/generated
areas.
By default the HTML helpers honor robots meta directives: `noindex` pages are
not converted into documents, and `nofollow` pages do not contribute links.
Use `parseRobotsTxt(...)` and `robotsTxtAllows(...)` with already-fetched
robots.txt text to apply longest-prefix `Allow`/`Disallow` policy before adding
URLs to a future crawl queue.
Set `ofxGgmlRagHtmlLinkOptions::robotsTxt` and `robotsTxtUserAgent` to apply
that same already-fetched robots.txt policy directly during link-frontier
extraction and page-batch snapshots.
Use `documentsFromHtmlPages(...)` when an app already has multiple fetched pages
and wants RAG documents plus a deduped aggregate link frontier in one offline
snapshot.

Use the `ofxGgmlRag` class when building an app: set a query/source root or
in-memory documents, tune `getRetrievalOptions()`, call `retrieve()` or
`search(...)`, then read `getLastRetrieval()`, `summarize()`, `format(...)`, or
`formatJson(...)`. Call `buildPrompt()` to get a citation-grounded prompt that
can be handed to a future local LLM backend. Call `draftAnswer()` when you want
an explicit extractive, citation-backed answer draft from the current retrieval
without claiming model generation.

To connect a ggml/GGUF text-generation backend, configure
`setPromptGenerator(...)` with a callback that sends `prompt.prompt` to the
model and returns generated text, then call `generateAnswer()`. The addon keeps
the retrieval/citation boundary here and leaves concrete model loading to the
app or a model-family companion addon.

Use `searchAndGenerateAnswer(...)` or `loadAndGenerateAnswer(...)` when an app
wants retrieval and the configured model callback in one call.

Call `findCitations()` to extract exact local quote candidates from the loaded
documents, ranked by topic relevance, source credibility, confidence, and source
diversity. `findCitationsFromInput(...)` detects prompts like "find citations
about ..." or "quote evidence on ..." before running the same local citation
search.

Pass `-SourceRoot` to `scripts\doctor-rag.ps1` to report supported local text
corpus files before wiring an app or example to the bridge.

Pass `-Query` and `-SourceRoot` to `scripts\run-rag-runtime-smoke.ps1` to carry
the same corpus bridge signal through runtime smoke output and run a
deterministic text-corpus retrieval probe while keeping `ModelBacked=false` and
`IndexBacked=false`.

## Boundary

Keep rag-specific preprocessing, postprocessing, model launch, media handling, and examples here. Move code down into `ofxGgmlCore` only when it becomes a stable, domain-neutral primitive with focused tests.
