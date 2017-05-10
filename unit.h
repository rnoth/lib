#ifndef _test_
#define _test_
#include <assert.h>
#include <signal.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ok(TEST) do {                                       \
	volatile char paren[64];                            \
	int sig = sigsetjmp(env, 1);                        \
	bool res = -1;                                      \
	if (!sig) res = (TEST);                             \
	if (!sig && res) break;                             \
	if (!sig) paren[0] = 0;                             \
	else snprintf((char*)paren, 64, "-- %s ",           \
		      sigtostr(sig));                       \
	ok_failed(#TEST, (char*)paren, __LINE__);           \
	raise(SIGTRAP);                                     \
	siglongjmp(checkpoint, 1);                          \
} while (0)

#define try(ACT) ok((ACT, 1))
#define expect(VAL, TEST) do {                              \
	volatile char expected[64] = "";                    \
	int sig = sigsetjmp(env, 1);                        \
	int res = -1;                                       \
	if (!sig) res = (TEST);                             \
	int hope = (VAL);                                   \
	if (!sig && res == hope) break;                     \
	if (!sig) snprintf((char*)expected, 64, "%d", res); \
	else snprintf((char*)expected, 64, "%s",            \
			sigtostr(sig));                     \
	expect_failed(#TEST,hope,(char*)expected, __LINE__);\
	raise(SIGTRAP);                                     \
	siglongjmp(checkpoint, 1);                          \
} while (0)

#define on_failure if (sigsetjmp(checkpoint, 1))

struct test {
	void (*setup)(void);
	void (*do_it)(void);
	char  *msg;
};

// must be defined
extern struct test  tests[];
extern size_t const tests_len;
extern char filename[];

void cleanup(void);

// provided
extern sigjmp_buf env;
extern sigjmp_buf checkpoint;
extern size_t total_failures;

void expect_failed(char *, int , char *, int);
void init_test(void);
void ok_failed(char *, char *, int);
char *sigtostr(int);

#endif
