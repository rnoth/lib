#include <stddef.h>
#include "test.h"
#include "../vec.h"

#define test(VAR, EXPR) do { \
	int *VAR = intvec;        \
	EXPR;                     \
} while (0)

struct test {
	void (*setup)(void);
	void (*do_it)(void);
};

 
struct dummy {
	float f;
	char c;
	int i;
	long l;
};

static void test_alloc_free(void);
static void test_insert(void);
static void cleanup(void);

struct test tests[] = {
	{ 0x0,   test_alloc_free, },
	{ 0x0,   test_insert, },
};

static size_t const tests_len = sizeof tests / sizeof *tests;
static int *intvec;

void
cleanup(void)
{
	if (intvec) {
		try(vec_free(intvec));
		intvec = 0x0;
	}

	printf("done\n");
}

void
test_insert(void)
{
	printf("\tadding some elements to a simple vector");

	ok(!vec_ctor(intvec));
	
	ok(!vec_append(&intvec, (int[]){5}));
	ok(vec_len(intvec) == 1);
	ok(intvec[0] == 5);

	ok(!vec_insert(&intvec, (int[]){3}, 0));
	ok(len(intvec) == 2);
	ok(intvec[0] == 3);
	ok(intvec[1] == 5);

	vec_insert(&intvec, (int[]){312345}, 1);
	ok(len(intvec) == 3);
	ok(intvec[0] == 3);
	ok(intvec[1] == 312345);
	ok(intvec[2] == 5);

	vec_insert(&intvec, (int[]){15557145}, 1);
	ok(len(intvec) == 4);
	ok(intvec[0] == 3);
	ok(intvec[1] == 15557145);
	ok(intvec[2] == 312345);
	ok(intvec[3] == 5);
}

void
test_alloc_free(void)
{
	int *vec = 0x0;
	
	printf("\tallocating & freeing a vector");
	ok(!vec_ctor(intvec));
	ok(vec_len(intvec) ==  0);
}

int
main()
{
	size_t i;

	init_test();

	printf("testing vec.c\n");

	for (i = 0; i < tests_len; ++i) {
		if (tests[i].setup) tests[i].setup();
		if (tests[i].do_it) tests[i].do_it();
		cleanup();
	}

	printf("testing complete (vec.c)\n");

	return 0;
}
