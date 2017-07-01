#include <unit.h>

void fail();

char unit_filename[] = "fail.c";
struct test unit_tests[] = {
	{ "whoops", 0, fail },
	{0},
};

void
fail()
{
	volatile int *null = 0;
	try(*null);
}
