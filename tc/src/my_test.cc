#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <tbm_surface.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <tpl.h>

#ifdef __cplusplus
}
#endif

#include "gtest/gtest.h"


class TPLTestFixture : public ::testing::Test {
public:
    struct wl_display *dpy;
    tpl_display_t *tpl_display;

protected:
    virtual void SetUp() {
        dpy = wl_display_connect(NULL);
        tpl_display = tpl_display_create(TPL_BACKEND_WAYLAND, (tpl_handle_t) dpy);
    }

    virtual void TearDown() {
        tpl_object_unreference((tpl_object_t *)tpl_display);

        wl_display_flush(dpy);
        wl_display_disconnect(dpy);

        //TODO: Memory Handling
    }

};


TEST_F(TPLTestFixture, tpl_display_get) {
    ASSERT_NE((void *)0, tpl_display_get((tpl_handle_t) dpy));
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

