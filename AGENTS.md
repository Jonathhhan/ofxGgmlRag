# AGENTS.md

This repository is `ofxGgmlRag`, the retrieval and local-search companion addon for the ofxGgml family.

Codex should treat `ofxGgmlCore` as the backend-neutral foundation. This repo owns retrieval workflows, citations, web crawl integration, local search, indexing flows, and retrieval-specific examples.

## Addon contract

Do:

- keep retrieval/search-specific workflows in this addon
- depend on shared primitives from `ofxGgmlCore`
- preserve openFrameworks addon layout and `addon_config.mk`
- keep examples projectGenerator-friendly
- document indexing/runtime requirements clearly

Do not:

- move backend-neutral Core primitives into this repo
- commit indexes, downloaded corpora, binaries, or caches
- hardcode local absolute paths

## Codex workflow

1. Inspect existing files first.
2. Keep changes small and focused.
3. Preserve addon boundaries.
4. Update docs/examples/scripts with code changes.
5. Summarize validation honestly.
