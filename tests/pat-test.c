#include "../unit.h"
#include "../pat.h"
#include "../util.h"
#include "../vec.h"


char filename[] = "pat.c";

// TODO: better organization

void test_compfree(void);
void test_qmark(void);
void test_star(void);

struct test tests[] = {
	{ 0x0, test_compfree, "compiling some patterns", },
	{ 0x0, test_qmark,    "testing the ? operator", },
	{ 0x0, test_star,     "testing the * operator", },
};

size_t const tests_len = arr_len(tests);

void cleanup() {}

void
test_compfree(void)
{
	size_t i = 0;
	struct pattern pat[11] = {{0}};

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

	printf("\tfreeing the patterns...\n");
	for (i = 0; i < sizeof pat / sizeof *pat; ++i)
		pat_free(pat + i);
}

void
test_qmark(void)
{
	struct pattern pat = {0};
	struct patmatch *mat = 0x0;

	ok(!pat_compile(&pat, "foo?"));
	ok(!vec_ctor(mat));

	printf("\t\tmatching over a minimal text...\n");

	ok(!pat_match(&mat, "foo", &pat));
	ok(mat->off == 0);
	ok(mat->ext == 3);

	printf("\t\tmatching within a text...\n");

	ok(!pat_match(&mat, "blah blah foo", &pat));
	ok(mat->off == 10);
	ok(mat->ext == 3);

	ok(!pat_match(&mat, "fe fi fo fum", &pat));

	ok(mat->off == 6);
	ok(mat->ext == 2);

	printf("\t\ttesting non-matches...\n");
	ok(pat_match(&mat, "it's not here", &pat) == -1);
	ok(pat_match(&mat, "ffffff", &pat) == -1);

	pat_free(&pat);
	vec_free(mat);
}

void
test_star(void)
{
	struct pattern pat = {0};
	struct patmatch *mat = 0x0;

	ok(!vec_ctor(mat));
	expect(0, pat_compile(&pat, "bleh*"));

	printf("\t\tmatching over a simple text...\n");

	expect(0, pat_match(&mat, "ble", &pat));
	expect(0, mat->off);
	expect(3, mat->ext);

	expect(0, pat_match(&mat, "bleh", &pat));
	expect(0, mat->off);
	expect(4, mat->ext);

	expect(0, pat_match(&mat, "blehhhh", &pat));
	expect(0, mat->off);
	expect(7, mat->ext);

	expect(0, pat_match(&mat, "blah blah blehh blah", &pat));
	expect(10, mat->off);
	expect(5,  mat->ext);

	printf("testing a more complex patterns...\n");
	ok(!pat_compile(&pat, "b*a*r*"));

	expect(0, pat_match(&mat, "bar", &pat));
	expect(0, mat->off);
	expect(3, mat->ext);

	expect(0, pat_match(&mat, "bbaaa", &pat));
	expect(0, mat->off);
	expect(5, mat->ext);

	expect(0, pat_match(&mat, "r", &pat));
	expect(0, mat->off);
	expect(1, mat->ext);

	expect(0, pat_match(&mat, "", &pat));
	expect(0, mat->off);
	expect(1, mat->ext);

	printf("done\n");
}
