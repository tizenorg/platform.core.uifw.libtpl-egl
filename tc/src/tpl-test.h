#ifndef TPL_TEST_H
#define TPL_TEST_H

/**
 * @file tpl-test.h
 * @brief TPLBackendBase class and TPLTestBase class declared in this file
 *
 */ 


#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>

#include <gbm.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <tpl.h>
#ifdef __cplusplus
}
#endif

#include "gtest/gtest.h"


typedef struct {
	int width;
	int height;
	int depth;
} Config;


/**
 * A base class contains struct for tpl APIs.
 *
 * Member variables in this class are used as argument of tpl APIs.
 * However, these variables are initialized in different ways up to backends.
 * Therefore, this class must be inherited and init and fini functions must be
 * fulfilled with backend implementation.
 */
class TPLBackendBase {
public:
	tpl_display_t *tpl_display;
	tpl_surface_t *tpl_surface;
	tbm_surface_h  tbm_surface;

	tpl_handle_t *display_handle;
	tpl_handle_t *surface_handle;

	virtual void tpl_backend_initialize(Config *config) = 0;
	virtual void tpl_backend_finalize(Config *config) = 0;
};


class TPLTestBase : public testing::Test {
public:
	static TPLBackendBase *backend;
	static Config config;

protected:
	// Called before first test in test case
	// Calls backend initialize function
	static void SetUpTestCase();

	// Called before every test
	virtual void SetUp() { /*  */ };

	// Called after every test
	virtual void TearDown() { /*  */ };

	// Called after last test in test case
	// Calls backend finalize function
	static void TearDownTestCase();
};


#endif
