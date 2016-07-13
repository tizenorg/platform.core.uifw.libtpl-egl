/**
 * @file main.cpp
 * @brief TPL Test main entry point
 * 
 * In main() function, test configurations are set from command line arguments.
 * Then, all tests will be run.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "gtest/gtest.h"
#include "tpl-test.h"
#include "tpl-test-wayland.h"


Config
__tpl_test_parse_arguments(int argc, char **argv) {
	Config config;

	// Default configurations
	config.width = 720;
	config.height = 1280;
	config.depth = 24;

	// Check option
	int opt_width = 0;
	int opt_height = 0;
	int opt_depth = 0;

	struct option longopts[] = {
		{"config.width", required_argument, &opt_width, 1},
		{"config.height", required_argument, &opt_height, 1},
		{"config.depth", optional_argument, &opt_depth, 1},
		{NULL, 0, NULL, 0}
	};

	int opt;
	while ((opt = getopt_long_only(argc, argv, "", longopts, NULL)) != -1)
		if (opt_width == 1) {
			printf("width set: %s\n", optarg);
			config.width = atoi(optarg); 
			opt_width = 0;
		} else if (opt_height == 1) {
			printf("height set: %s\n", optarg);
			config.height = atoi(optarg); 
			opt_height = 0;
		} else if (opt_depth == 1) {
			printf("depth set: %s\n", optarg);
			config.depth = atoi(optarg); 
			opt_depth = 0;
		} else {
			break;
		}

	return config;
}



int
main(int argc, char **argv) {

	// Setup configurations
	TPLTestBase::config = __tpl_test_parse_arguments(argc, argv);
	TPLTestBase::backend = new TPLWayland();

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

