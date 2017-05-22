#include "../unit.h"
#include "../pat.h"
#include "../util.h"
#include "../vec.h"

char filename[] = "pat.c";

static void do_qmark(void);
static void do_star(void);
static void do_plus(void);
static void loop(void);

struct a {
	char *pat;
	struct b *accept;
	struct b *reject;
};

struct b {
	char *txt;
	size_t off;
	size_t ext;
};

struct test tests[] = {
	{ do_qmark, loop,      "testing the ? operator", },
	{ do_star,  loop,      "testing the * operator", },
	{ do_plus,  loop,      "testing the + operator", },
};

struct a qmark[] = {
	{ "foo?", (struct b[]) {
		{ "foo",          0, 3, },
                { "fo",           0, 2, },
	        { "fe fi fo fum", 6, 2, },
		{ 0x0 }, },

		(struct b[]) {
		{ "it's no here", },
	        { "ffffff", },
		{ 0x0 }, },
	},

	{ "b?a?r?", (struct b[]) {
		{ "bar",     0, 3, },
		{ "a",       0, 1, },
		{ "bababar", 0, 2, },
		{ "",        0, 0, },
		{ 0x0 }, },

		(struct b[]) {
		{ 0x0 }, },
	},

	{ 0x0 },
};

struct a star[] = {
	{ "bleh*", (struct b[]) {
		{ "ble",                  0,  3, },
		{ "bleh",                 0,  4, },
		{ "blehhhh",              0,  7, },
		{ "blah blah blehh blah", 10, 5, },
		{ 0x0 }, },
	
		(struct b[]) {
		{ "no" },
		{ "bl" },
		{ 0x0 }, },
	},

	{ "b*a*r*", (struct b[]) {
		{ "bar",   0, 3, },
		{ "bbaaa", 0, 5, },
		{ "r",     0, 1, },
		{ "",      0, 0, },
		{ 0x0 }, },

		(struct b[]) {
		{ 0x0 }, },
	},

	{ 0x0 },
};

struct a plus[] = {
	{ "hi+", (struct b[]) {
		{ "hi",    0, 2, },
		{ "hiiii", 0, 5, },
		{ 0x0}, },
	
		(struct b[]) {
		{ "h" },
		{ "" },
		{ 0x0 }, }
	},

	{ 0x0 },
};

size_t const tests_len = arr_len(tests);
struct a *cur;

void do_qmark(void) { cur = qmark; }
void do_star(void)  { cur = star; }
void do_plus(void)  { cur = plus; }

void cleanup() {}

void
loop(void)
{
	struct a *a = 0x0;
	struct b *b = 0x0;
	struct pattern pat[1] = {{0}};

	for (a = cur; a->pat; ++a) {
		expect(0, pat_compile(pat, a->pat));

		for (b = a->accept; b->txt; ++b) {
			expect(0, pat_match(pat, b->txt));
			expect(b->off, pat->mat->off);
			expect(b->ext, pat->mat->ext);
		}

		for (b = a->reject; b->txt; ++b)
			expect(-1, pat_match(pat, b->txt));

		pat_free(pat);
	}
}
