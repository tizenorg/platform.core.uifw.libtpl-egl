/**
 * @file main.cc
 * @brief TPL Test main entry point
 * 
 * In main() function, test configurations must be set by user.
 * Then, all tests will be run.
 */

#include "tpl-test.h"

#include "gtest/gtest.h"


int
main(int argc, char **argv) {
	// Setup configurations
	TPLTest::width = 720;
	TPLTest::height = 1280;
	TPLTest::depth = 24;

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

