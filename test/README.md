# ramfs/test

This directory is a 'test component' for use by the esp-idf build system.

The 'Unity' test-framework is used. A test-application (not included) runs
the tests by calling one of the following:
	
	unity_run_all_tests()
	unity_run_tests_by_tag("[ramfs]", false);

Note: unity_run-tests_by_tag simply calls strstr() internally, meaning that
it would also work with "ramfs", "fs]" or even "[", but not "[*fs]"