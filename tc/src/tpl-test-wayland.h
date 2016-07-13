#ifndef TPL_TEST_WAYLAND_H
#define TPL_TEST_WAYLAND_H

/**
 * @file tpl-test-wayland.h
 * @brief TPL Wayland Backend class declared in this file
 *
 */ 

#include "gtest/gtest.h"

#include "tpl-test.h"


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


class TPLWayland : public TPLBackendBase {
public:
	void tpl_backend_initialize(Config *config);
	void tpl_backend_finalize(Config *config);

private:
	struct Display wl_disp;
	struct Window wl_win;
};


#endif
