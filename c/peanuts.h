/* (c) 2026 FRINKnet & Friends – MIT licence */

#ifndef PEANUTS_H
#define PEANUTS_H

#include <stdio.h>
#include <stdbool.h>

// ONESHOT REQUEST
typedef struct peanuts {
	char *persona;  // what role the AI plays
	char *evidence; // what the AI should know
	char *analysis; // what the AI should think
	char *nudging;  // what the USER asks for
	char *updates;  // what the AI responded
	char *turnout;  // how the AI should respond

	// Test of it did the right thing
	bool (*safety)(struct peanuts *nut, char **res);

	void *data; // what you want to rember betwen loops
} peanuts_t;

// USAGE TRACKING
typedef struct nutuse {
	long   inTokens;    // standard prompt tokens (accumulated, auto-subtracted from cached)
	long   cached;      // cache hit tokens (accumulated)
	long   outTokens;   // completion tokens (accumulated)
	long   calls;       // API calls (accumulated)
	double inCost;      // cost per 1M input tokens (set by user)
	double cacheCost;   // cost per 1M cache hit tokens (set by user, usually ~10% of inCost)
	double outCost;     // cost per 1M output tokens (set by user)
	double callsCost;   // cost per API call (set by user for aggregators)
	double spend;       // actual cost from provider if returned (accumulated)
	const char *inPath;     // JSON path for total input tokens (NULL=auto)
	const char *cachePath;  // JSON path for cached tokens (NULL=auto)
	const char *outPath;    // JSON path for output tokens (NULL=auto)
	const char *spendPath;  // JSON path for cost (NULL="cost")
} nutuse_t;

// BASIC SETTINGS
#define NUTUSE_COST(nu) ((nu).inTokens / 1000000.0 * (nu).inCost + (nu).cached / 1000000.0 * (nu).cacheCost + (nu).outTokens / 1000000.0 * (nu).outCost + (nu).calls * (nu).callsCost)

typedef struct nutmeg {
	const char *model;      // AI model name
	const char *endpoint;   // AI endpoint url
	const char *apikey;     // API apikey
	int        timeout;     // seconds
	int        tokens;      // tokens
	int        tries;       // tries
	int        pause;       // ms
	double     temp;        // temp

	nutuse_t   usage;       // token tracking (accumulated)
} nutmeg_t;

// CHAT CONVERSATION
typedef struct nutmix {
	nutmeg_t *ctx;
	peanuts_t *nut;
	bool self;
	const char *text;
	struct nutmix *prev;
} nutmix_t;

// NUTMEG LIFECYCLE
nutmeg_t *nutmeg(const char *model, const char *endpoint, const char *apikey);
void nutout(nutmeg_t *ctx);
nutuse_t nutuse(nutmeg_t *ctx);

// PEANUTS CALLING
char *nutjob(nutmeg_t *ctx, peanuts_t *nut);
char *nutbad(void);

// NUTMIX CHAT CYCLE
nutmix_t *nutmix(nutmeg_t *ctx, peanuts_t *nut);
nutmix_t *nutsay(nutmix_t **msg, const char *say);
nutmix_t *nutfix(nutmix_t **msg, nutmeg_t *ctx, peanuts_t *nut);
void nutoff(nutmix_t *msg);

#endif /* PEANUTS_H */
