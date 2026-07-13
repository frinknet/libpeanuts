// (c) 2026 FRINKnet & Friends – MIT licence

package libpeanuts

import (
	"encoding/json"
	"strings"
	"testing"
)

func TestNewNutmegDefaults(t *testing.T) {
	ctx := NewNutmeg("gpt4", "https://api.example.com/v1/chat/completions", "sk-xxx")
	if ctx.Timeout != 300 {
		t.Errorf("expected timeout 300, got %d", ctx.Timeout)
	}
	if ctx.Tokens != 9000 {
		t.Errorf("expected tokens 9000, got %d", ctx.Tokens)
	}
	if ctx.Tries != 10 {
		t.Errorf("expected tries 10, got %d", ctx.Tries)
	}
	if ctx.Pause != 2000 {
		t.Errorf("expected pause 2000, got %d", ctx.Pause)
	}
	if ctx.Temp != 0.3 {
		t.Errorf("expected temp 0.3, got %f", ctx.Temp)
	}
}

func TestUsage(t *testing.T) {
	ctx := NewNutmeg("m", "e", "k")
	u := Usage(ctx)
	if u.Calls != 0 {
		t.Errorf("expected 0 calls, got %d", u.Calls)
	}
	// nil context
	u = Usage(nil)
	if u.Calls != 0 {
		t.Errorf("expected 0 calls from nil, got %d", u.Calls)
	}
}

func TestNuterr(t *testing.T) {
	// Should return "unknown error" when no error set
	err := Nuterr()
	if err != "unknown error" {
		t.Errorf("expected 'unknown error', got %q", err)
	}
	// Second call should also return "unknown error" since it was reset
	err = Nuterr()
	if err != "unknown error" {
		t.Errorf("expected 'unknown error' on second call, got %q", err)
	}
}

func TestJsonEscape(t *testing.T) {
	tests := []struct {
		input    string
		expected string
	}{
		{`hello`, `hello`},
		{`hello "world"`, `hello \"world\"`},
		{`back\slash`, `back\\slash`},
		{"new\nline", `new\nline`},
		{"carriage\rreturn", `carriage\rreturn`},
		{"tab\tchar", `tab\tchar`},
	}
	for _, tc := range tests {
		got := jsonEscape(tc.input)
		if got != tc.expected {
			t.Errorf("jsonEscape(%q) = %q, want %q", tc.input, got, tc.expected)
		}
	}
}

func TestJsonTemplate(t *testing.T) {
	tmpl := jsonTemplate("https://api.openai.com/v1/chat/completions")
	if !strings.Contains(tmpl, "messages") {
		t.Error("expected chat completions template")
	}
	if strings.Contains(tmpl, "max_output_tokens") {
		t.Error("chat completions should use max_tokens")
	}

	tmpl = jsonTemplate("https://api.openai.com/v1/responses")
	if !strings.Contains(tmpl, "input") {
		t.Error("expected responses template")
	}
	if !strings.Contains(tmpl, "max_output_tokens") {
		t.Error("responses should use max_output_tokens")
	}

	tmpl = jsonTemplate("https://api.anthropic.com/v1/messages")
	if !strings.Contains(tmpl, "messages") {
		t.Error("expected messages template")
	}

	tmpl = jsonTemplate("https://api.example.com/v1/completions")
	if !strings.Contains(tmpl, "# SYSTEM:") {
		t.Error("expected completions fallback template")
	}
}

func TestResponsePath(t *testing.T) {
	tests := []struct {
		url      string
		expected string
	}{
		{"https://api.openai.com/v1/responses", "output[0].content[0].text"},
		{"https://api.anthropic.com/v1/messages", "choices[0].message.content"},
		{"https://api.openai.com/v1/chat/completions", "choices[0].message.content"},
		{"https://api.openai.com/v1/completions", "choices[0].text"},
		{"https://api.example.com/custom", "."},
	}
	for _, tc := range tests {
		got := responsePath(tc.url)
		if got != tc.expected {
			t.Errorf("responsePath(%q) = %q, want %q", tc.url, got, tc.expected)
		}
	}
}

func TestResolveJSONPath(t *testing.T) {
	data := json.RawMessage(`{
		"choices": [{"message": {"content": "hello"}}],
		"usage": {"prompt_tokens": 10, "completion_tokens": 20, "cost": 0.0015}
	}`)

	content, err := resolveJSONPath(data, "choices[0].message.content")
	if err != nil {
		t.Fatalf("resolveJSONPath failed: %v", err)
	}
	var s string
	if err := json.Unmarshal(content, &s); err != nil {
		t.Fatalf("unmarshal failed: %v", err)
	}
	if s != "hello" {
		t.Errorf("expected 'hello', got %q", s)
	}

	// Test dot path
	tokens, err := resolveJSONPath(data, ".")
	if err != nil {
		t.Fatalf("dot path failed: %v", err)
	}
	if len(tokens) == 0 {
		t.Error("dot path returned empty")
	}

	// Test usage path
	prompt, err := resolveJSONPath(data, "usage.prompt_tokens")
	if err != nil {
		t.Fatalf("usage path failed: %v", err)
	}
	var n float64
	if err := json.Unmarshal(prompt, &n); err != nil {
		t.Fatalf("unmarshal prompt_tokens failed: %v", err)
	}
	if int64(n) != 10 {
		t.Errorf("expected prompt_tokens 10, got %d", int64(n))
	}

	// Test invalid path
	_, err = resolveJSONPath(data, "nonexistent")
	if err == nil {
		t.Error("expected error for nonexistent path")
	}
}

func TestBuildRequest(t *testing.T) {
	ctx := NewNutmeg("gpt-4", "https://api.openai.com/v1/chat/completions", "sk-xxx")
	ctx.Temp = 0.7
	ctx.Tokens = 500

	nut := &Peanuts{
		Persona:  "You are a test.",
		Evidence: "Some evidence.",
		Analysis: "I think...",
		Nudging:  "Do something.",
		Updates:  "Done.",
		Turnout:  "Say something.",
	}

	body := buildRequest(ctx, nut)
	if !strings.Contains(body, `"model":"gpt-4"`) {
		t.Error("body missing model")
	}
	if !strings.Contains(body, `"temperature":0.7`) {
		t.Error("body missing temperature")
	}
	if !strings.Contains(body, `"max_tokens":500`) {
		t.Error("body missing max_tokens")
	}
	if !strings.Contains(body, `"messages"`) {
		t.Error("body missing messages")
	}
}

func TestNutmixChain(t *testing.T) {
	ctx := NewNutmeg("m", "e", "k")
	nut := &Peanuts{}

	msg := NewNutmix(ctx, nut)
	if msg.Ctx != ctx {
		t.Error("ctx not set")
	}
	if msg.Nut != nut {
		t.Error("nut not set")
	}

	// nutoff should not panic
	Nutoff(msg)
}

func TestNutyes(t *testing.T) {
	nut := &Peanuts{}
	res := "test"
	if !Nutyes(nut, &res) {
		t.Error("Nutyes should always return true")
	}
}

func TestCost(t *testing.T) {
	u := Nutuse{
		InTokens:  1000000,
		InCost:    3.0,
		OutTokens: 500000,
		OutCost:   15.0,
	}
	c := u.Cost()
	expected := 1.0*3.0 + 0.5*15.0 // 3 + 7.5 = 10.5
	if c != expected {
		t.Errorf("expected cost %f, got %f", expected, c)
	}
}

func TestSendRejectsBadEndpoint(t *testing.T) {
	ctx := NewNutmeg("m", "http://127.0.0.1:1/nonexistent", "")
	_, err := send(ctx, &Peanuts{
		Persona:  "x",
		Evidence: "x",
		Analysis: "x",
		Nudging:  "x",
		Updates:  "x",
		Turnout:  "x",
	})
	if err == nil {
		t.Error("expected error from bad endpoint")
	}
}

func TestNutjobDefaults(t *testing.T) {
	ctx := NewNutmeg("m", "http://127.0.0.1:1/nonexistent", "")
	// Nut with all empty fields — should fill defaults
	nut := &Peanuts{}
	_, err := Nutjob(ctx, nut)
	if err == nil {
		t.Error("expected error from bad endpoint")
	}
	// Verify defaults were filled
	if nut.Persona != "You are a helpful assistant." {
		t.Errorf("bad default persona: %q", nut.Persona)
	}
	if nut.Evidence != "Respond with a concise but complete answer." {
		t.Errorf("bad default evidence: %q", nut.Evidence)
	}
}

func TestResolveJSONPathNested(t *testing.T) {
	data := json.RawMessage(`{
		"output": [{"content": [{"text": "hello world"}]}]
	}`)
	res, err := resolveJSONPath(data, "output[0].content[0].text")
	if err != nil {
		t.Fatalf("resolve failed: %v", err)
	}
	var s string
	if err := json.Unmarshal(res, &s); err != nil {
		t.Fatalf("unmarshal failed: %v", err)
	}
	if s != "hello world" {
		t.Errorf("expected 'hello world', got %q", s)
	}
}

func TestResolveJSONPathErrors(t *testing.T) {
	data := json.RawMessage(`{"a": [1, 2]}`)
	// Index out of bounds
	_, err := resolveJSONPath(data, "a[5]")
	if err == nil {
		t.Error("expected error for out of bounds index")
	}
}

func TestJSONPathNumberParse(t *testing.T) {
	data := json.RawMessage(`{"a": [{"b": {"c": 42}}]}`)
	res, err := resolveJSONPath(data, "a[0].b.c")
	if err != nil {
		t.Fatalf("resolve failed: %v", err)
	}
	var n float64
	if err := json.Unmarshal(res, &n); err != nil {
		t.Fatalf("unmarshal failed: %v", err)
	}
	if n != 42 {
		t.Errorf("expected 42, got %f", n)
	}
}

func TestJsonTemplateFallback(t *testing.T) {
	tmpl := jsonTemplate("https://api.example.com/v1/completions")
	if !strings.Contains(tmpl, `# SYSTEM:`) {
		t.Error("fallback template should contain # SYSTEM:")
	}
	if !strings.Contains(tmpl, `# ASSISTANT:`) {
		t.Error("fallback template should contain # ASSISTANT:")
	}
}

func TestResolveJSONPathDict(t *testing.T) {
	data := json.RawMessage(`{
		"usage": {
			"prompt_tokens_details": {
				"cached_tokens": 50
			}
		}
	}`)
	res, err := resolveJSONPath(data, "usage.prompt_tokens_details.cached_tokens")
	if err != nil {
		t.Fatalf("resolve failed: %v", err)
	}
	var n float64
	if err := json.Unmarshal(res, &n); err != nil {
		t.Fatalf("unmarshal failed: %v", err)
	}
	if int64(n) != 50 {
		t.Errorf("expected 50, got %d", int64(n))
	}
}
