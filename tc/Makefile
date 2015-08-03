############################################################
# Makefile for GLES 2.x test application
#    Created by yigl - 090113
############################################################

############################################################
# configuration
############################################################

NULL =

TOOLCHAIN_PREFIX =

# Application name

BIN_NAME    = tpl-test

# Debugging mode: yes, no
DEBUG_MODE  = yes

# Project path
INSTALL_DIR = /usr/bin

# C110_SGX540, ...
#GLES_CORE   = C110_SGX540
#GLES_CORE   = C210_MALI400

# WS_NULL, WS_X11
NATIVE_WIN	= WS_WL
#NATIVE_WIN	= WS_NULL

# Defines
DEFS        =

# Required packages
REQ_PKGS    =

# LDFLAGS
LDFLAGS     = -lm -lrt -lpthread -ltpl-egl   # -lEGL -lGLESv2

SUPPORT_LIBPNG=0
ifeq ($(SUPPORT_LIBPNG), 1)
REQ_PKGS   += libpng
endif

# Hostname
HOST_NAME   = `uname -n`


# Checks GLES core
#ifeq  ($(GLES_CORE), C110_SGX540)
#	DEFS     += -DGLES_C110_SGX540
#	REQ_PKGS += opengl-es-20
#endif
REQ_PKGS   += gles20

# Checks window system
ifeq  ($(NATIVE_WIN), WS_NULL)
	DEFS     += -DWS_NULL
	REQ_PKGS +=
	C_SRCS   += \
			    src/gfx_native_wnd_null.c \
				$(NULL)
endif

ifeq  ($(NATIVE_WIN), WS_X11)
	DEFS     += -DWS_X11
REQ_PKGS += x11 xext libdrm libtbm libudev
	C_HEADERS+= \
				src/gfx_util_x11.h \
				src/gfx_egl_x11.h \
				src/gfx_data_00.h \
				src/gfx_data_01.h \
				src/gfx_data_02.h \
				$(NULL)
	C_SRCS   += \
				src/gfx_native_wnd_x11.c \
				src/gfx_util_x11.c \
				src/gfx_egl_x11.c \
				src/gfx_test_gles_horizontal_tearing_with_xevent.c \
				src/gfx_test_gles_inconsistent_rendering.c \
				src/gfx_test_gles_inconsistent_rendering_with_xevent.c \
				src/gfx_test_gles_measure_perf_with_xevent.c \
				src/gfx_test_egl_client_pixmap_basic.c \
				src/gfx_test_egl_client_pixmap_glReadPixels.c \
				src/gfx_test_egl_pixmap_surface_pixmap.c \
				src/gfx_test_egl_pixmap_surface_x11_n_gles_rendering.c \
				src/gfx_test_egl_pixmap_surface_glReadPixels.c \
				src/gfx_test_egl_pixmap_surface_pixmap_eglimage.c \
				src/gfx_test_egl_window_surface_resize_x11_window.c \
				src/gfx_test_egl_window_surface_transparency.c \
				src/gfx_test_egl_EGL_texture_from_pixmap.c \
				src/gfx_test_eglimg_texture_from_pixmap.c \
				src/gfx_test_eglimg_texture_from_pixmap_many_imgs.c \
				src/gfx_test_eglimg_texture_from_pixmap_width.c \
				src/gfx_test_eglimg_texture_from_pixmap_multi_thread.c \
				src/gfx_test_eglimg_texture_from_pixmap_multi_thread2.c \
				src/gfx_test_gles_wagonwheel_effect.c \
				src/gfx_test_eglimg_rgb_yuv_texture.c\
				src/gfx_test_eglimg_rgb_yuv_texture_format.c\
				$(NULL)
endif


ifeq  ($(NATIVE_WIN), WS_WL)
        DEFS     += -DWS_WL
REQ_PKGS += wayland wayland-client wayland-egl wayland-drm libdrm libtbm libudev
        C_HEADERS+= \
#                                src/gfx_util_x11.h \
                                src/gfx_egl_x11.h \
                                src/gfx_data_00.h \
                                src/gfx_data_01.h \
                                src/gfx_data_02.h \
                                $(NULL)
        C_SRCS   += \
 #                               src/gfx_native_wnd_wayland.c \
                                src/gfx_util_wayland.c \
                                src/gfx_egl_x11.c \
                                src/gfx_test_gles_inconsistent_rendering.c \
                                src/gfx_test_egl_window_surface_transparency.c \
                                $(NULL)
endif

############################################################
# build tools
############################################################

CPP = $(TOOLCHAIN_PREFIX)cpp
CC  = $(TOOLCHAIN_PREFIX)gcc
CXX = $(TOOLCHAIN_PREFIX)g++
LD  = $(TOOLCHAIN_PREFIX)ld

############################################################
# tool flags
############################################################

CPP_FLAGS = \
			-fPIC \
			-Wall \
			-I$(PWD)/src \
			$(DEFS) \
			-DBLD_HOST_NAME=\"$(HOST_NAME)\" \
			-I/usr/include/libdrm/ `pkg-config --cflags $(REQ_PKGS)` \
			$(NULL)

ifeq ($(SUPPORT_LIBPNG), 1)
CPP_FLAGS += -DSUPPORT_LIBPNG
endif

CFLAGS    = \
			$(CPP_FLAGS) \
			$(NULL)

CXXFLAGS  = \
			$(CPP_FLAGS) \
			$(NULL)

ifeq ($(DEBUG_MODE), yes)
	CFLAGS   += -g -DGFX_ENABLE_LOG=1
	CXXFLAGS += -g -DGFX_ENABLE_LOG=1
else
#	CFLAGS   += -O2
#	CXXFLAGS += -O2
	CFLAGS   += -g
	CXXFLAGS += -g
endif

############################################################
# library path and libs
############################################################

LDFLAGS  += \
			`pkg-config --libs $(REQ_PKGS)` -lwayland-client -ludev -ltbm -lwayland-egl -ldrm\
			$(NULL)

############################################################
# sources
############################################################

#C_HEADERS += \
			src/gfx_platform.h \
			src/gfx_util.h \
			src/gfx_list.h \
			src/gfx_egl.h \
			src/gfx_test.h \
		    $(NULL)
C_HEADERS +=            src/tpl_test_util.h #src/gfx_egl.h
C_SRCS    += \
			src/main.c src/tpl_test_surface.c src/tpl_test_display.c src/tpl_test_object.c src/tpl_test_buffer.c
			#src/gfx_util.c  src/gfx_test_eglimg_map_render_basic.c src/main.c
			#src/gfx_list.c \
			src/gfx_egl_common.c \
			src/gfx_test_egl_memory.c \
			src/gfx_test_gles_ext_basic.c \
			src/gfx_test_egl_color_buffer_preservation.c \
			src/gfx_test_gles_tex_mapping.c \
			src/gfx_test_gles_tex_mapping_type.c \
			src/gfx_test_gles_tex_render_basic.c \
			src/gfx_test_gles_tex_render_many_objs.c \
			src/gfx_test_gles_tex_update_basic.c \
			src/gfx_test_gles_tex_update_non_multiple_of_32.c \
			src/gfx_test_eglimg_map_render_basic.c \
			src/gfx_test_eglimg_map_render_many_objs.c \
			src/gfx_test_eglimg_map_update_basic.c \
			src/gfx_test_eglimg_map_update_non_multiple_of_32.c \
			src/gfx_test_eglimg_map_update_width.c \
			src/gfx_test_eglimg_map_n_client_pixmap_render_basic.c \
			src/gfx_test_gles_stencil_operation.c \
			src/gfx_test_gles_GL_TRIANGLES.c \
			src/gfx_test_gles_landscape.c \
			src/gfx_test_gles_texture_width.c \
			src/gfx_test_gles_horizontal_tearing.c \
			src/gfx_test_gles_measure_perf_alpha_blending.c \
			src/gfx_test_gles_measure_perf_pixel_fillrate.c \
			src/gfx_test_gles_measure_perf_texel_fillrate.c \
			src/gfx_test_gles_fbo_basic.c \
			src/gfx_test_gles_frag_shader_basic.c \
			src/gfx_test_gles_tex_render_tga.c \
			src/gfx_test_gles_binary_shader_basic.c \
			src/gfx_test_gles_binary_program_basic.c \
			src/gfx_test_gles_fbo_multisample.c \
			src/gfx_test_eglimg_map_render_2d.c \
			src/gfx_test_egl_pbuffer_basic.c \
			src/gfx_test_gles_cube_tex_render_basic.c \
			src/gfx_test_gles_cube_tex_render_fbo.c \
			src/gfx_test_egl_swap_buffer_with_damage.c \
			src/gfx_test_gles_qcom_tile_partial_rendering.c \
			src/main.c \
		    $(NULL)

CXX_SRCS  += \
			$(NULL)

OBJS      = \
		    $(CXX_SRCS:%.cpp=%.o) \
		    $(C_SRCS:%.c=%.o) \
		    $(NULL)

############################################################
# targets and rules
############################################################

all: $(BIN_NAME)

$(BIN_NAME): $(OBJS) $(C_HEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

install: $(BIN_NAME)
	cp -va  $(BIN_NAME) $(INSTALL_DIR)

clean:
	find . -name "*.o" -exec rm -vf {} \;
	find . -name "*~" -exec rm -vf {} \;
	rm -vf $(BIN_NAME)
