# libPEANUTS 🥜

**Surgical AI prompt engineering library with context injection + structured output validation.**

```
Persona     → Who you are (Architect, Doctor, Rabbi)
Evidence    → What you know (POSIX, codebase tree, docs)
Analysis    → How you think (pre-digested CoT injection)
Nudging     → What user wants (exact intent)
Updates     → Last AI response (ground truth)
Turnout     → Expected output format
```
***Safety callback** surgically validates output (JSON schema, tool calls).*

## NO HALLUCINATIONS ALLOWED

**Beats RAG:** Structural context (trees, metrics) > semantic soup. Inject workflow verified reasoning paths.

```
❌ Typical: "Fix my JSON parser" → 3K tokens of hallucinated context
✅ PEANUTS: Tree → "files avaiable" → "I see they are using cJSON and it needs a tweak" → focused 200 token response
```

- **Thread-safe** (`thread_local` errors)
- **Multi-API** (OpenAI, OpenRouter, custom endpoints)
- **Zero-copy** error capture (`jsio_t` nodes)
- **State mutation** between retries (swap `safety`, `analysis`)
- **Pipe+curl** (no libcurl dep)

# Why this is better than basic RAG + CoT

Instead of just giving the AI information you are telling it what it thinks before it starts to respond which launches its trajectory. You have two Assistant messages (Analysis and Updates) and two User messages (Evidence and Nudging) allong with a System message AND a formatted response template. This gives you a cleaner version of inteaction allowing you to not only insert context but help the AI undestand what it thinks about it BEFORE it starts to infer a response.

Also, the safety function can be used to respond with your own message or trigger more interaction loops. Instead of the usual tool calling we replace things with concised microformats sepcified in Turnout so that you can parse them and do whatever you need. This is itended too replace more precarious formmat of the past with something that is both very simple and VERY extensible. This means that you can always guarantee the thing does what its supposed to do and you don't need endless code trying  to figure out MCP providers. (although we might try to add that back later...)

The biggest usecase is in our up and coming AI CODING framework. But you should find it useful everywhere AI can be used.

# Quick Start

Since every language is native each has their own quick start guide. Click any language to begin.

# ROADMAP

## Common Features

- [x] Basic P.E.A.N.U.T.S. looped verification.
- [x] Basic P.E.A.N.U.T.S. chat conversation.
- [x] Custom tool calling within safety function.
- [x] Cost tracking / reporting
- [ ] Advanced MCP injection

## Native Implementations

- [x] [C](./go/README.md)
- [x] [Go](./go/README.md)
- [ ] Rust
- [ ] JavaScript
- [ ] Python
- [ ] PHP
- [ ] Ruby
- [ ] Java
- [ ] C#

## Import Bindings

- [ ] Lua (use C API & LuaJIT FFI)
- [ ] Zig (use `@importC`)
- [ ] Swift
- [ ] Julia (use `ccall`)
- [ ] R (use Rcpp)

