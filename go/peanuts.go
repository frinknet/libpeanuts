// (c) 2026 FRINKnet & Friends – MIT licence

// Package peanuts implements surgical AI prompt engineering with context
// injection and structured output validation.
//
// Persona  → Who you are (Architect, Doctor, Rabbi)
// Evidence → What you know (POSIX, codebase tree, docs)
// Analysis → How you think (pre-digested CoT injection)
// Nudging  → What user wants (exact intent)
// Updates  → Last AI response (ground truth)
// Turnout  → Expected output format
//
// Safety callback surgically validates output (JSON schema, tool calls).
package libPEANUTS

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"sync"
	"time"
)

// Peanuts holds a one-shot request to the AI.
type Peanuts struct {
	Persona   string // what role the AI plays
	Evidence  string // what the AI should know
	Analysis  string // what the AI should think
	Nudging   string // what the USER asks for
	Updates   string // what the AI responded
	Turnout   string // how the AI should respond

	// Safety is called after each AI response. Return true to accept the
	// response, or false to retry (up to ctx.Tries times). If safety is nil
	// the response is always accepted.
	Safety func(nut *Peanuts, res *string) bool

	Data interface{} // what you want to remember between loops
}

// Nutuse tracks token usage and costs.
type Nutuse struct {
	InTokens  int64   // standard prompt tokens (accumulated, auto-subtracted from cached)
	Cached    int64   // cache hit tokens (accumulated)
	OutTokens int64   // completion tokens (accumulated)
	Calls     int64   // API calls (accumulated)
	InCost    float64 // cost per 1M input tokens (set by user)
	CacheCost float64 // cost per 1M cache hit tokens (set by user, usually ~10% of InCost)
	OutCost   float64 // cost per 1M output tokens (set by user)
	CallsCost float64 // cost per API call (set by user for aggregators)
	Spend     float64 // actual cost from provider if returned (accumulated)
	InPath    string  // JSON path for total input tokens ("" = auto)
	CachePath string  // JSON path for cached tokens ("" = auto)
	OutPath   string  // JSON path for output tokens ("" = auto)
	SpendPath string  // JSON path for cost ("" = "cost")
}

// Cost returns the total estimated cost based on current accumulated usage.
func (nu Nutuse) Cost() float64 {
	return float64(nu.InTokens)/1e6*nu.InCost +
		float64(nu.Cached)/1e6*nu.CacheCost +
		float64(nu.OutTokens)/1e6*nu.OutCost +
		float64(nu.Calls)*nu.CallsCost
}

// Nutmeg holds basic settings for an AI endpoint.
type Nutmeg struct {
	Model    string   // AI model name
	Endpoint string   // AI endpoint url
	Gatekey  string   // API gatekey
	Timeout  int      // seconds
	Tokens   int      // max output tokens
	Tries    int      // max retries
	Pause    int      // ms between retries
	Temp     float64  // temperature

	Usage Nutuse // token tracking (accumulated)
}

// Nutmix is a node in a chat conversation chain.
type Nutmix struct {
	Ctx  *Nutmeg
	Nut  *Peanuts
	Self bool
	Text string
	Prev *Nutmix
}

// thread-local error from last nutjob failure
var (
	errMu  sync.Mutex
	nutErr string
)

// Nuterr returns the error from the last Nutjob failure (thread-safe).
func Nuterr() string {
	errMu.Lock()
	s := nutErr
	nutErr = ""
	errMu.Unlock()
	if s == "" {
		return "unknown error"
	}
	return s
}

// NewNutmeg creates a configured API context with surgical defaults:
// 300s timeout, 9K tokens, 10 retries, 2s pause, 0.3 temp.
func NewNutmeg(model, endpoint, gatekey string) *Nutmeg {
	return &Nutmeg{
		Model:    model,
		Endpoint: endpoint,
		Gatekey:  gatekey,
		Timeout:  300,
		Tokens:   9000,
		Tries:    10,
		Pause:    2000,
		Temp:     0.3,
	}
}

// Nutout frees (releases) resources held by Nutmeg. Currently a no-op in Go
// since there's no manual memory management, but kept for API compatibility.
func Nutout(ctx *Nutmeg) {
	// no-op in Go; kept for API parity with the C version
}

// Usage returns the accumulated token usage from the context.
func Usage(ctx *Nutmeg) Nutuse {
	if ctx == nil {
		return Nutuse{}
	}
	return ctx.Usage
}

// jsonTemplate returns the JSON body template based on the endpoint URL.
func jsonTemplate(postURL string) string {
	switch {
	case strings.Contains(postURL, "/v1/responses"):
		return `{"model":"%s","instructions":"%s","input":[{"role":"user","content":"%s"},{"role":"assistant","content":"%s"},{"role":"user","content":"%s"},{"role":"assistant","content":"%s"},{"role":"user","content":"%s"}],"temperature":%.1f,"max_output_tokens":%d}`

	case strings.Contains(postURL, "/v1/messages"):
		return `{"model":"%s","messages":[{"role":"system","content":"%s"},{"role":"user","content":"%s"},{"role":"assistant","content":"%s"},{"role":"user","content":"%s"},{"role":"assistant","content":"%s"},{"role":"user","content":"%s"}],"temperature":%.1f,"max_tokens":%d}`

	case strings.Contains(postURL, "/v1/chat/completions"):
		return `{"model":"%s","messages":[{"role":"system","content":"%s"},{"role":"user","content":"%s"},{"role":"assistant","content":"%s"},{"role":"user","content":"%s"},{"role":"assistant","content":"%s"},{"role":"user","content":"%s"}],"temperature":%.1f,"max_tokens":%d}`

	default:
		return `{"model":"%s","prompt":"# SYSTEM:\n%s\n\n# USER:\n%s\n\n# ASSISTANT:\n%s\n\n# USER:\n%s\n\n# ASSISTANT:\n%s\n\n# USER:\n%s\n\n# ASSISTANT:\n","temperature":%.1f,"max_tokens":%d}`
	}
}

// responsePath returns the JSON path for extracting the response text.
func responsePath(postURL string) string {
	switch {
	case strings.Contains(postURL, "/v1/responses"):
		return "output[0].content[0].text"
	case strings.Contains(postURL, "/v1/messages"):
		return "choices[0].message.content"
	case strings.Contains(postURL, "/v1/chat/completions"):
		return "choices[0].message.content"
	case strings.Contains(postURL, "/v1/completions"):
		return "choices[0].text"
	default:
		return "."
	}
}

// jsonEscape escapes a string for inclusion in a JSON value.
func jsonEscape(txt string) string {
	var b strings.Builder
	b.Grow(len(txt) * 2)
	for _, c := range txt {
		switch c {
		case '"':
			b.WriteString(`\"`)
		case '\\':
			b.WriteString(`\\`)
		case '\n':
			b.WriteString(`\n`)
		case '\r':
			b.WriteString(`\r`)
		case '\t':
			b.WriteString(`\t`)
		default:
			b.WriteRune(c)
		}
	}
	return b.String()
}

// buildRequest formats the JSON request body according to the endpoint type.
func buildRequest(ctx *Nutmeg, nut *Peanuts) string {
	fmtStr := jsonTemplate(ctx.Endpoint)
	return fmt.Sprintf(fmtStr,
		ctx.Model,
		jsonEscape(nut.Persona),
		jsonEscape(nut.Evidence),
		jsonEscape(nut.Analysis),
		jsonEscape(nut.Nudging),
		jsonEscape(nut.Updates),
		jsonEscape(nut.Turnout),
		ctx.Temp,
		ctx.Tokens,
	)
}

// resolveJSONPath walks a simple dot/bracket path into a json.RawMessage tree.
// Supports paths like "choices[0].message.content" and "output[0].content[0].text".
func resolveJSONPath(data json.RawMessage, path string) (json.RawMessage, error) {
	if path == "." {
		return data, nil
	}

	parts := strings.FieldsFunc(path, func(r rune) bool {
		return r == '.' || r == '[' || r == ']'
	})

	current := data
	for _, part := range parts {
		// Try array index first
		var idx int
		if _, err := fmt.Sscanf(part, "%d", &idx); err == nil {
			var arr []json.RawMessage
			if err := json.Unmarshal(current, &arr); err != nil {
				return nil, err
			}
			if idx < 0 || idx >= len(arr) {
				return nil, fmt.Errorf("index %d out of bounds", idx)
			}
			current = arr[idx]
			continue
		}

		// Object key
		var obj map[string]json.RawMessage
		if err := json.Unmarshal(current, &obj); err != nil {
			return nil, err
		}
		val, ok := obj[part]
		if !ok {
			return nil, fmt.Errorf("key %q not found", part)
		}
		current = val
	}

	return current, nil
}

// send makes the HTTP POST request to the AI endpoint and returns the
// response text and updates usage tracking.
func send(ctx *Nutmeg, nut *Peanuts) (string, error) {
	body := buildRequest(ctx, nut)

	client := &http.Client{Timeout: time.Duration(ctx.Timeout) * time.Second}
	req, err := http.NewRequest("POST", ctx.Endpoint, bytes.NewReader([]byte(body)))
	if err != nil {
		return "", err
	}

	req.Header.Set("Content-Type", "application/json")
	if ctx.Gatekey != "" {
		req.Header.Set("Authorization", "Bearer "+ctx.Gatekey)
	}

	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	if resp.StatusCode != http.StatusOK {
		// Try to extract error message from response body
		var errResp struct {
			Error struct {
				Message string `json:"message"`
			} `json:"error"`
		}
		if json.Unmarshal(raw, &errResp) == nil && errResp.Error.Message != "" {
			errMu.Lock()
			nutErr = errResp.Error.Message
			errMu.Unlock()
		}
		return "", fmt.Errorf("API returned status %d", resp.StatusCode)
	}

	var rawMsg json.RawMessage
	if err := json.Unmarshal(raw, &rawMsg); err != nil {
		return "", err
	}

	// Check for error in response body
	errRes, _ := resolveJSONPath(rawMsg, "error.message")
	if errRes != nil {
		var errStr string
		if json.Unmarshal(errRes, &errStr) == nil && errStr != "" {
			errMu.Lock()
			nutErr = errStr
			errMu.Unlock()
			return "", fmt.Errorf("API error: %s", errStr)
		}
	}

	path := responsePath(ctx.Endpoint)
	resolved, err := resolveJSONPath(rawMsg, path)
	if err != nil {
		return "", err
	}

	var content string
	if err := json.Unmarshal(resolved, &content); err != nil {
		return "", err
	}

	// Parse usage from response
	ctx.Usage.Calls++

	usageRaw, err := resolveJSONPath(rawMsg, "usage")
	if err == nil {
		inpPath := ctx.Usage.InPath
		outPath := ctx.Usage.OutPath
		cchPath := ctx.Usage.CachePath

		if inpPath == "" {
			if strings.Contains(ctx.Endpoint, "/v1/messages") || strings.Contains(ctx.Endpoint, "/v1/responses") {
				inpPath = "input_tokens"
			} else {
				inpPath = "prompt_tokens"
			}
		}
		if outPath == "" {
			if strings.Contains(ctx.Endpoint, "/v1/messages") || strings.Contains(ctx.Endpoint, "/v1/responses") {
				outPath = "output_tokens"
			} else {
				outPath = "completion_tokens"
			}
		}
		if cchPath == "" {
			if strings.Contains(ctx.Endpoint, "/v1/messages") || strings.Contains(ctx.Endpoint, "/v1/responses") {
				cchPath = "cache_read_input_tokens"
			} else {
				cchPath = "prompt_tokens_details.cached_tokens"
			}
		}

		if inp, err := resolveJSONPath(usageRaw, inpPath); err == nil {
			var v float64
			if json.Unmarshal(inp, &v) == nil {
				totalIn := int64(v)
				var cached int64
				if cch, err := resolveJSONPath(usageRaw, cchPath); err == nil {
					var cv float64
					if json.Unmarshal(cch, &cv) == nil {
						cached = int64(cv)
					}
				}
				ctx.Usage.Cached += cached
				ctx.Usage.InTokens += totalIn - cached
			}
		}

		if out, err := resolveJSONPath(usageRaw, outPath); err == nil {
			var v float64
			if json.Unmarshal(out, &v) == nil {
				ctx.Usage.OutTokens += int64(v)
			}
		}

		costPath := ctx.Usage.SpendPath
		if costPath == "" {
			costPath = "cost"
		}
		if cost, err := resolveJSONPath(usageRaw, costPath); err == nil {
			var v float64
			if json.Unmarshal(cost, &v) == nil {
				ctx.Usage.Spend += v
			}
		}
	}

	return content, nil
}

// Nutjob fires a peanuts_t at the LLM via the given context. It retries up to
// ctx.Tries times until the safety callback returns true. Returns the response
// string and any error. The caller owns the returned string.
func Nutjob(ctx *Nutmeg, nut *Peanuts) (string, error) {
	// Fill defaults for nil fields
	if nut.Persona == "" {
		nut.Persona = "You are a helpful assistant."
	}
	if nut.Evidence == "" {
		nut.Evidence = "Respond with a concise but complete answer."
	}
	if nut.Analysis == "" {
		nut.Analysis = "Got it. What do you want me to do?"
	}
	if nut.Nudging == "" {
		nut.Nudging = "Help me understand this better."
	}
	if nut.Updates == "" {
		nut.Updates = "Okay. So I need to think through this before I respond."
	}
	if nut.Turnout == "" {
		nut.Turnout = "Respond with a concise but complete answer."
	}

	for i := 0; i < ctx.Tries; i++ {
		res, err := send(ctx, nut)
		if err != nil {
			if i < ctx.Tries-1 {
				time.Sleep(time.Duration(ctx.Pause) * time.Millisecond)
				continue
			}
			return "", err
		}

		if nut.Safety == nil || nut.Safety(nut, &res) {
			return res, nil
		}

		time.Sleep(time.Duration(ctx.Pause) * time.Millisecond)
	}

	errMu.Lock()
	nutErr = "No more tries."
	errMu.Unlock()
	return "", fmt.Errorf("no more tries")
}

// Nutyes is a default safety callback that always accepts the response.
func Nutyes(nut *Peanuts, res *string) bool {
	return true
}

// NewNutmix allocates a bare message node. Set fields manually or use Nutsay.
func NewNutmix(ctx *Nutmeg, nut *Peanuts) *Nutmix {
	return &Nutmix{
		Ctx: ctx,
		Nut: nut,
	}
}

// Nutsay appends a user turn, calls Nutfix for compaction, and gets an AI
// response via Nutjob. Returns the newest node in the chain.
func Nutsay(msg **Nutmix, say string) *Nutmix {
	ctx := (*msg).Ctx
	nut := (*msg).Nut
	tlk := NewNutmix(ctx, nut)
	res := NewNutmix(ctx, nut)

	tlk.Prev = *msg
	tlk.Self = true
	tlk.Text = say
	res.Prev = Nutfix(&tlk, ctx, nut)
	res.Text, _ = Nutjob(ctx, nut)
	*msg = res

	return res
}

// Nutfix serializes the conversation chain into the nudging field.
// If the number of turns exceeds ctx.Tries, it compacts via LLM.
func Nutfix(msg **Nutmix, ctx *Nutmeg, nut *Peanuts) *Nutmix {
	var chat strings.Builder
	i := 0
	cur := NewNutmix(ctx, nut)

	cur.Self = true
	cur.Prev = *msg
	cur.Text = nut.Updates

	// Walk the chain backwards building the conversation
	for cur.Prev != nil {
		i++
		if cur.Text != "" {
			role := "USER"
			if !cur.Self {
				role = "ASSISTANT"
			}
			chat.WriteString(fmt.Sprintf("%s:\n%s\n\n", role, cur.Text))
		}
		cur = cur.Prev
	}

	conversation := chat.String()

	// If conversation is too long, compact via LLM
	if i > ctx.Tries {
		shrink := &Peanuts{
			Persona:  "You compact conversations without losing their essence.",
			Evidence: conversation,
			Analysis: "So we need to shrink this to around 1000-2000 tokens.",
			Nudging:  "Yes. But we don't want to lose the important points.",
			Updates:  "Okay so I'm free to change the turns as long as I keep it focused.",
			Turnout:  "Respond with a compact conversation keeping the general turn taking.",
		}

		newmsg := NewNutmix(ctx, nut)
		newmsg.Self = true
		newmsg.Text, _ = Nutjob(ctx, shrink)

		Nutoff(*msg)
		conversation = newmsg.Text
		*msg = newmsg
	}

	nut.Nudging = conversation
	return *msg
}

// Nutoff frees (releases) the entire conversation chain.
func Nutoff(msg *Nutmix) {
	for msg != nil {
		prev := msg.Prev
		msg.Prev = nil
		msg = prev
	}
}
