#ifndef _test_
#define _test_
#include <assert.h>
#include <signal.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define safely(EXPR) do {                                   \
	switch (sigsetjmp(env, 1)) {                        \
	case SIGSEGV:                                       \
		snprintf(paren, 64, "segfault");            \
		break;                                      \
	case SIGBUS:                                        \
		snprintf(paren, 64, "bus error");           \
		break;                                      \
	case SIGABRT:                                       \
		snprintf(paren, 64, "aborted");             \
		break;                                      \
	case 0:                                             \
		if (TEST) {                                 \
			printf(".");                        \
			continue;                           \
		}                                           \
		snprintf(paren, 64, "@%d", __LINE__);       \
	}                                                   \

#define ok(TEST) do {                                       \
	volatile char paren[64];                            \
	int sig = sigsetjmp(env, 1);                        \
	bool res = (TEST);                                  \
	if (!sig && res) break;                             \
	if (!sig) paren[0] = 0;                             \
	else snprintf((char*)paren, 64, "-- %s",            \
		      sigtostr(sig));                       \
	test_failed(#TEST, (char*)paren, __LINE__);         \
	raise(SIGTRAP);                                     \
} while (0)

#define try(ACT) do { ok((ACT, 1)); printf("."); } while (0)
#define expect(VAL, TEST) do {                              \
	volatile char expected[64] = "";                    \
	int sig = sigsetjmp(env, 1);                        \
	int res = (TEST);                                   \
	if (!sig && res == (VAL)) { printf("."); break; }   \
	if (!sig) snprintf((char*)expected, 64, "%d", res); \
	else snprintf((char*)expected, 64, "%s",            \
			sigtostr(sig));                     \
	expect_failed(#TEST,#VAL,(char*)expected, __LINE__);\
	raise(SIGTRAP);                                     \
} while (0)

static
char *
sigtostr(int src)
{
	switch (src) {
	case SIGSEGV: return "segfault";
	case SIGBUS:  return "bus error";
	case SIGABRT: return "aborted";
	default:
		assert(!"I will never be reached");
	}
}

static
void
expect_failed(char *expr, char *expected, char *got, int lineno)
{
	int err = 0;
	err = printf("\r\033[K!!! expect failed: ``%s'' expected %s, got %s (#%d)\n", expr, expected, got, lineno);
	if (err < 0) {
		perror("printf");
		exit(1);
	}
}

static
void
test_failed(char *expr, char *sigmsg, int lineno)
{
	int err = 0;
	err = printf("\r\033[K!!! test failed: ``%s'' %s(#%d)\n", expr, sigmsg, lineno);
	if (err < 0) {
		perror("printf");
		exit(1);
	}

	err = fflush(stdout);
	if (err == -1) {
		perror("fflush");
		exit(1);
	}
}

static sigjmp_buf env;

static
void
fault(int sig)
{
	siglongjmp(env, sig);
}

static
void
init_test(void)
{
	struct sigaction sa = {0};

	// enable x86 alignment check
	// commented out as it causes most stdlibs to crash
	//__asm__("pushf\n"  
	//          "orl $0x40000, (%rsp)\n"  
	//          "popf"); 

	sa.sa_handler = fault;

	if (sigaction(SIGSEGV, &sa, 0x0)) {
		perror("sigaction");
		abort();
	}

	// mostly useless without alignment checks
	if (sigaction(SIGBUS, &sa, 0x0)) {
		perror("sigaction");
		abort();
	}

	if (sigaction(SIGABRT, &sa, 0x0)) {
		perror("sigaction");
		abort();
	}

	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGTRAP, &sa, 0x0)) {
		perror("sigaction");
		abort();
	}

}

#endif
