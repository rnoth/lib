#include "unit.h"

sigjmp_buf env;
sigjmp_buf checkpoint;
size_t total_failures;

static void fault(int);

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

void
expect_failed(char *expr, int expected, char *got, int lineno)
{
	int err = 0;
	err = printf("\r\033[K!!! test failed: ``%s''; expected %d, got %s (#%d)\n",
			expr, expected, got, lineno);
	if (err < 0) {
		perror("printf");
		exit(1);
	}

	++total_failures;
}

void
ok_failed(char *expr, char *sigmsg, int lineno)
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

	++total_failures;
}

void
fault(int sig)
{
	siglongjmp(env, sig);
}

void
init_test(void)
{
	struct sigaction sa = {0};

	// enable x86 alignment check
	// commented out as it causes most stdlibs to crash
	//__asm__("pushf\n"  
	//        "orl $0x40000, (%rsp)\n"  
	//        "popf"); 

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
