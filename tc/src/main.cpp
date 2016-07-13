/**
 * @file main.cpp
 * @brief TPL Test main entry point
 * 
 * In main() function, test configurations must be set by user.
 * Then, all tests will be run.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gtest/gtest.h"
#include "tpl-test.h"


struct Config __tpl_test_parse_arguments(int argc, char **argv) {
	struct Config config;
	
	// Default configurations
	config.width = 720;
	config.height = 1280;
	config.depth = 24;

	// Disable getopt error message
	opterr = 0;

	int  opt;
	bool exit = false;
	while (!exit && (opt = getopt(argc, argv, "w:h:d:")) != -1)
		switch (opt) {
			case 'w':
				config.width = atoi(optarg);
				break;
			case 'h':
				config.height = atoi(optarg);
				break;
			case 'd':
				config.depth = atoi(optarg);
				break;
			case '?':
				optind --;
				exit = true;
				break;
		}

	argc -= optind;
	argv += optind;

	return config;
}


int
main(int argc, char **argv) {
	
	// Setup configurations
	struct Config config = __tpl_test_parse_arguments(argc, argv);
	TPLTest::config = &config;
	
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

