#include "unit.h"

#define callq(F, A) do if (F) F(A); while (0)

int
main()
{
	volatile size_t i = 0;

	unit_init();

	printf("testing %s\n", filename);

	for (i = 0; tests[i].msg; ++i) {
		on_failure {
			on_failure continue;
			callq(tests[i].cleanup, (tests[i].ctx));
			continue;
		}

		printf("\t%s...\n", tests[i].msg);
		callq(tests[i].setup, tests[i].ctx);
		callq(tests[i].test, tests[i].ctx);
		callq(tests[i].cleanup, tests[i].ctx);
	}

	printf("testing finished (%s) -- %zu failed tests\n", filename, total_failures);

	return 0;
}
