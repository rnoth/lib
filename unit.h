#ifndef _test_
#define _test_
#include <signal.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define unit_begin(name, ln, ...) do {    \
	if (!unit_has_init) unit_init(); \
	unit_set_lineno(ln);             \
	unit_##name##_init(__VA_ARGS__); \
	alarm(3);                        \
} while (0)

#define ok(TEST) do {                  \
	unit_begin(ok,__LINE__,#TEST); \
	int unit_result = (TEST);      \
	alarm(0);                      \
	if (unit_result) break;        \
	unit_longjmp();                \
} while (0)

#define try(TEST) do {                  \
	unit_begin(try,__LINE__,#TEST); \
	TEST;                           \
	alarm(0);                       \
} while (0)

#define expect(VAL, TEST) expectm(VAL, TEST, #TEST)
#define expectf(VAL, TEST, ...) do {    \
	char buf[256];                  \
	snprintf(buf, 256, __VA_ARGS__);\
	expectm(VAL, TEST, buf);        \
} while (0)

#define expectm(VAL, TEST, MSG) do {            \
	unit_begin(expect, __LINE__, VAL, MSG); \
	int unit_result = (TEST);               \
	alarm(0);                               \
	if (unit_expected(unit_result)) break;  \
	else unit_longjmp();                    \
} while (0)

struct test {
	char  *msg;
	void (*setup)();
	void (*test)();
	void (*cleanup)();
	void  *ctx;
};

// must be defined
extern struct test  unit_tests[];
extern char         unit_filename[];

// provided
extern bool       unit_has_init;
extern size_t     unit_total_failures;

void unit_longjmp(void);
void unit_fail(void);
void unit_ok_init(char *);
void unit_expect_init(int, char *);
bool unit_expected(int);
void unit_set_lineno(int);
void unit_try_init(char *);
void unit_init(void);

#endif
