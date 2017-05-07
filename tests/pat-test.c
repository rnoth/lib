#include "test.h"
#include "../vec.h"
#include "../pat.h"

void
test_compfree(void)
{
	size_t i = 0;
	struct pattern pat[11] = {{0}};

	printf("\tcompiling some patterns...");
	fflush(stdout);
	// note: you should examine these in a debugger to ensure sanity
	ok(!pat_compile(pat, "foo"));
	ok(!pat_compile(pat + 1, "foo?"));
	ok(!pat_compile(pat + 2, "f?o?o?"));
	ok(!pat_compile(pat + 3, "f*o+oo+"));
	ok(!pat_compile(pat + 4, "(foo)"));
	ok(!pat_compile(pat + 5, "(f|o|o)"));
	ok(!pat_compile(pat + 6, "(f*|o|(o+))"));
	ok(!pat_compile(pat + 7, "^foo$"));
	ok(!pat_compile(pat + 8, "^fo(o$|oo)"));
	ok(!pat_compile(pat + 9, "\\^f\\*o\\(\\(\\$"));
	printf("done\n");

	printf("\tfreeing the patterns...");
	fflush(stdout);
	for (i = 0; i < sizeof pat / sizeof *pat; ++i)
		pat_free(pat + i);
	printf("done\n");
}

void
test_qmark(void)
{
	struct pattern pat = {0};
	struct patmatch *mat = 0x0;

	ok(!pat_compile(&pat, "foo?"));
	ok(!vec_ctor(mat));

	printf("\ttesting the ? operator...\n");
	printf("\t\tmatching over a minimal text...");
	fflush(stdout);

	ok(!pat_match(&mat, "foo", &pat));
	ok(mat->off == 0);
	ok(mat->ext == 3);
	printf("done\n");

	printf("\t\tmatching within a text...");
	fflush(stdout);
	ok(!pat_match(&mat, "blah blah foo", &pat));
	ok(mat->off == 10);
	ok(mat->ext == 3);

	ok(!pat_match(&mat, "fe fi fo fum", &pat));
	ok(mat->off == 6);
	ok(mat->ext == 2);
	printf("done\n");

	printf("\t\ttesting non-matches...");
	fflush(stdout);
	ok(pat_match(&mat, "it's not here", &pat) == -1);
	ok(pat_match(&mat, "ffffff", &pat) == -1);
	printf("done\n");
	printf("\tdone\n");

	pat_free(&pat);
	vec_free(mat);
}

void
test_star(void)
{
	struct pattern pat = {0};
	struct patmatch *mat = 0x0;

	ok(!vec_ctor(mat));
	ok(!pat_compile(&pat, "b*a*r*"));

	printf("\ttesting the * operator...\n");

	printf("\t\tmatching over a simple texts.");

	expect(0, pat_match(&mat, "bar", &pat));
	ok(mat->off == 0);
	ok(mat->ext == 3);

	expect(0, pat_match(&mat, "bbaaa", &pat));
	ok(mat->off == 0);
	ok(mat->ext == 5);

	expect(0, pat_match(&mat, "r", &pat));
	ok(mat->off == 0);
	ok(mat->ext == 1);

	expect(0, pat_match(&mat, "", &pat));
	ok(mat->off == 0);
	ok(mat->ext == 1);

	printf("done\n");
}

int
main()
{
	init_test();

	printf("testing pat.c\n");

	test_compfree();
	test_qmark();
	test_star();

	printf("testing complete (pat.c)\n");

	return 0;
}
