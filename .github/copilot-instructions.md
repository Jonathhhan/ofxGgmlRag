# GitHub Copilot Repository Instructions

ofxGgmlRag is part of the ofxGgml openFrameworks addon ecosystem.

- Scope: document ingestion, retrieval, citations, search, and project memory boundaries
- Keep changes inside this addon's lane unless a task explicitly asks for a cross-addon update.
- For ecosystem planning tasks, prefer instruction, documentation, workflow, and validation changes before addon source changes.
- Use ofxGgmlCore for shared runtime primitives and keep companion workflows out of Core.
- Avoid committing generated outputs, local models, build directories, IDE metadata, downloaded runtimes, caches, or media dumps.
- Add or update headless tests for public helper behavior.
- Validation before handoff: scripts\validate-local.ps1.
- Keep explanations concise and include the files and checks that matter.
