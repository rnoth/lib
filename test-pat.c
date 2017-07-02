#include <unit.h>
#include <pat.h>
#include <util.h>
#include <vec.h>

#define subm(...) ((struct patmatch[]){__VA_ARGS__, {-1, -1}})

static void test_free(void);
static void test_alter(void);
static void test_esc(void);
static void test_qmark(void);
static void test_star(void);
static void test_sub(void);
static void test_plain(void);
static void test_plus(void);
static void test_dot(void);
static void test_match(void);

struct a {
	char *pat;
	struct b *accept;
	struct b *reject;
};

struct b {
	char *txt;
	struct patmatch *sub;
};

char unit_filename[] = "pat.c";
struct test unit_tests[] = {
	{ "testing plaintext matching",  test_plain, test_match, test_free, },
	{ "testing the \\ escapes",      test_esc,   test_match, test_free, },
	{ "testing the ? operator",      test_qmark, test_match, test_free, },
	{ "testing the * operator",      test_star,  test_match, test_free, },
	{ "testing the + operator",      test_plus,  test_match, test_free, },
	{ "testing the | operator",      test_alter, test_match, test_free, },
	{ "testing the () submatches",   test_sub,   test_match, test_free, },
	{ "testing the . metacharacter", test_dot,   test_match, test_free, },
	{ 0x0 },
};

struct a plain[] = {
	{ "abc", (struct b[]) {
		{ "abc", subm({0, 3}) },
		{ "   abc   ", subm({3, 3}) },
		{ 0x0 }, },
	},
	{ 0x0 }
};

struct a esc[] = {
	{ "a\\*", (struct b[]) { 
		{ "a*", subm({0, 2}) },
		{ 0x0 } },

		(struct b[]) {
		{ "aaa" },
		{ 0x0 } },
	},

	{ "\\\\", (struct b[]) {
		{ "\\", subm({0, 1}) },
		{ 0x0 } },
	},

	{ "\\++", (struct b[]) {
		{ "+++", subm({0, 3}) },
		{ 0x0 } },
	},

	{ 0x0 }
};

struct a qmark[] = {
	{ "foo?", (struct b[]) {
		{ "foo",          subm({0, 3}) },
                { "fo",           subm({0, 2}) },
	        { "fe fi fo fum", subm({6, 2}) },
		{ 0x0 }, },

		(struct b[]) {
		{ "it's not here", },
	        { "ffffff", },
		{ 0x0 }, },
	},

	{ "b?a?r?", (struct b[]) {
		{ "bar",     subm({0, 3}) },
		{ "a",       subm({0, 1}) },
		{ "bababar", subm({0, 2}) },
		{ "",        subm({0, 0}) },
		{ 0x0 }, },
	},

	{ 0x0 },
};

struct a star[] = {
	{ "bleh*", (struct b[]) {
		{ "ble",                  subm({ 0, 3}) },
		{ "bleh",                 subm({ 0, 4}) },
		{ "blehhhh",              subm({ 0, 7}) },
		{ "blah blah blehh blah", subm({10, 5}) },
		{ 0x0 }, },
	
		(struct b[]) {
		{ "no" },
		{ "bl" },
		{ 0x0 }, },
	},

	{ "b*a*r*", (struct b[]) {
		{ "bar",   subm({0, 3}) },
		{ "bbaaa", subm({0, 5}) },
		{ "r",     subm({0, 1}) },
		{ "",      subm({0, 0}) },
		{ 0x0 }, },
	},

	{ 0x0 },
};

struct a plus[] = {
	{ "hi+", (struct b[]) {
		{ "hi",    subm({0, 2}) },
		{ "hiiii", subm({0, 5}) },
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
		{ "a",  subm({0, 1}) },
		{ "b",  subm({0, 1}) },
		{ "ab", subm({0, 1}) },
		{ 0x0 }, }, 

		(struct b[]) {
		{ "", },
		{ 0x0 }, }
	},

	{ "abc|def", (struct b[]) {
		{ "abc",    subm({0, 3}) },
		{ "def",    subm({0, 3}) },
		{ "defabc", subm({0, 3}) },
		{ 0x0 }, },
	},

	{ "ab+c|de*f?", (struct b[]) {
		{ "abc",      subm({0, 3}) },
		{ "def",      subm({0, 3}) },
		{ "abbbbbbc", subm({0, 8}) },
		{ "d",        subm({0, 1}) },
		{ "dee",      subm({0, 3}) },
		{ "Yeah, definitetly",
	                      subm({6, 3}) },
		{ 0x0 }, },
	},
	
	{ 0x0 },
};

struct a sub[] = {
	{ "abc(def)", (struct b[]) {
		{ "abcdef",    subm({0, 6}, {3, 3}) },
		{ "123abcdef", subm({3, 6}, {6, 3}) },
		{ 0x0 } },
	},

	{ "a(b)c", (struct b[]) {
		{ "abc", subm({0, 3}, {1, 1}) },
		{ 0x0 }, },
	},

	{ "a(b(c)d)e", (struct b[]) {
		{ "abcde", subm({0, 5}, {1, 3}, {2, 1}) },
		{ 0x0 }, },
	},

	{ "ab*(c+d?)ff", (struct b[]) {
		{ "abbbcccdff", subm({0, 10}, {4, 4}) },
		{ "acff",       subm({0,  4}, {1, 1}) },
		{ 0x0 } },
	},

	{ "abc(def|ghi)jkl", (struct b[]) {
		{ "abcdefjkl", subm({0, 9}, {3,3})},
		{ "abcghijkl", subm({0, 9}, {3,3})},
		{ 0x0 } },
	},

	{ "abc(def)+", (struct b[]) {
		{ "abcdefdefdef", subm({0,12}, {3,3}, {6,3}, {9,3})},
		{ 0x0 } },
	},

	{ "a(b+)b", (struct b[]) {
		{ "abbb", subm({0, 4},{1,2})},
		{ 0x0 } },
	},

	{ "(a*)bc", (struct b[]) {
		{ "bc", subm({0, 2}, {0,0})},
		{ 0x0 } }
	},

	{ 0x0 },
};

struct a dot[] = {
	{ ".oo", (struct b[]) {
		{ "foo", subm({0, 3}) },
		{ "boo", subm({0, 3}) },
		{ 0x0 } },

		(struct b[]) {
		{ "\noo" },
		{ 0x0 } },
	},

	{ "(..)|(.)(.)", (struct b[]) {
		{ "ab", subm({0,2},{0,2}) },
		{ 0x0 } }
	},

	{ "(.)?(a)?", (struct b[]) {
		{ "a", subm({0, 1}, {0,1}) },
		{ 0x0 } }
	},

	{ "a*(.*)", (struct b[]) {
		{ "aaabbb", subm({0, 6}, {0, 6}) },
		{ 0x0 } },
	},

	{ 0x0 },
};

struct a *cur;

struct pattern pat[1];

void test_alter(void) { cur = alter; }
void test_qmark(void) { cur = qmark; }
void test_star(void)  { cur = star; }
void test_sub(void)   { cur = sub; }
void test_plain(void) { cur = plain; }
void test_plus(void)  { cur = plus; }
void test_esc(void)   { cur = esc; }
void test_dot(void)   { cur = dot; }

void
test_free()
{
	pat_free(pat);
	memset(pat, 0, sizeof *pat);
}

void
test_match(void)
{
	struct a *a = 0x0;
	struct b *b = 0x0;
	size_t i;

	for (a = cur; a->pat; ++a) {
		try(pat_free(pat));
		expectf(0, pat_compile(pat, a->pat), "couldn't compile: '%s'", a->pat);

		for (b = a->accept; b && b->txt; ++b) {
			expectf(0, pat_execute(pat, b->txt),
			        "couldn't match '%s' over '%s'", a->pat, b->txt);

			for (i = 0; i < pat->nmat; ++i) {
				expectf(b->sub[i].off, pat->mat[i].off,
				        "bad submatch offset (\\%zd) for '%s' on '%s'",
				         i, a->pat, b->txt);

				expectf(b->sub[i].ext, pat->mat[i].ext,
				        "bad submatch extent (\\%zd) for '%s' on '%s'",
				         i, a->pat, b->txt);
			}
		}

		for (b = a->reject; b && b->txt; ++b) {
			expect(-1, pat_execute(pat, b->txt));
		}
	}
}
