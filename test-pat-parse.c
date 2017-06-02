#include <unit.h>
#include <pat-parse.c>

void test_process(void);

struct test tests[] = {
	{ 0x0, test_process, "testing the parser" },
};
size_t const tests_len = arr_len(tests);
char filename[] = "pat-parse.c";

struct state st[1];

void cleanup() { st_fini(st); }

void test_process(void)
{
	st_init(st, "foo");
	pat_process(st);
}
