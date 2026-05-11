# Contributing

`ofxGgmlRag` is a companion addon. Keep rag-specific workflow code here and keep generic ggml/runtime primitives in `ofxGgmlCore`.

Before changing public API or scripts:

- keep `ofxGgmlRag` depending on `ofxGgmlCore`, never the reverse
- keep examples focused and copyable
- keep generated models, media, builds, and IDE projects out of git
- update docs when command behavior changes

Run local validation before pushing:

```powershell
scripts\validate-local.bat
```