#include "../unit.h"
#include "../pat.h"
#include "../util.h"
#include "../vec.h"

char filename[] = "pat.c";

// TODO: there is a simpler way to implement these tests

static void test_qmark1(void);
static void test_qmark2(void);
static void test_qmark3(void);
static void test_star1(void);
static void test_star2(void);
static void test_star3(void);

struct test tests[] = {
	{ 0x0, 0x0,           "testing the ? operator", },
	{ 0x0, test_qmark1,   "\tmatching over a minimal text", },
	{ 0x0, test_qmark2,   "\tmatching within a text", },
	{ 0x0, test_qmark3,   "\ttesting non-matches", },
	{ 0x0, 0x0,           "testing the * operator", },
	{ 0x0, test_star1,    "\tmatching over a simple text", },
	{ 0x0, test_star2,    "\ttesting a more complex pattern", },
	{ 0x0, test_star3,    "\ttesting non-matches" },
};

size_t const tests_len = arr_len(tests);

void cleanup() {}

void
test_qmark1(void)
{
	struct pattern pat = {0};
	struct patmatch *mat = 0x0;

	ok(!vec_ctor(mat));
	ok(!pat_compile(&pat, "foo?"));

	expect(0, pat_match(&mat, "foo", &pat));
	expect(0, mat->off);
	expect(3, mat->ext);

	expect(0, pat_match(&mat, "fo", &pat));
	expect(0, mat->off);
	expect(2, mat->ext);
}

void
test_qmark2(void)
{
	struct pattern pat = {0};
	struct patmatch *mat = 0x0;

	ok(!vec_ctor(mat));
	ok(!pat_compile(&pat, "foo?"));

	ok(!pat_match(&mat, "blah blah foo", &pat));
	ok(mat->off == 10);
	ok(mat->ext == 3);

	ok(!pat_match(&mat, "fe fi fo fum", &pat));

	ok(mat->off == 6);
	ok(mat->ext == 2);
}

void
test_qmark3(void)
{
	struct pattern pat = {0};
	struct patmatch *mat = 0x0;

	ok(!vec_ctor(mat));
	ok(!pat_compile(&pat, "foo?"));

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
	expect(0, mat->ext);
}

void
test_star3(void)
{
	struct pattern pat = {0};
	struct patmatch *mat = 0x0;

	expect(0, vec_ctor(mat));

	expect(0, pat_compile(&pat, "qu+u?x$"));

	expect(-1, pat_match(&mat, "no", &pat));
	expect(-1, pat_match(&mat, "quuuuu", &pat));
	expect(-1, pat_match(&mat, "", &pat));
}
