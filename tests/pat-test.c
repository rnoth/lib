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
	patfree(&pat);

	return 0;
}
