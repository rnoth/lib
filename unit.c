#include <unit.h>
#include <util.h>

#define printf(...) do {                   \
	int err = printf(__VA_ARGS__);     \
	if (err < 0) die("printf failed"); \
} while (0)

bool   unit_has_init;
size_t unit_total_failures;

static char       handler_stack[MINSIGSTKSZ];
static char       current_test[256];
static char       error_message[256];
static int        line_number;
static int        expected;
static sigjmp_buf checkpoint;

static void  fault(int);

void
fault(int sig)
{
	switch (sig) {
	case SIGILL:  strcpy(error_message, "illegal instruction"); break; 
	case SIGSEGV: strcpy(error_message, "segfault");            break; 
	case SIGBUS:  strcpy(error_message, "bus error");           break; 
	case SIGABRT: strcpy(error_message, "abort");               break;
	case SIGALRM: strcpy(error_message, "timeout");             break;
	}

	siglongjmp(checkpoint, 1);
}

void
unit_fail(void)
{
	++unit_total_failures;
	printf("failed\n");
	printf("!!! test failed: ");
	printf("%s", current_test);
	printf(" -- ");
	printf("%s", error_message);
	printf("\n");
}

bool
unit_expected(int res)
{
	return expected == res;
}

void
unit_expect_init(int exp, char *msg)
{
	expected = exp;
	snprintf(current_test, 256, "%s -- expected %d, got", msg, exp);
}

void
unit_ok_init(char *test)
{
	strcpy(current_test, test);
	error_message[0] = 0;
}

void
unit_try_init(char *test)
{
	strcpy(current_test, test);
	error_message[0] = 0;
}

void
unit_set_lineno(int ln)
{
	line_number = ln;
}
 
void
unit_init(void)
{
	struct sigaction sa[1] = {{0}};
	stack_t st[1] = {{.ss_sp = handler_stack, .ss_size = MINSIGSTKSZ}};

	if (unit_has_init) return;

	if (sigaltstack(st,0)) die("sigaltstack failed");

	sa->sa_handler = fault;
	sa->sa_flags = SA_ONSTACK;

	if (sigaction(SIGSEGV, sa, 0x0)) die("sigaction failed");
	if (sigaction(SIGALRM, sa, 0x0)) die("sigaction failed");
	if (sigaction(SIGILL,  sa, 0x0)) die("sigaction failed");
	if (sigaction(SIGBUS,  sa, 0x0)) die("sigaction failed");
	if (sigaction(SIGABRT, sa, 0x0)) die("sigaction failed");

	sa->sa_handler = SIG_IGN;
	if (sigaction(SIGTRAP, sa, 0x0)) die("sigaction failed");

	unit_has_init = true;
}

#define callq(F, A) do if (F) F(A); while (0)
#define on_failure if (sigsetjmp(checkpoint, 1))

int
main()
{
	volatile size_t i = 0;

	unit_init();

	printf("testing %s\n", unit_filename);

	for (i = 0; unit_tests[i].msg; ++i) {
		on_failure {
			unit_fail();
			on_failure continue;
			callq(unit_tests[i].cleanup, unit_tests[i].ctx);
			continue;
		}

		printf("\t%s...", unit_tests[i].msg);
		callq(unit_tests[i].setup,   unit_tests[i].ctx);
		callq(unit_tests[i].test,    unit_tests[i].ctx);
		callq(unit_tests[i].cleanup, unit_tests[i].ctx);
		printf("ok\n");
	}

	printf("testing finished (%s)", unit_filename);
	printf(" -- %zu failed tests\n", unit_total_failures);

	return 0;
}
