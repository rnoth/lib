#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test.h"
#include "../vec.h"

/* FIXME: no tests for vec_transfer(), vec_copy(), or vec_elim() */

int *vec, *clone;
size_t i;
int elem;

static
void
test_alloc(void)
{
	printf("\tallocating a vector...");
	vec_ctor(vec);
	ok(len(vec) == 0);
	printf("done\n");

}

static
void
test_clone(void)
{
	printf("\tcloning vector...");
	clone = vec_clone(vec);
	ok(clone);
	ok(!memcmp(vec, clone, len(vec) * sizeof *vec));
	printf("done\n");
}

static
void
test_concat(void)
{
	printf("\tconcatenating a vector...");
	vec_truncate(&vec, 0);
	vec_concat(&vec, clone, len(clone));
	printf("done\n");
}

static
void
test_insert(void)
{
	printf("\tadding a few ints...");
	elem = 5;
	vec_append(&vec, &elem);
	ok(len(vec) == 1);
	ok(vec[0] == 5);

	elem = 3;
	vec_insert(&vec, &elem, 0);
	ok(len(vec) == 2);
	ok(vec[0] == 3);
	ok(vec[1] == 5);

	elem = 2345;
	vec_insert(&vec, &elem, 1);
	ok(len(vec) == 3);
	ok(vec[0] == 3);
	ok(vec[1] == 2345);
	ok(vec[2] == 5);
	printf("done\n");
}

static
void
test_insert2(void)
{
	printf("\tinserting another element...");
	elem = 8;
	vec_insert(&vec, &elem, 0);
	ok(vec[0] == 8);
	ok(vec[27] == 26);
	printf("done\n");
}

static
void
test_fun(void)
{
	printf("\ttesting fancy macros...");
	{
		printf("\n\t\ttrying a basic map...");
		elem = 0;
		vec_foreach (int *each, vec) ok(*each == elem++);
		printf("done\n");

		printf("\t\tretrying without a declaration...");
		elem = 0;
		int *each;
		vec_foreach (each, vec) ok(*each == elem++);
		printf("done\n");

		printf("\t\ttrying continue inside a map...");
		vec_foreach (int *i, vec) {
			if (*i != 8) continue;
			vec_delete(&vec, i - vec);
		}
		ok(len(vec) > 8);
		ok(vec[7] == 7);
		ok(vec[8] == 9);
		printf("done\n");

		printf("\t\ttrying to break out of a map...");
		vec_foreach (int *i, vec) {
			if (*i == 9) break;
			ok(*i < 9);
		}
		// FIXME: does not test nested maps
		printf("done\n");
	}
	printf("\tdone\n");
}

static
void
test_free(void)
{
	printf("\tfreeing the vectors...");
	vec_free(vec);
	vec_free(clone);
	printf("done\n");
}

static
void
test_join(void)
{
	printf("\tjoining two vectors...");
	vec_copy(&vec, clone);
	//vec_join(&vec, clone);
	ok(vec[4] == 4);
	printf("done\n");
}

static
void
test_large_insert(void)
{
	printf("\tadding a large numbers of elements...");
	for (elem = 0; elem < 1000; ++elem) {
		vec_append(&vec, &elem);
		ok(vec[elem] == elem);
	}
	printf("done\n");
}

static
void
test_delete(void)
{
	printf("\tdeleting an element...");
	ok(vec[432] == 431);
	vec_delete(&vec, 432);
	ok(vec[432] == 432);
	ok(vec[500] == 500);
	ok(vec[431] == 430);
	printf("done\n");
}

static
void
test_shift(void)
{
	printf("\tshifting a vector...");
	vec_shift(&vec, 5);
	ok(vec[0] == 4);
	ok(vec[45] == 49);
	printf("done\n");
}

static
void
test_slice(void)
{
	printf("\tslicing a vector...");
	vec_slice(&vec, 45, 100);
	for (i = 0; i < 100; ++i)
		ok(vec[i] == (int)i + 49);
	printf("done\n");
}

static
void
test_truncate(void)
{
	printf("\ttruncating vector...");
	vec_truncate(&vec, 0);
	ok(len(vec) == 0);
	printf("done\n");
}

int
main()
{
	printf("testing vec.c\n");
	test_alloc();
	test_insert();
	test_truncate();
	test_large_insert();
	test_clone();
	test_insert2();
	test_delete();
	test_shift();
	test_slice();
	test_join();
	test_concat();
	test_fun();
	test_free();
	printf("testing successful (vec.c)\n");
	return 0;
}
