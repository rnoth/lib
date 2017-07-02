#include <unit.h>
#include <pat-exec.c>

char unit_filename[] = "pat-exec.c";

void setup_plain(char *);
void cleanup();
void test_match();

struct test unit_tests[] = {
	{ "doing nothing", setup_plain, test_match, cleanup, "abc" },
	{ 0x0 }
};

struct ins plain[] = {
	{ do_char, 'a' },
	{ do_char, 'b' },
	{ do_char, 'c' },
	{ do_halt },
};

struct context ctx[1];
struct pattern pat[1];

void
setup_plain(char *txt)
{
	ctx->str = txt;
	ctx->len = strlen(txt);
	pat->prog = plain;
	try(ctx_init(ctx, pat));
}

void
cleanup()
{
	try(ctx_fini(ctx));
}

void
test_match()
{
	expect(0, pat_exec(ctx));
	expect(0, pat_fini(ctx));
}
