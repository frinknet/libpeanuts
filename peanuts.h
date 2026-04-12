/* (c) 2026 FRINKnet & Friends – MIT licence */

#ifndef PEANUTS_H
#define PEANUTS_H

#include <stdbool.h>

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

typedef struct peanuts {
	const char *persona;      // what role the AI plays
	const char *environment;  // what the AI should know
	const char *analysis;     // what the AI should think
	const char *negotiation;  // what the USER asked for
	const char *updates;      // what the AI responded
	const char *templates;    // what to return template

	// Test of it did the right thing
	bool (*safety)(struct peanuts *nut, char **res);
} peanuts_t;

typedef struct nutmsg {
	nutmeg_t *ctx;
	peanuts_t *nut;
	bool self;
	const char *text;
	struct nutmsg *prev;
} nutmsg_t;

nutmeg_t *nutmeg(const char *model, const char *endpoint, const char *gatekey);
char *nutjob(nutmeg_t *ctx, peanuts_t *nut);
char *nutbad(void);
bool nutyes(peanuts_t *nut, char **res);
nutmsg_t *nutmsg(nutmeg_t *ctx, peanuts_t *nut);
nutmsg_t *nutsay(nutmsg_t *msg, const char *say);
nutmsg_t *nutfix(nutmsg_t *msg, nutmeg_t *ctx, peanuts_t *nut);
void nutclr(nutmsg_t *msg);
void nutout(nutmeg_t *ctx);

#endif /* PEANUTS_H */
