# Release Checklist

Use this before tagging or announcing an `ofxGgmlRag` release. The goal is to
prove the addon boundary and example layout without claiming indexing or crawler
runtime support that is not wired yet.

## Fresh Clone Layout

From the openFrameworks `addons` folder:

```powershell
git clone https://github.com/Jonathhhan/ofxGgmlCore.git
git clone https://github.com/Jonathhhan/ofxGgmlRag.git
cd ofxGgmlRag
```

Expected layout:

```text
addons/
  ofxGgmlCore/
  ofxGgmlRag/
  ofxImGui/
```

## Local Validation

Run:

```powershell
scripts\validate-local.bat
```

macOS/Linux:

```sh
./scripts/validate-local.sh
```

For a pre-tag release candidate gate:

```powershell
scripts\release-candidate.bat
```

macOS/Linux:

```sh
./scripts/release-candidate.sh
```

## Example Scope

`ofxGgmlRagSearchExample` is intentionally narrow in this release:

- root-level openFrameworks example
- `ofxImGui` dependency declared in `addons.make`
- citation search request smoke surface with optional local text-corpus input
- deterministic `.md` / `.txt` bridge only; no generated indexes or model files
- clear future path for crawling, embeddings, vector search, reranking, and
  model-backed citations

This release does not promise a complete model-backed RAG runtime, crawler,
embedding backend, or vector index.

## Before Tagging

- `git status --short --ignored` shows no unexpected generated outputs
- no model files, crawled data, generated indexes, generated OF project files,
  or build outputs are staged
- `CHANGELOG.md` has an entry for the release
- `docs/releases/vX.Y.Z.md` matches the release scope
- release notes distinguish request/helper skeleton work from future runtime
  adapters
