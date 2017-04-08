#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test.h"
#include "../vec.h"

int
main()
{
	int i;
	int elem;
	Vector(int) *vec, *clone;

	printf("testing vec.c\n");

	printf("\tallocating a vector...");
	make_vector(vec);
	ok(len(vec) == 0);
	printf("done\n");

	printf("\tadding a few ints...");
	elem = 5;
	vec_append(vec, &elem);
	ok(len(vec) == 1);
	ok(vec[0] == 5);

	elem = 3;
	vec_insert(vec, &elem, 0);
	ok(len(vec) == 2);
	ok(vec[0] == 3);
	ok(vec[1] == 5);

	elem = 2345;
	vec_insert(vec, &elem, 1);
	ok(len(vec) == 3);
	ok(vec[0] == 3);
	ok(vec[1] == 2345);
	ok(vec[2] == 5);
	printf("done\n");

	printf("\ttruncating vector...");
	vec_truncate(vec, 0);
	ok(len(vec) == 0);
	printf("done\n");

	printf("\tadding a large numbers of elements...");
	for (elem = 0; elem < 1000; ++elem) {
		vec_append(vec, &elem);
		ok(vec[elem] == elem);
	}
	printf("done\n");

	printf("\tcloning vector...");
	clone = vec_clone(vec);
	ok(clone);
	ok(!memcmp(vec, clone, len(vec) * sizeof *vec));
	printf("done\n");

	printf("\tinserting another element...");
	elem = 8;
	vec_insert(vec, &elem, 0);
	ok(vec[0] == 8);
	ok(vec[27] == 26);
	printf("done\n");

	printf("\tremoving an element...");
	ok(vec[432] == 431);
	vec_delete(vec, 432);
	ok(vec[432] == 432);
	ok(vec[500] == 500);
	ok(vec[431] == 430);
	printf("done\n");

	printf("\tshifting a vector...");
	vec_shift(vec, 5);
	ok(vec[0] == 4);
	ok(vec[45] == 49);
	printf("done\n");

	printf("\tslicing a vector...");
	vec_slice(vec, 45, 100);
	for (i = 0; i < 100; ++i)
		ok(vec[i] == i + 49);
	printf("done\n");

	vec_free(vec);
	make_vector(vec);
	printf("\tjoining two vectors...");
	vec_join(vec,clone);
	ok(vec[4] == 4);
	printf("done\n");

	vec_free(vec);
	make_vector(vec);
	printf("\tconcatenating a vector...");
	vec_concat(vec, clone, len(clone));
	printf("done\n");

	printf("\ttesting functional macros...");
	{
		printf("\n\t\ttrying a basic map...");
		elem = 0;
		mapv (int *each, vec) ok(*each == elem++);
		printf("done\n");

		printf("\t\tretrying without a declaration...");
		elem = 0;
		int *each;
		mapv (each, vec) ok(*each == elem++);
		printf("done\n");

		printf("\t\ttrying continue inside a map...");
		mapv (int *i, vec) {
			if (*i != 8) continue;
			vec_delete(vec, i - vec);
		}
		ok(len(vec) > 8);
		ok(vec[7] == 7);
		ok(vec[8] == 9);
		printf("done\n");

		printf("\t\ttrying to break out of a map...");
		mapv (int *i, vec) {
			if (*i == 9) break;
			ok(*i != 9);
		}
		printf("done\n");
	}
	printf("\tdone\n");

	printf("\tfreeing the vectors...");
	vec_free(vec);
	vec_free(clone);
	printf("done\n");

	printf("testing successful (vec.c)\n");
	return 0;
}
