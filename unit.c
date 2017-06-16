#include <unit.h>
#include <util.h>

bool unit_has_init;
sigjmp_buf env;
sigjmp_buf checkpoint;
size_t total_failures;

static char stack[MINSIGSTKSZ];
static void fault(int);
static char *sigtostr(int);
static void test_failed(char *);
static void print_lineno(int);

char *
sigtostr(int src)
{
	switch (src) {
	case SIGILL:  return "illegal instruction";
	case SIGSEGV: return "segfault";
	case SIGBUS:  return "bus error";
	case SIGABRT: return "abort";
	case SIGALRM: return "timeout";
	}

	return "INTERNAL ERROR";
}

void
test_failed(char *test)
{
	int err;
	err = printf("!!! test failed: %s ", test);
	if (err < 0) die("printf failed");
}

void
print_lineno(int lineno)
{
	int err;
	err = printf("(#%d)", lineno);
	if (err < 0) die("printf failed");
}

void
expect_failed(char *test, int lineno, int expected, int actual,int signum)
{
	int err = 0;
	char got[64];

	test_failed(test);

	if (signum) sprintf(got, "%s", sigtostr(signum));
	else sprintf(got, "%d", actual);

	err = printf("-- expected %d, got %s ", expected, got);
	if (err < 0) die("printf failed");

	print_lineno(lineno);

	printf("\n");

	err = fflush(stdout);
	if (err == -1) die("fflush failed");

	++total_failures;
}

void
ok_failed(char *test, int lineno, int signum)
{
	int err = 0;

	test_failed(test);

	if (signum) {
		err = printf("-- %s", sigtostr(signum));
		if (err < 0)  die("printf failed");
	}

	print_lineno(lineno);

	printf("\n");

	err = fflush(stdout);
	if (err == -1) die("fflush failed");

	++total_failures;
}

void
fault(int sig)
{
	siglongjmp(env, sig);
}

void
unit_init(void)
{
	struct sigaction sa[1] = {{0}};
	stack_t st[1] = {{.ss_sp = stack, .ss_size = MINSIGSTKSZ}};

	if (unit_has_init) return;

	if (sigaltstack(st,0)) die("sigaltstack failed");

	// enable x86 alignment check
	// commented out as it causes most stdlibs to crash
	//__asm__("pushf\n"  
	//        "orl $0x40000, (%rsp)\n"  
	//        "popf"); 

	sa->sa_handler = fault;
	sa->sa_flags = SA_ONSTACK;

	if (sigaction(SIGSEGV, sa, 0x0)) die("sigaction failed");
	if (sigaction(SIGALRM, sa, 0x0)) die("sigaction failed");
	if (sigaction(SIGILL,  sa, 0x0)) die("sigaction failed");

	// mostly useless without alignment checks
	if (sigaction(SIGBUS, sa, 0x0)) die("sigaction failed");
	if (sigaction(SIGABRT, sa, 0x0)) die("sigaction failed");

	sa->sa_handler = SIG_IGN;
	if (sigaction(SIGTRAP, sa, 0x0)) die("sigaction failed");

	unit_has_init = true;
}
