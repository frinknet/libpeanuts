/* (c) 2026 FRINKnet & Friends – MIT licence */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jsio.h>
#include "peanuts.h"

static thread_local jsio_t *__nut_err = NULL;

static inline int __nut_args(const char* cmd, char*** argv) {
	if (!cmd) return -1;

	char *wrk = malloc(MAX_CMD_LEN);
	char **res = malloc(MAX_ARGV_LEN * sizeof(char*));

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

	while (*src && dst < wrk + MAX_CMD_LEN - 1) {
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

				if (cnt >= MAX_ARGV_LEN - 1) break;
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
	const char *environment = __nut_prep( nut->environment);
	const char *analysis    = __nut_prep( nut->analysis);
	const char *negotiation = __nut_prep( nut->negotiation);
	const char *updates     = __nut_prep( nut->updates);
	const char *templated   = __nut_prep( nut->templated);

	size_t len = 64;

	len += strlen(fmt);
	len += strlen(ctx->model);
	len += strlen(persona);
	len += strlen(environment);
	len += strlen(analysis);
	len += strlen(negotiation);
	len += strlen(updates);
	len += strlen(templated);

	char *call = malloc(len);

	if (!call) return NULL;

	sprintf(call, fmt, ctx->model, persona, environment, analysis, negotiation, updates, templated, ctx->temp, ctx->tokens);
	free(persona);
	free(environment);
	free(analysis);
	free(negotiation);
	free(updates);
	free(templated);

	return (const char*)call;
}

static inline size_t __nut_read(char **buf, size_t *len, FILE *s, size_t *max) {
	if (!*buf) {
		*len = 0;
		*max = 128;
		*buf = malloc(*max);

		if (!*buf) die("Out of memory.");
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

static inline const char *__nut_send(nutmeg_t *ctx, peanuts_t *nut) {
	const char *call =  __nut_call(ctx,  nut);
	char cmd[1024];
	pid_t pid = 0;

	// TODO: we should make this use  libcurl or similar
	if (ctx->gatekey) {
		snprintf(cmd, sizeof(cmd),
			"curl -s --no-fail --connect-timeout 5 --max-time %d -X POST %s "
			"-H 'Content-Type: application/json' "
			"-H 'Authorization: Bearer %s' "
			"--data-binary @-", ctx->timeout, ctx->endpoint, ctx->gatekey);
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

	js_delete(json);
	free(recv);

	return cont;
}


nutmeg_t *nutmeg(const char *model, const char *endpoint, const char *gatekey) {
	nutmeg_t *ctx = malloc(sizeof(nutmeg_t));

	// set the MEG
	ctx->model    = strdup(model);
	ctx->endpoint = strdup(endpoint);
	ctx->gatekey  = strdup(gatekey);

	// make sure it took
	if (!ctx->model || !ctx->endpoint || !ctx->gatekey) {
		nutout(ctx);

		return NULL;
	}

	// defaults
	ctx->timeout = 300;
	ctx->tokens  = 9000;
	ctx->tries   = 10;
	ctx->pause   = 2000;
	ctx->temp    = 0.3;

	return ctx;
}

char *nutjob(nutmeg_t *ctx, peanuts_t *nut) {
	int i = ctx->tries;
	char *res;

	while (i--) {
		res = __nut_send(ctx, nut);

		if (nut->safety(nut, &res)) return res;

		free(res);

		usleep((useconds_t)ctx->pause * 1000);
	}

	return NULL;
}

char *nutbad(void) {
	if (!__nut_err) return strdup("unknown error");

	char *str = strdup(__nut_err->value.str);

	js_delete(__nut_err);

	__nut_err = NULL;

	return str;
}

void nutout(nutmeg_t *ctx) {
	if (!ctx) return;

	free((char *)ctx->model);
	free((char *)ctx->endpoint);
	free((char *)ctx->gatekey);
	free(ctx);
}
