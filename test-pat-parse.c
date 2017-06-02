#include <unit.h>
#include <pat-parse.c>

char filename[] = "pat-parse.c";

struct test tests[] = {
	{ 0x0, 0x0, "" },
};
size_t const tests_len = arr_len(tests);

void cleanup() { return; }
