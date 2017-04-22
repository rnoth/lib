#include "test.h"
#include "../pat.h"

int
main()
{
	pattern_t pat = {0};
	struct patmatch *mat = 0x0;

	printf("testing pat.c\n");
	fflush(stdout);
	ok(!vec_ctor(mat));

	printf("\tcompiling a pattern...");
	fflush(stdout);
	ok(!patcomp(&pat, "foo?"));
	printf("done\n");

	printf("\tmatching over a minimal text...");
	fflush(stdout);
	ok(!patexec(&mat, "foo", &pat));
	ok(mat->off == 0);
	ok(mat->ext == 3);
	printf("done\n");

	printf("\tmatching within a text...");
	fflush(stdout);
	ok(!patexec(&mat, "blah blah foo", &pat));
	ok(mat->off == 10);
	ok(mat->ext == 3);
	printf("done\n");

	printf("\ttesting the ? operator...");
	fflush(stdout);
	ok(!patexec(&mat, "fe fi fo fum", &pat));
	ok(mat->off == 6);
	ok(mat->ext == 2);
	printf("done\n");

	printf("\tfreeing a pattern...");
	fflush(stdout);
	patfree(&pat);
	vec_free(mat);
	printf("done\n");

	printf("testing complete (pat.c)\n");

	return 0;
}
