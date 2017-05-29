#include "unit.h"

int
main()
{
	size_t i = 0;

	unit_init();

	printf("testing %s\n", filename);

	for (i = 0; i < tests_len; ++i) {
		on_failure {
			cleanup();
			continue;
		}

		if (tests[i].setup) tests[i].setup();
		printf("\t%s...\n", tests[i].msg);
		if (tests[i].do_it) tests[i].do_it();

		cleanup();
	}

	printf("testing finished (%s) -- %zu failed tests\n", filename, total_failures);

	return 0;
}
