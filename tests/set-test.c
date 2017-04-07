#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "test.h"
#include "../list.h"
#include "../set.h"

int
main()
{
	Set *A;
	Vector(char *) *reply;

	printf("testing set.c\n");
	
	printf("\tallocating a set...");
	A = set_alloc();
	ok(A);
	printf("done\n");

	printf("\tinserting a string...");
	set_adds(A, "foobar");
	printf("done\n");

	printf("\tconfirming the addition...");
	ok(set_membs(A, "foobar"));
	printf("done\n");

	printf("\tsearching the set...");
	reply = set_querys(A, "foo");
	ok(reply);
	ok(len(reply) == 1);
	ok(arr(reply)[0]);
	ok(!strcmp(arr(reply)[0], "foobar"));
	mapv(void **each, reply) free(*each);
	free(reply);
	printf("done\n");

	printf("\tremoving the string...");
	set_rms(A, "foobar");
	printf("done\n");

	printf("\tconfirming the removal...");
	ok(!set_membs(A, "foobar"));
	reply = set_querys(A, "foo");
	ok(!len(reply));
	mapv(void **each, reply) free(*each);
	free(reply);
	printf("done\n");

	printf("\tadding more strings...");
	set_adds(A, "foo");
	set_adds(A, "bar");
	set_adds(A, "baz");
	set_adds(A, "quux");
	printf("done\n");

	printf("\tchecking for the strings...");
	ok(set_membs(A, "foo"));
	ok(set_membs(A, "bar"));
	ok(set_membs(A, "baz"));
	ok(set_membs(A, "quux"));

	reply = set_querys(A, "f");
	ok(len(reply))
	ok(!strcmp(arr(reply)[0], "foo"));
	mapv(void **each, reply) free(each);
	free(reply);

	reply = set_querys(A, "b");
	ok(len(reply) == 2)
	ok(arr(reply)[0]);
	ok(!strcmp(arr(reply)[0], "bar"));
	ok(!strcmp(arr(reply)[1], "baz"));
	mapv(void **each, reply) free(*each);
	free(reply);
	printf("done\n");

	printf("\tadding a prefix conflict...");
	set_adds(A, "barber");
	ok(set_membs(A, "barber"));
	printf("done\n");

	printf("\ttesting new prefix check...");
	ok(set_prefixs(A, "f"));
	ok(set_prefixs(A, "q"));
	printf("done\n");

	printf("\tfreeing the set...");
	set_free(A);
	printf("done\n");

	printf("test successful (set.c)\n");

	return 0;
}
