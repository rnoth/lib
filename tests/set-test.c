#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "test.h"
#include "../set.h"

int
main()
{
	Set *t;
	char **reply;

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
	reply = set_query_string(t, "foo");
	ok(reply);
	ok(len(reply) == 1);
	ok(reply[0]);
	ok(!strcmp(reply[0], "foobar"));
	vec_foreach(void **each, reply) vec_free(*each);
	vec_free(reply);
	printf("done\n");

	printf("\tremoving the string...");
	ok(!set_remove_string(t, "foobar"));
	printf("done\n");

	printf("\tconfirming the removal...");
	ok(!set_contains_string(t, "foobar"));
	reply = set_query_string(t, "foo");
	ok(!len(reply));
	vec_foreach(void **each, reply) vec_free(*each);
	vec_free(reply);
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

	reply = set_query_string(t, "f");
	ok(len(reply) == 1)
	ok(!strcmp(reply[0], "foo"));
	vec_foreach(void **each, reply) vec_free(*each);
	vec_free(reply);

	reply = set_query_string(t, "b");
	ok(len(reply) == 2)
	ok(reply[0]);
	ok(!strcmp(reply[0], "bar"));
	ok(!strcmp(reply[1], "baz"));
	vec_foreach(void **each, reply) vec_free(*each);
	vec_free(reply);
	printf("done\n");

	printf("\tadding a prefix conflict...");
	set_add_string(t, "barber");
	ok(set_contains_string(t, "barber"));
	printf("done\n");

	printf("\ttesting new prefix check...");
	ok(set_prefix_string(t, "f"));
	ok(set_prefix_string(t, "q"));
	printf("done\n");

	printf("\tfreeing the set...");
	set_free(t);
	printf("done\n");

	printf("test successful (set.c)\n");

	return 0;
}
