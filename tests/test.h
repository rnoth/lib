#ifndef _test_
#define _test_
#include <signal.h>
#include <stdio.h>

#define ok(test) do {				\
	if (test) break;			\
	printf("\n!!! test failed: %s (@%d)\n",	\
		#test, __LINE__);		\
	fflush(stdout);				\
	raise(SIGABRT);				\
} while (0);

#endif
