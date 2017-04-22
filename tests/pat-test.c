#include "test.h"
#include "../pat.h"

void
test_compfree(void)
{
	size_t i = 0;
	pattern_t pat[11] = {{0}};

	printf("\tcompiling some patterns...");
	fflush(stdout);
	// note: you should examine these in a debugger to ensure sanity
	ok(!patcomp(pat, "foo"));
	ok(!patcomp(pat + 1, "foo?"));
	ok(!patcomp(pat + 2, "f?o?o?"));
	ok(!patcomp(pat + 3, "f*o+oo+"));
	ok(!patcomp(pat + 4, "(foo)"));
	ok(!patcomp(pat + 5, "(f|o|o)"));
	ok(!patcomp(pat + 6, "(f*|o|(o+))"));
	ok(!patcomp(pat + 7, "^foo$"));
	ok(!patcomp(pat + 8, "^fo(o$|oo)"));
	ok(!patcomp(pat + 9, "\\^f\\*o\\(\\(\\$"));
	printf("done\n");

	printf("\tfreeing the patterns...");
	fflush(stdout);
	for (i = 0; i < sizeof pat / sizeof *pat; ++i)
		patfree(pat + i);
	printf("done\n");
}

void
test_qmark(void)
{
	pattern_t pat = {0};
	patmatch_t *mat = 0x0;

	ok(!patcomp(&pat, "foo?"));
	ok(!vec_ctor(mat));

	printf("\ttesting the ? operator...\n");
	printf("\t\tmatching over a minimal text...");
	fflush(stdout);

	ok(!patexec(&mat, "foo", &pat));
	ok(mat->off == 0);
	ok(mat->ext == 3);
	printf("done\n");

	printf("\t\tmatching within a text...");
	fflush(stdout);
	ok(!patexec(&mat, "blah blah foo", &pat));
	ok(mat->off == 10);
	ok(mat->ext == 3);

	ok(!patexec(&mat, "fe fi fo fum", &pat));
	ok(mat->off == 6);
	ok(mat->ext == 2);
	printf("done\n");

	printf("\t\ttesting non-matches...");
	fflush(stdout);
	ok(patexec(&mat, "it's not here", &pat) == -1);
	ok(patexec(&mat, "ffffff", &pat) == -1);
	printf("done\n");
	printf("\tdone\n");

	patfree(&pat);
	vec_free(mat);
}

int
main()
{
	printf("testing pat.c\n");

	test_compfree();
	test_qmark();

	printf("testing complete (pat.c)\n");

	return 0;
}
