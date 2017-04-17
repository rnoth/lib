#include "test.h"
#include "../pat.c"

int
main()
{
	pattern_t pat = {0};
	struct patmatch *mat = 0x0;

	vec_ctor(mat);
	patcomp(&pat, "foo?");
	patexec(&mat, "foo", &pat);
	ok(mat->off == 0);
	ok(mat->ext == 3);

	patexec(&mat, "blah blah foo", &pat);
	ok(mat->off == 10);
	ok(mat->ext == 3);

	patexec(&mat, "fe fi fo fum", &pat);
	ok(mat->off == 6);
	ok(mat->ext == 2);
	patfree(&pat);

	return 0;
}
