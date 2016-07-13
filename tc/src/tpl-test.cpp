/**
 * @file tpl-test.cpp
 * @brief TPL Test class functions are defined in this file
 *
 */ 

#include "tpl-test.h"

#include "gtest/gtest.h"

// Configuration initialization
int TPLTest::width  = 0;
int TPLTest::height = 0;
int TPLTest::depth  = 0;
struct Config *TPLTest::config = (struct Config *)NULL;

// Wayland related
static void
registry_handle_global(void *data, struct wl_registry *registry, uint32_t name,
					   const char *interface, uint32_t version)
{
	struct Display *d = (struct Display *) data;

	if (strcmp(interface, "wl_compositor") == 0)
	{
		d->compositor = (struct wl_compositor *) wl_registry_bind(registry,
					   name, &wl_compositor_interface, 1);
	}
	if (strcmp(interface, "wl_shell") == 0)
	{
		d->shell = (struct wl_shell *) wl_registry_bind(registry, name,
				  &wl_shell_interface, 1);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
							  uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};


static void
shell_ping(void *data, struct wl_shell_surface *shell_surface,
		   uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void
shell_configure(void *data, struct wl_shell_surface *shell_surface,
				uint32_t edges, int32_t width, int32_t height)
{
}

static void
shell_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	shell_ping,
	shell_configure,
	shell_popup_done
};


void
TPLTest::SetUp()
{
	__tpl_test_initialize();
}

void
TPLTest::TearDown()
{
	__tpl_test_finalize();
}

void
TPLTest::__tpl_test_initialize()
{
	wl_disp.display = wl_display_connect(NULL);
	wl_disp.registry = wl_display_get_registry(wl_disp.display);

	wl_registry_add_listener(wl_disp.registry, &registry_listener, &wl_disp);

	wl_display_roundtrip(wl_disp.display);

	wl_win.surface = wl_compositor_create_surface(wl_disp.compositor);
	wl_win.shell_surface = wl_shell_get_shell_surface(wl_disp.shell,
						  wl_win.surface);

	wl_shell_surface_add_listener(wl_win.shell_surface,
								  &shell_surface_listener, &wl_win);
	wl_shell_surface_set_title(wl_win.shell_surface, "TPL Test");
	wl_shell_surface_set_toplevel(wl_win.shell_surface);

	wl_win.native = wl_egl_window_create(wl_win.surface, width, height);

	tpl_display = tpl_display_create(TPL_BACKEND_WAYLAND,
				 (tpl_handle_t)wl_disp.display);
	tpl_surface = tpl_surface_create(tpl_display, (tpl_handle_t)wl_win.native,
				 TPL_SURFACE_TYPE_WINDOW, TBM_FORMAT_ARGB8888);
}

void
TPLTest::__tpl_test_finalize()
{
	if (tpl_surface != NULL) {
		tpl_object_unreference((tpl_object_t *) tpl_surface);
	}
	if (tpl_display != NULL) {
		tpl_object_unreference((tpl_object_t *) tpl_display);
	}

	wl_egl_window_destroy(wl_win.native);
	if (wl_win.shell_surface)
		wl_shell_surface_destroy(wl_win.shell_surface);
	if (wl_win.surface)
		wl_surface_destroy(wl_win.surface);
	if (wl_disp.shell)
		wl_shell_destroy(wl_disp.shell);
	if (wl_disp.compositor)
		wl_compositor_destroy(wl_disp.compositor);

	wl_registry_destroy(wl_disp.registry);
	wl_display_flush(wl_disp.display);
	wl_display_disconnect(wl_disp.display);
}

