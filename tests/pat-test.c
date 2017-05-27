#include "../unit.h"
#include "../pat.h"
#include "../util.h"
#include "../vec.h"

char filename[] = "pat.c";

static void do_alter(void);
static void do_esc(void);
static void do_qmark(void);
static void do_star(void);
static void do_sub(void);
static void do_plus(void);
static void do_dot(void);
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
	struct patmatch *sub;
};

struct test tests[] = {
	{ do_qmark, loop,      "testing the ? operator", },
	{ do_star,  loop,      "testing the * operator", },
	{ do_plus,  loop,      "testing the + operator", },
	{ do_alter, loop,      "testing the | operator", },
	{ do_esc,   loop,      "testing the \\ escapes", },
	{ do_sub,   loop,      "testing the () submatches", },
	{ do_dot,   loop,      "testing the . metacharacter", },
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
	},

	{ 0x0 },
};

struct a plus[] = {
	{ "hi+", (struct b[]) {
		{ "hi",    0, 2, },
		{ "hiiii", 0, 5, },
		{ 0x0 }, },
	
		(struct b[]) {
		{ "h" },
		{ "" },
		{ 0x0 }, }
	},

	{ 0x0 },
};

struct a alter[] = {
	{ "a|b", (struct b[]) {
		{ "a",  0, 1, },
		{ "b",  0, 1, },
		{ "ab", 0, 1 },
		{ 0x0 }, }, 

		(struct b[]) {
		{ "", },
		{ 0x0 }, }
	},

	{ "abc|def", (struct b[]) {
		{ "abc",    0, 3, },
		{ "def",    0, 3, },
		{ "defabc", 0, 3, },
		{ 0x0 }, },
	},

	{ "ab+c|de*f?", (struct b[]) {
		{ "abc", 0, 3, },
		{ "def", 0, 3, },
		{ "abbbbbbc", 0, 8, },
		{ "d",        0, 1, },
		{ "dee",      0, 3, },
		{ "Yeah, definitetly",
	                      6, 3, },
		{ 0x0 }, },
	},
	
	{ 0x0 },
};

struct a esc[] = {
	{ "a\\*", (struct b[]) { 
		{ "a*", 0, 2, },
		{ 0x0 } },

		(struct b[]) {
		{ "aaa" },
		{ 0x0 } },
	},

	{ "\\\\", (struct b[]) {
		{ "\\", 0, 1, },
		{ 0x0 } },
	},

	{ 0x0 }
};

struct a sub[] = {
	{ "abc(def)", (struct b[]) {
		{ "abcdef",   0, 6, (struct patmatch[]) {{ 3, 3 }, {-1}} },
		{ "123abcdef", 3, 6, (struct patmatch[]) {{ 6, 3 }, {-1}} },
		{ 0x0 } },
	},

	{ "a(b)c", (struct b[]) {
		{ "abc", 0, 3, (struct patmatch[]) {{ 1, 1 }, {-1}} },
		{ 0x0 }, },
	},

	{ "a(b(c)d)e", (struct b[]) {
		{ "abcde", 0, 5, (struct patmatch[]) {
			{ 1, 3, },
			{ 2, 1, },
			{ -1 },
		} },
		{ 0x0 }, },
	},

	{ "ab*(c+d?)ff", (struct b[]) {
		{ "abbbcccdff", 0, 10, (struct patmatch[]) { { 4, 4 }, { -1 } } },
		{ "acff",       0, 4,  (struct patmatch[]) { { 1, 1 }, { -1 } } },
		{ 0x0 } },
	},

	{ "abc(def|ghi)jkl", (struct b[]) {
		{ "abcdefjkl", 0, 9, (struct patmatch[]) {{3,3},{-1}}},
		{ "abcghijkl", 0, 9, (struct patmatch[]) {{3,3},{-1}}},
		{ 0x0 } },
	},

	{ "abc(def)+", (struct b[]) {
		{ "abcdefdefdef", 0, 12, (struct patmatch[]) {{3,3}, {6,3}, {9,3},{-1}}},
		{ 0x0 } },
	},

	// posix
	{ "a(b+)b", (struct b[]) {
		{ "abbb", 0, 4, (struct patmatch[]) {{1,2},{-1}}},
		{ 0x0 } },
	},

	{ "(a*)bc", (struct b[]) {
		{ "bc", 0, 2, (struct patmatch[]) {{0,0}, {-1}}},
		{ 0x0 } }
	},

	{ "(..)|(.)(.)", (struct b[]) {
		{ "ab", 0, 2, (struct patmatch[]) {{0,2}, {-1}} },
		{ 0x0 } }
	},

	{ "(.)?(a)?", (struct b[]) {
		{ "a", 0, 1, (struct patmatch[]) {{0,1}, {-1}} },
		{ 0x0 } }
	},

	{ 0x0 },
};

struct a dot[] = {
	{ ".oo", (struct b[]) {
		{ "foo", 0, 3 },
		{ "boo", 0, 3 },
		{ 0x0 } },

		(struct b[]) {
		{ "\noo" },
		{ 0x0 } },
	},

	{ 0x0 },
};

size_t const tests_len = arr_len(tests);
struct a *cur;

void do_alter(void) { cur = alter; }
void do_qmark(void) { cur = qmark; }
void do_star(void)  { cur = star; }
void do_sub(void)   { cur = sub; }
void do_plus(void)  { cur = plus; }
void do_esc(void)   { cur = esc; }
void do_dot(void)   { cur = dot; }

void cleanup() {}

void
loop(void)
{
	struct pattern pat[1] = {{0}};
	struct a *a = 0x0;
	struct b *b = 0x0;
	size_t i;

	for (a = cur; a->pat; ++a) {
		expect(0, pat_compile(pat, a->pat));

		for (b = a->accept; b && b->txt; ++b) {
			expect(0, pat_execute(pat, b->txt));
			expect(b->off, pat->mat->off);
			expect(b->ext, pat->mat->ext);

			if (b->sub) {
				for (i = 1; b->sub[i-1].off < (size_t)-1; ++i) {
					expect(b->sub[i-1].off, pat->mat[i].off);
					expect(b->sub[i-1].ext, pat->mat[i].ext);
				}

				expect(-1, pat->mat[i].off);
				expect(-1, pat->mat[i].ext);
			}
		}

		for (b = a->reject; b && b->txt; ++b)
			expect(-1, pat_execute(pat, b->txt));

		pat_free(pat);
	}
}
