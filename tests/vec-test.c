#include <stddef.h>
#include "test.h"
#include "../vec.h"
#include "../util.h"

#define test(VAR, EXPR) do { \
	int *VAR = intvec;        \
	EXPR;                     \
} while (0)

struct test {
	int (*setup)(void);
	int (*do_it)(void);
	char *msg;
};

 
static int  test_alloc(void);
static int  test_delete(void);
static int  test_elim(void);
static int  test_insert(void);
static int  test_large_insert(void);
static void cleanup(void);

struct test tests[] = {
	{ 0x0,                test_alloc,         "allocating & freeing a vector", },
	{ test_alloc,         test_insert,        "adding some elements to a simple vector", },
	{ 0x0,                test_large_insert,  "adding a large number of elements", },
	{ test_large_insert,  test_delete,        "deleting elements from vector", },
	{ test_large_insert,  test_elim,          "eliminating elements from vector", },
};

static size_t const tests_len = sizeof tests / sizeof *tests;
static int *intvec;
static int *intvec2;

void
cleanup(void)
{
	if (intvec) {
		try(vec_free(intvec));
		intvec = 0x0;
	}

	if (fflush(stdout)) {
		perror("fflush");
		exit(1);
	}

	failures = 0;
}

int
test_alloc(void)
{
	int *vec = 0x0;
	
	ok(!vec_ctor(intvec));
	ok(vec_len(intvec) ==  0);

	return failures ? -1 : 0;
}

int
test_delete(void)
{
	for (int i = 501; i < 600; ++i) {
		vec_delete(&intvec, 500);
		expect(900U + 600U - i, vec_len(intvec));
		ok(intvec[500] == i);
		if (failures) return -1;
	}

	return 0;
}

int
test_elim(void)
{
	vec_elim(&intvec, 200, 400);
	expect(600, vec_len(intvec));

	if (failures) return -1;

	for (int i = 0; i < 200; ++i) {
		ok(intvec[i] == i);
		if (failures) return -1;
	}

	for (int i = 200; i < 600; ++i) {
		ok(intvec[i] == i + 400);
		if (failures) return -1;
	}

	return 0;
}

int
test_insert(void)
{
	expect(0, vec_append(&intvec, (int[]){5}));
	ok(vec_len(intvec) == 1);
	ok(intvec[0] == 5);

	if (failures) return -1;

	expect(0, vec_insert(&intvec, (int[]){3}, 0));
	ok(len(intvec) == 2);
	ok(intvec[0] == 3);
	ok(intvec[1] == 5);

	if (failures) return -1;

	expect(0, vec_insert(&intvec, (int[]){312345}, 1));
	ok(len(intvec) == 3);
	ok(intvec[0] == 3);
	ok(intvec[1] == 312345);
	ok(intvec[2] == 5);

	if (failures) return -1;

	expect(0, vec_insert(&intvec, (int[]){15557145}, 1));
	ok(len(intvec) == 4);
	ok(intvec[0] == 3);
	ok(intvec[1] == 15557145);
	ok(intvec[2] == 312345);
	ok(intvec[3] == 5);

	return failures ? -1 : 0;
}

int
test_large_insert(void)
{
	ok(!vec_ctor(intvec));

	for (int i = 0; i < 1000; ++i) {
		expect(0, vec_append(&intvec, &i));
		ok(intvec[i] == i);
		ok(vec_len(intvec) == (size_t)i + 1);
		if (failures) return -1;
	}

	return 0;
}

int
main()
{
	size_t i;

	init_test();

	printf("testing vec.c\n");

	for (i = 0; i < tests_len; ++i) {
		if (tests[i].setup) if (tests[i].setup()) continue;

		printf("\t%s\n", tests[i].msg);
		if (tests[i].do_it) tests[i].do_it();

		cleanup();
	}

	printf("testing finished (vec.c) -- %zu failed tests\n", total_failures);

	return 0;
}
