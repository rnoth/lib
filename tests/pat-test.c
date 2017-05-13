#include "../unit.h"
#include "../pat.h"
#include "../util.h"
#include "../vec.h"

char filename[] = "pat.c";

static void test_qmark(void);
static void test_star1(void);
static void test_star2(void);

struct test tests[] = {
	{ 0x0, test_qmark,    "testing the ? operator", },
	{ 0x0, 0x0,           "testing the * operator", },
	{ 0x0, test_star1,    "\tmatching over a simple text", },
	{ 0x0, test_star2,    "\ttesting a more complex pattern", },
};

size_t const tests_len = arr_len(tests);

void cleanup() {}

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
test_star1(void)
{
	struct pattern pat = {0};
	struct patmatch *mat = 0x0;

	expect(0, vec_ctor(mat));
	expect(0, pat_compile(&pat, "bleh*"));

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
}

void
test_star2(void)
{
	struct pattern pat = {0};
	struct patmatch *mat = 0x0;

	expect(0, vec_ctor(mat));
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
