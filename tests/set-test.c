#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "test.h"
#include "../set.h"

int
main()
{
	init_test();
	set_t *t;
	char **reply = 0x0;
	char **arr = calloc(3, sizeof *arr);

	printf("testing set.c\n");
	
	printf("\tallocating a set...");
	t = set_alloc();
	ok(t);
	printf("done\n");

	printf("\tinserting a string...");
	ok(!set_add_string(t, "foobar"));
	printf("done\n");

	printf("\tconfirming the addition...");
	ok(set_contains_string(t, "foobar"));
	printf("done\n");

	printf("\tsearching the set...");
	ok(set_query_string(0x0, 0, t, "foo") == 1);

	set_query_string(&reply, 0, t, "f");

	ok(reply);
	ok(reply[0]);
	ok(!strcmp(reply[0], "foobar"));
	ok(!reply[1]);

	free(reply);
	reply = 0x0;

	printf("done\n");

	printf("\tremoving the string...");
	ok(!set_remove_string(t, "foobar"));
	printf("done\n");

	printf("\tconfirming the removal...");
	ok(!set_contains_string(t, "foobar"));
	ok(set_query_string(0x0, 0, t, "f") == 0);
	printf("done\n");

	printf("\tadding more strings...");
	ok(!set_add_string(t, "foo"));
	ok(!set_add_string(t, "bar"));
	ok(!set_add_string(t, "baz"));
	ok(!set_add_string(t, "quux"));
	printf("done\n");

	printf("\tchecking for the strings...");

	ok(set_contains_string(t, "foo"));
	ok(set_contains_string(t, "bar"));
	ok(set_contains_string(t, "baz"));
	ok(set_contains_string(t, "quux"));

	ok(set_query_string(0, 0, t, "") == 4);

	ok(set_query_string(&reply, 0, t, "b") == 2);
	ok(reply[0]);
	ok(reply[1]);
	ok(!reply[2]);
	ok(!strcmp(reply[0], "bar"));
	ok(!strcmp(reply[1], "baz"));
	free(reply);
	reply = 0x0;

	printf("done\n");

	printf("\tquerying with a fixed-size buffer");

	ok(set_query_string(&arr, 3, t, "") == 4);
	ok(arr[0]);
	ok(arr[1]);
	ok(!arr[2]);
	ok(!strcmp(arr[0], "bar"));
	ok(!strcmp(arr[1], "baz"));

	printf("done\n");

	printf("\ttesting new prefix check...");
	ok(set_prefix_string(t, "f"));
	ok(set_prefix_string(t, "q"));
	ok(set_prefix_string(t, "b"));
	printf("done\n");

	printf("\tfreeing the set...");
	set_free(t);
	printf("done\n");

	printf("test successful (set.c)\n");

	free(arr);

	return 0;
}
