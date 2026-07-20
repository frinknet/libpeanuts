/* (c) 2026 FRINKnet & Friends – MIT licence */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jsio.h>
#include "peanuts.h"

#define NUT_CMD_LEN 2048
#define NUT_ARGV_LEN 128

static thread_local jsio_t *__nut_err = NULL;

static inline int __nut_args(const char* cmd, char*** argv) {
	if (!cmd) return -1;

	char *wrk = malloc(NUT_CMD_LEN);
	char **res = malloc(NUT_ARGV_LEN * sizeof(char*));

	if (!wrk || !res) {
		free(wrk);
		free(res);

		return -1;
	}

	const char* src = cmd;
	char* dst = wrk;
	int in_sq = 0, in_dq = 0;
	int cnt = 0;

	res[cnt++] = dst;

	while (*src && dst < wrk + NUT_CMD_LEN - 1) {
		if (*src == '\\' && src[1]) {
			if (in_sq && src[1] != '\'') *dst++ = *src++;
			else if (in_dq && src[1] != '\"') *dst++ = *src++;
			else src++;
		} else if (*src == '\'' && !in_dq) {
			in_sq = !in_sq;
			src++;

			continue;
		} else if (*src == '\"' && !in_sq) {
			in_dq = !in_dq;
			src++;

			continue;
		} else if (*src == ' ' && !in_sq && !in_dq) {
			if (dst > (cnt > 0 ? res[cnt-1] : wrk)) {
				*dst++ = '\0';
				res[cnt++] = dst;

				if (cnt >= NUT_ARGV_LEN - 1) break;
			}

			src++;

			continue;
		}

		*dst++ = *src++;
	}

	*dst = '\0';
	res[cnt] = NULL;
	*argv = res;

	return cnt;
}

static inline int __nut_pipe(const char* cmd, FILE** pipe_in, FILE** pipe_out, pid_t* pid) {
	int pin[2], pout[2];

	if (pipe(pin) < 0) return -1;

	if (pipe(pout) < 0) {
		close(pin[0]); close(pin[1]);

		return -1;
	}

	*pid = fork();

	if (*pid == 0) {
		close(pin[1]); close(pout[0]);

		if (dup2(pin[0], STDIN_FILENO) == -1) exit(1);
		if (dup2(pout[1], STDOUT_FILENO) == -1) exit(1);

		close(pin[0]);
		close(pout[1]);

		char **argv = NULL;
		int argc = __nut_args(cmd, &argv);

		if (argc <= 0 || !argv) return -1;

		execvp(argv[0], argv);
		exit(127);
	} else if (*pid > 0) {
		close(pin[0]); close(pout[1]);

		*pipe_in = fdopen(pin[1], "w");
		*pipe_out = fdopen(pout[0], "r");

		if (!*pipe_in || !*pipe_out) {
			if (*pipe_in) fclose(*pipe_in);
			if (*pipe_out) fclose(*pipe_out);

			*pipe_in = NULL; *pipe_out = NULL;

			close(pin[1]);
			close(pout[0]);

			return -1;
		}

		return 0;
	} else {
		close(pin[0]); close(pin[1]);
		close(pout[0]); close(pout[1]);

		return -1;
	}
}

static inline const char *__nut_json(const char *posturl) {
	if (strstr(posturl, "/v1/responses")) return
		"{\"model\":\"%s\",\"instructions\":\"%s\","
		"\"input\":["
		"{\"role\":\"user\",\"content\":\"%s\"},"
		"{\"role\":\"assistant\",\"content\":\"%s\"},"
		"{\"role\":\"user\",\"content\":\"%s\"},"
		"{\"role\":\"assistant\",\"content\":\"%s\"},"
		"{\"role\":\"user\",\"content\":\"%s\"}],"
		"\"temperature\":%.1f,\"max_output_tokens\":%d}";

	if (strstr(posturl, "/v1/messages")) return
		"{\"model\":\"%s\",\"messages\":["
		"{\"role\":\"system\",\"content\":\"%s\"},"
		"{\"role\":\"user\",\"content\":\"%s\"},"
		"{\"role\":\"assistant\",\"content\":\"%s\"},"
		"{\"role\":\"user\",\"content\":\"%s\"},"
		"{\"role\":\"assistant\",\"content\":\"%s\"},"
		"{\"role\":\"user\",\"content\":\"%s\"}],"
		"\"temperature\":%.1f,\"max_tokens\":%d}";

	if (strstr(posturl, "/v1/chat/completions")) return
		"{\"model\":\"%s\",\"messages\":["
		"{\"role\":\"system\",\"content\":\"%s\"},"
		"{\"role\":\"user\",\"content\":\"%s\"},"
		"{\"role\":\"assistant\",\"content\":\"%s\"},"
		"{\"role\":\"user\",\"content\":\"%s\"},"
		"{\"role\":\"assistant\",\"content\":\"%s\"},"
		"{\"role\":\"user\",\"content\":\"%s\"}],"
		"\"temperature\":%.1f,\"max_tokens\":%d}";

	return
		"{\"model\":\"%s\",\"prompt\":\""
		"# SYSTEM:\\n%s\\n\\n"
		"# USER:\\n%s\\n\\n"
		"# ASSISTANT:\\n%s\\n\\n"
		"# USER:\\n%s\\n\\n"
		"# ASSISTANT:\\n%s\\n\\n"
		"# USER:\\n%s\\n\\n"
		"# ASSISTANT:\\n"
		"\"temperature\":%.1f,\"max_tokens\":%d}";
}

static inline const char *__nut_path(const char *posturl) {
	if (strstr(posturl, "/v1/responses")) return "output[0].content[0].text";
	if (strstr(posturl, "/v1/messages")) return "choices[0].message.content";
	if (strstr(posturl, "/v1/chat/completions")) return "choices[0].message.content";
	if (strstr(posturl, "/v1/completions")) return "choices[0].text";

	return ".";
}

static inline const char *__nut_prep(const char *txt) {
	if (!txt) return NULL;

	size_t txt_len = strlen(txt);
	size_t dst_cap = txt_len * 2 + 1;
	char *dst = malloc(dst_cap);
	char *p = dst;

	if (!dst) return NULL;

	for (const char *s = txt; *s; s++) {
		switch(*s) {
			case '"':   *p++ = '\\';  *p++ = '"';   break;
			case '\\':  *p++ = '\\';  *p++ = '\\';  break;
			case '\n':  *p++ = '\\';  *p++ = 'n';   break;
			case '\r':  *p++ = '\\';  *p++ = 'r';   break;
			case '\t':  *p++ = '\\';  *p++ = 't';   break;
			default:	*p++ = *s;
		}
	}

	*p = 0;

	return (const char *)dst;
}

static inline const char *__nut_call(nutmeg_t *ctx, peanuts_t *nut) {
	const char *fmt   = __nut_json(ctx->endpoint);
	const char *persona     = __nut_prep( nut->persona);
	const char *evidence    = __nut_prep( nut->evidence);
	const char *analysis    = __nut_prep( nut->analysis);
	const char *nudging     = __nut_prep( nut->nudging);
	const char *updates     = __nut_prep( nut->updates);
	const char *turnout     = __nut_prep( nut->turnout);

	size_t len = 64;

	len += strlen(fmt);
	len += strlen(ctx->model);
	len += strlen(persona);
	len += strlen(evidence);
	len += strlen(analysis);
	len += strlen(nudging);
	len += strlen(updates);
	len += strlen(turnout);

	char *call = malloc(len);

	if (!call) return NULL;

	sprintf(call, fmt, ctx->model, persona, evidence, analysis, nudging, updates, turnout, ctx->temp, ctx->tokens);
	free((void*)persona);
	free((void*)evidence);
	free((void*)analysis);
	free((void*)nudging);
	free((void*)updates);
	free((void*)turnout);

	return (const char*)call;
}

static inline size_t __nut_read(char **buf, size_t *len, FILE *s, size_t *max) {
	if (!*buf) {
		*len = 0;
		*max = 128;
		*buf = malloc(*max);

		if (!*buf) exit(127);
	}

	size_t beg = *len;
	int c;

	while ((c = getc(s)) != EOF) {
		(*buf)[(*len)++] = (char)c;

		if (*len >= *max - 2) {
			size_t old_max = *max;

			*max *= 2;
			*buf = (char *)realloc(*buf, *max);
		}
	}

	(*buf)[*len] = '\0';

	return (*len) - beg;
}

static inline char *__nut_send(nutmeg_t *ctx, peanuts_t *nut) {
	const char *call =  __nut_call(ctx,  nut);
	char cmd[1024];
	pid_t pid = 0;

	// TODO: we should make this use  libcurl or similar
	if (ctx->apikey) {
		snprintf(cmd, sizeof(cmd),
			"curl -s --no-fail --connect-timeout 5 --max-time %d -X POST %s "
			"-H 'Content-Type: application/json' "
			"-H 'Authorization: Bearer %s' "
			"--data-binary @-", ctx->timeout, ctx->endpoint, ctx->apikey);
	} else {
		snprintf(cmd, sizeof(cmd),
			"curl -s --fail --connect-timeout 5 --max-time %d -X POST %s "
			"-H 'Content-Type: application/json' "
			"--data-binary @-", ctx->timeout, ctx->endpoint);
	}

	FILE *pipe_in = NULL;
	FILE *pipe_out = NULL;

	if (__nut_pipe(cmd, &pipe_in, &pipe_out, &pid) != 0) return NULL;

	size_t written = fwrite(call, 1, strlen(call), pipe_in);

	fflush(pipe_in);
	fclose(pipe_in);
	usleep(100000);

	char *recv = NULL;
	size_t cap = 128;
	size_t len = 0;

	__nut_read(&recv, &len, pipe_out, &cap);
	fclose(pipe_out);

	int status = 0;

	if (pid > 0) waitpid(pid, &status, 0);

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		free(recv);

		return NULL;
	}

	jsio_t *json = js_parse(recv);

	if (!json) {
		free(recv);

		return NULL;
	}

	jsio_t *err = js_resolve(json, "error.message");

	if (err && err->type == JS_TYPE_STRING) {
		__nut_err = err;

		return NULL;
	}

	const char *path = __nut_path(ctx->endpoint);

	jsio_t *res = js_resolve(json, path);

	if (!res || res->type != JS_TYPE_STRING || !res->value.str) {
		free(recv);
		js_delete(json);

		return NULL;
	}

	char *cont = strdup(res->value.str);

	// Parse usage tokens from response
	jsio_t *usage = js_resolve(json, "usage");

	ctx->usage.calls++;

	if (usage && usage->type == JS_TYPE_OBJECT) {
		const char *inpPath  = ctx->usage.inPath;
		const char *outPath  = ctx->usage.outPath;
		const char *cchPath  = ctx->usage.cachePath;

		// Auto-detect paths if not overridden
		if (!inpPath)
			inpPath = (strstr(ctx->endpoint, "/v1/messages") || strstr(ctx->endpoint, "/v1/responses"))
				? "input_tokens" : "prompt_tokens";
		if (!outPath)
			outPath = (strstr(ctx->endpoint, "/v1/messages") || strstr(ctx->endpoint, "/v1/responses"))
				? "output_tokens" : "completion_tokens";
		if (!cchPath)
			cchPath = (strstr(ctx->endpoint, "/v1/messages") || strstr(ctx->endpoint, "/v1/responses"))
				? "cache_read_input_tokens" : "prompt_tokens_details.cached_tokens";

		jsio_t *inp   = js_resolve(usage, inpPath);
		jsio_t *out   = js_resolve(usage, outPath);
		jsio_t *cch   = js_resolve(usage, cchPath);
		long totalIn  = (inp && inp->type == JS_TYPE_NUMBER) ? (long)inp->value.num : 0;
		long cached   = (cch && cch->type == JS_TYPE_NUMBER) ? (long)cch->value.num : 0;
		ctx->usage.inTokens  += totalIn - cached;
		ctx->usage.cached    += cached;
		if (out && out->type == JS_TYPE_NUMBER) ctx->usage.outTokens += (long)out->value.num;

		// Parse actual cost if provider returns it (OpenRouter, Portkey, DeepInfra, etc.)
		const char *costPath = ctx->usage.spendPath ? ctx->usage.spendPath : "cost";
		jsio_t *cost = js_resolve(usage, costPath);
		if (cost && cost->type == JS_TYPE_NUMBER) ctx->usage.spend += cost->value.num;
	}

	js_delete(json);
	free(recv);

	return cont;
}

// NUTMEG LIFECYCLE
nutmeg_t *nutmeg(const char *model, const char *endpoint, const char *apikey) {
	nutmeg_t *ctx = malloc(sizeof(nutmeg_t));

	// set the MEG
	ctx->model    = strdup(model);
	ctx->endpoint = strdup(endpoint);
	ctx->apikey  = strdup(apikey);

	// make sure it took
	if (!ctx->model || !ctx->endpoint || !ctx->apikey) {
		nutout(ctx);

		return NULL;
	}

	// defaults
	ctx->usage = (nutuse_t){0};
	ctx->timeout = 300;
	ctx->tokens  = 9000;
	ctx->tries   = 10;
	ctx->pause   = 2000;
	ctx->temp    = 0.3;

	return ctx;
}

char *nutbad(void) {
	if (!__nut_err) return "unknown error";

	char *str = __nut_err->value.str;

	js_delete(__nut_err);

	__nut_err = NULL;

	return str;
}

nutuse_t nutusage(nutmeg_t *ctx) {
	if (!ctx) return (nutuse_t){0};

	return ctx->usage;
}

void nutout(nutmeg_t *ctx) {
	if (!ctx) return;

	free((void*)ctx->model);
	free((void*)ctx->endpoint);
	free((void*)ctx->apikey);
	free(ctx);
}

// PEANUTS ONE SHOT
char *nutjob(nutmeg_t *ctx, peanuts_t *nut) {
	int i = ctx->tries;
	char *res;

	// Make sure the nut is good before we try to crack it
	if (!nut->persona)      nut->persona      = strdup("You are a helful assistant.");
	if (!nut->evidence)     nut->evidence     = strdup("Respond with a concise but coomplete answer.");
	if (!nut->analysis)     nut->analysis     = strdup("Got it. What do you want me to do?");
	if (!nut->nudging)      nut->nudging      = strdup("Help me understand this better.");
	if (!nut->updates)      nut->updates      = strdup("Okay. So I need to think through this before I respond.");
	if (!nut->turnout)      nut->turnout      = strdup("Respond with a concise but coomplete answer.");

	// Loop as many tries as allowd
	while (i--) {
		//Send to AI
		res = __nut_send(ctx, nut);

		// Respond if we are good
		if (!nut->safety || nut->safety(nut, &res)) return res;

		// Otherwise, we get ready to try again
		free(res);
		usleep((useconds_t)ctx->pause * 1000);
	}

	// Apparently we failed
	__nut_err = JS_STRING("No more tries.");

	return NULL;
}

// NUTMIX LIFECYLCLE
nutmix_t *nutmix(nutmeg_t *ctx, peanuts_t *nut) {
	nutmix_t *msg = malloc(sizeof(nutmix_t));

	msg->ctx = ctx;
	msg->nut = nut;
	msg->text = NULL;

	return msg;
}

nutmix_t *nutsay(nutmix_t **msg, const char *say) {
	nutmeg_t *ctx = (*msg)->ctx;
	peanuts_t *nut = (*msg)->nut;
	nutmix_t *tlk = nutmix(ctx, nut);
	nutmix_t *res = nutmix(ctx, nut);

	tlk->prev = *msg;
	tlk->self = true;
	tlk->text = strdup(say);
	res->prev = nutfix(&tlk, ctx, nut);
	res->text = nutjob(ctx, nut);
	*msg = res;

	return res;
}

nutmix_t *nutfix(nutmix_t **msg, nutmeg_t *ctx, peanuts_t *nut) {
	int i = 0;
	size_t len = 0;
	char *chat =  NULL;
	nutmix_t *cur = nutmix(ctx, nut);

	// Populate new message
	cur->self = true;
	cur->prev = *msg;
	cur->text = nut->updates;

	// Loop messages
	while (cur->prev && ++i) {
		if (cur->text) {
			len += strlen(cur->text) + 14;

			char *tmp = malloc(len);

			len = snprintf(tmp, len, "%s%s:\n%s\n\n", chat, cur->self ? "USER" : "ASSISTANT", cur->text);

			free(chat);

			chat = tmp;
		}

		cur = cur->prev;
	}

	// Check Convo length
	if (i > ctx->tries) {
		peanuts_t shrink = {
			.persona     = "You compact conversations without loosing their escence.",
			.evidence    = chat,
			.analysis    = "So we need to shrink this to around 1000-2000 tokens.",
			.nudging     = "Yes. But we don't want to lose the important points.",
			.updates     = "Okay so I'm free to change the turns as long as I keep the focused.",
			.turnout     = "Respond with a compat conversation keeping the general turn taking.",
		};
		nutmix_t *newmsg = nutmix(ctx, nut);

		newmsg->self = true;
		newmsg->text = nutjob(ctx, &shrink);

		nutoff(*msg);
		free(chat);

		chat = strdup(newmsg->text);
		*msg = newmsg;
	}

	nut->nudging = chat;

	return *msg;
}

void nutoff(nutmix_t *msg) {
	while (msg) {
		nutmix_t *tmp = msg->prev;

		free((void *)msg->text);
		free(msg);

		msg = tmp;
	}
}
