/* (c) 2026 FRINKnet & Friends – MIT licence */

#ifndef PEANUTS_H
#define PEANUTS_H

#include <stdio.h>
#include <stdbool.h>

// ONESHOT REQUEST
typedef struct peanuts {
	const char *persona;      // what role the AI plays
	const char *exposition;   // what the AI should know
	const char *analysis;     // what the AI should think
	const char *needs;        // what the USER asked for
	const char *updates;      // what the AI responded
	const char *templates;    // what to return template

	// Test of it did the right thing
	bool (*safety)(struct peanuts *nut, char **res);
} peanuts_t;

// BASIC SETTINGS
typedef struct nutmeg {
	const char *model;      // AI model name
	const char *endpoint;   // AI endpoint url
	const char *gatekey;    // API gatekey
	int        timeout;     // seconds
	int        tokens;      // tokens
	int        tries;       // tries
	int        pause;       // ms
	double     temp;        // temp
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
nutmeg_t *nutmeg(const char *model, const char *endpoint, const char *gatekey);
void nutout(nutmeg_t *ctx);

// PEANUTS CALLING
char *nutjob(nutmeg_t *ctx, peanuts_t *nut);
char *nutbad(void);

// NUTMIX CHAT CYCLE
nutmix_t *nutmix(nutmeg_t *ctx, peanuts_t *nut);
nutmix_t *nutsay(nutmix_t **msg, const char *say);
nutmix_t *nutfix(nutmix_t **msg, nutmeg_t *ctx, peanuts_t *nut);
void nutoff(nutmix_t *msg);

#endif /* PEANUTS_H */
