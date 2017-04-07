#ifndef _test_
#define _test_

#define ok(test) do {				\
	if (test) break;			\
	printf("\n!!! test failed: %s (@%d)\n",	\
		#test, __LINE__);		\
	fflush(stdout);				\
	abort();				\
} while (0);

#endif
