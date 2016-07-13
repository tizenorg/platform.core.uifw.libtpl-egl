#ifndef TPL_TEST_H
#define TPL_TEST_H

/**
 * @file tpl-test.h
 * @brief TPL Test class declared in this file
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

struct Display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
};

struct Window {
	struct Display *display;
	struct wl_egl_window *native;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
};

class TPLTest : public ::testing::Test {
public:
	struct Display wl_disp;
	struct Window wl_win;

	tpl_display_t *tpl_display;
	tpl_surface_t *tpl_surface;
	tbm_surface_h  tbm_surface;

	// Configurations
	static int width;
	static int height;
	static int depth;

protected:
	// Called before first test in test case
	static void SetUpTestCase() {}
	
	// Called before every test
	virtual void SetUp();

	// Called after every test
	virtual void TearDown();

	// Called after last test in test case
	static void TearDownTestCase() {}

private:
	void __tpl_test_initialize();
	void __tpl_test_finalize();
};


#endif
