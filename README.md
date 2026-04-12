# libPEANUTS 🥜

**Surgical AI prompt engineering library with context injection + structured output validation.**

```
Persona     → Who you are (Architect, Doctor, Rabbi)
Environment → What you know (POSIX, codebase tree, docs)  
Analysis    → How you think (pre-digested CoT injection)
Negotiation → What user wants (exact intent)
Updates     → Last AI response (ground truth)
Templated   → Expected output format
```
***Safety callback** surgically validates output (JSON schema, tool calls).*

## NO HALLUCINATIONS ALLOWED

**Beats RAG:** Structural context (trees, metrics) > semantic soup. Verified reasoning paths.

```
❌ Typical: "Fix my JSON parser" → 3K tokens of hallucinated context
✅ PEANUTS: Tree → "files avaiable" → "User is using cJSON needs a tweak" → focused 200 token response
```

- **Thread-safe** (`thread_local` errors)
- **Multi-API** (OpenAI, OpenRouter, custom endpoints)  
- **Zero-copy** error capture (`jsio_t` nodes)
- **State mutation** between retries (swap `safety`, `analysis`)
- **Pipe+curl** (no libcurl dep)

# Why this is better than basic RAG + CoT

Instead of just giving the AI information you are telling it what it thinks before it starts to respond which launches its trajectory. You have two Assistant messages (Analysis and Updates) and two User messages (Environment and Negotiation) allong with a System message AND a formatted response template. This gives you a cleaner version of inteaction allowing you to not only insert context but help the AI undestand what it thinks about it BEFORE it starts to infer a response.

Also, the safety function can be used to respond with your own message or trigger more interaction loops. Instead of the usual tool calling we replace things with concised microformats espcified in templates so that you can parse them and do whatever you need. This is itended too replace more precarious formmat of the past with something that is both very simple and VERY extensible. This means that you can always guarantee the thing does what its supposed to do and you don't need endless code trying  to figure out MCP providers. (although we might try to add that back later...)

The biggest usecase is in our up and coming AI CODING framework. But you should find it useful everywhere AI can be used.

# ROADMAP

- [x] Basic P.E.A.N.U.T.S. looped verification.
- [x] Basic P.E.A.N.U.T.S. chat conversation.
- [x] Custom tool calling within safety function.
- [ ] Language bindings for Rust
- [ ] Language bindings for Python
- [ ] Language bindings for Javascript
- [ ] Language bindings for PHP
- [ ] Language bindings for Lua
- [ ] Language bindings for Java
- [ ] Language bindings for Zig
- [ ] Language bindings for C#
- [ ] Advanced RAG with embedding
- [ ] Integrated memories system

# Quick Start

```c
nutmeg_t *ctx = nutmeg("gpt4-mini", "https://api.openrouter.ai/v1/chat/completions", "sk-or-v1-...");
peanuts_t nut = {
    .persona = "You are a Software Architect using action-focused logic to itentify weaknesses...",
    .environment = file_list,
    .analysis = "Okay I see the files but what are you trying to do?",
    .negotiation = "Figure out if the main path is memory safe and add TODOs if  it isn't",
    .updates = "Okay. so  I need to find the main entry point and work from there.",
    .templated = "Respond ONLY with lines like `READ|filename|hunch` or `line|filename|comment` and I will show you the files or add the comments..."
    .safety = ai_readwrite
};

char *res = nutjob(ctx, nut);
if (!res) {
    char *err = nutbad();  // "Invalid API key"
    fprintf(stderr, "%s\n", err); free(err);
} else {
    printf("AI: %s\n", res); free(res);
}
nutout(ctx);
```

ai_readwrite parses READ|file|hunch → shows file
line|file|comment → adds TODO comment.
Looping until safety returns or tries are exhausted.

```c
bool ai_readwrite(peanuts_t *nut, char **res) {
    char *saveptr;
    char *line = strtok_r(*res, "\n", &saveptr);
    char filename[128];
    char hunch[1024];
    int line_num;
    char comment[1024];
    
    while (line) {
        if (sscanf(line, "READ|%127[^|]|%1023[^\n]", filename, hunch) == 2) {
            // Read file, inject into updates
            FILE *f = fopen(filename, "r");
            if (f) {
                char *contents = read_file_contents(f);  // Your impl
                free((char*)nut->updates);
                nut->updates = malloc(4096);
                snprintf(nut->updates, 4096, 
                    "# %s (hunch: %s)\n```\n%s\n```\n", 
                    filename, hunch, contents);
                free(contents);
                fclose(f);
                return 0;  // Continue loop
            }
        } 
        else if (sscanf(line, "%d|%127[^|]|%1023[^\n]", &line_num, filename, comment) == 3) {
            // Add TODO comment to file
            add_todo_comment(filename, line_num, comment);  // Your impl
            return 1;  // Complete
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
    return 1;  // Default complete
}
```

Because `**res` is a pointer pointer, you can do things like rewrite the response text based 0n your own whim. This means you don't have to let the AI riff and say whatever you can give precise wording.

You can also do things like nesting the calls or rewriting and even giving  it a different safety on the next go round. The safety is your side of processing what is given by the AI.

## API Reference

```c
nutmeg_t *nutmeg(const char *model, const char *endpoint, const char *gatekey);
```
Creates configured API context. Sets surgical defaults: 300s timeout, 9K tokens, 10 retries, 2s pause, 0.3 temp.

```c
char *nutjob(nutmeg_t *ctx, peanuts_t *nut);
```
Fires `peanuts_t` at LLM via `ctx`. Retries `tries` times until `safety(nut, &res)` passes. **Returns allocated string (caller frees).** NULL on final failure.

```c
char *nutbad(void);
```
Thread-local error from last `nutjob()` failure. **Caller frees.** `jsio_t` node preserved across retries.

```c
bool nutyes(peanuts_t *nut, char **res);
```
Default safety - always accepts. Use for testing or permissive flows.

```c
nutmsg_t *nutmsg(nutmeg_t *ctx, peanuts_t *nut);
```
Allocates bare message node. **Set `->text`, `->self`, `->prev` manually.** `nutclr()` expects owned `text`.

```c
nutmsg_t *nutsay(nutmsg_t *prev, const char *say);
```
Appends user turn (`strdup(say)`), calls `nutfix()` compaction, gets AI response via `nutjob()`. Returns newest node.

```c
nutmsg_t *nutfix(nutmsg_t *chain, nutmeg_t *ctx, peanuts_t *nut);
```
Serializes chain → `chat`. If turns > `tries`, compacts via LLM → fresh chain. Frees `chat`.

```c
void nutclr(nutmsg_t *chain);
```
Frees entire chain + all owned `text` pointers. Walks `prev` to root.

```c
void nutout(nutmeg_t *ctx);
```
Frees `model`, `endpoint`, `gatekey`, `ctx`.

## Conversation Flow

```
nutmsg(ctx, nut)  → genesis
    ↓ nutsay("ask")
chain: genesis ← tlk(say) ← res(nutjob)
    ↓ nutsay("next") 
chain grows → nutfix() compacts → nutsay continues
nutclr(chain)     → frees everything
```

**Ownership:** `strdup(say)` + `nutjob()` → `nutclr()` perfect. No leaks.

**peanuts_t.safety** → `bool safety(peanuts_t *nut, char **res)`

