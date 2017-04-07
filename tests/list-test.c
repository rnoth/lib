#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "test.h"
#include "../list.h"

int
main()
{
	List(int) *li, *tmp;
	int item;

	printf("testing list.c\n");

	printf("\tmaking a list...");

	make_list(li);
	ok(li);

	printf("done\n");

	printf("\tadding some items...");

	item = 4;
	li = list_cons(&item, li);
	ok(li);
	ok(car(li) == 4);
	ok(!cdr(li));

	item = 2;
	li = list_cons(&item, li);
	ok(li);
	ok(car(li) == 2);
	ok(cdr(li));
	tmp = cdr(li);
	ok(car(tmp) == 4);
	ok(!cdr(tmp));

	printf("done\n");

	printf("\tfreeing the list...");
	free_list(li);
	printf("done\n");

	printf("testing complete (list.c)\n");
}
