# Roadmap

## Current Milestone

- Seed the companion addon skeleton.
- Keep `ofxGgmlRagSearchExample` as the first root-level smoke example.
- Keep `ofxGgmlCore` as the only required library dependency; examples may depend on `ofxImGui`.
- Add local validation and headless tests.
- Add independent addon version metadata and release-candidate docs.
- Add deterministic request validation, tag normalization, source-root scoped retrieval, excluded source-root filtering, text chunking, in-memory search, minimum-score filtering, minimum matched-term filtering, excluded-tag filtering, phrase-aware scoring, search-hit excerpts, retrieval context assembly, in-memory retrieval pipeline, retrieval diagnostics, retrieval summary/report/JSON formatting, structured hit metadata and citation JSON output, pretty JSON reports, reference-aware retrieval reports, citation-aware result assembly, citation/reference formatting, and result summaries.

## Next Milestones

- Connect the first real local backend or bridge adapter.
- Add one useful openFrameworks example that runs with user-provided assets.
- Add focused tests around backend handoff behavior.
- Document the `clone -> setup -> run` path from a new user's point of view.
