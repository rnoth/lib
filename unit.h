#ifndef _test_
#define _test_
#include <signal.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// TODO this all should be cleaned up

#define ok(TEST) do {                         \
	if (!unit_has_init) unit_init();      \
	int _signal = sigsetjmp(env, 1);      \
	int _result = -1;                     \
	alarm(3);                             \
	if (!_signal) _result = (TEST);       \
	alarm(0);                             \
	if (!_signal && _result) break;       \
	ok_failed(#TEST, __LINE__, _signal);  \
	raise(SIGTRAP);                       \
	siglongjmp(checkpoint, 1);            \
} while (0)

#define try(ACT) ok((ACT, 1))
#define expect(VAL, TEST) expectm(VAL, TEST, #TEST)
#define expectf(VAL, TEST, ...) do {    \
	char buf[256];                  \
	snprintf(buf, 256, __VA_ARGS__);\
	expectm(VAL, TEST, buf);       \
} while (0)

#define expectm(VAL, TEST, MSG) do {                \
	if (!unit_has_init) unit_init();            \
	int _signal = sigsetjmp(env, 1);            \
	int _result = -1;                           \
	alarm(3);                                   \
	int _expect = (VAL);                        \
	if (!_signal) _result = (TEST);             \
	alarm(0);                                   \
	if (!_signal && _result == _expect) break;  \
	expect_failed(MSG,__LINE__,                 \
	              _expect,_result,_signal);     \
	raise(SIGTRAP);                             \
	siglongjmp(checkpoint, 1);                  \
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
extern bool unit_has_init;
extern sigjmp_buf env;
extern sigjmp_buf checkpoint;
extern size_t total_failures;

void expect_failed(char *, int , int, int, int);
void unit_init(void);
void ok_failed(char *, int, int);

#endif
