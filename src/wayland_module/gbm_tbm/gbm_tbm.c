/**************************************************************************

Copyright 2012 Samsung Electronics co., Ltd. All Rights Reserved.

Contact: Sangjin Lee <lsj119@samsung.com>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>
#include <xf86drm.h>

#include "gbm_tbmint.h"
#include <tbm_surface_internal.h>

#ifdef TPL_USING_WAYLAND_TBM
#include <wayland-tbm-server.h>
#else
#include <wayland-drm.h>
#endif

GBM_EXPORT tbm_bo
gbm_tbm_bo_get_tbm_bo(struct gbm_tbm_bo *bo)
{
   tbm_bo tbo;
   tbo = tbm_surface_internal_get_bo(bo->tbm_surf, 0);
   return tbo;
}

GBM_EXPORT uint32_t
gbm_tbm_surface_get_width(struct gbm_tbm_surface *surf)
{
   return surf->base.width;
}

GBM_EXPORT uint32_t
gbm_tbm_surface_get_height(struct gbm_tbm_surface *surf)
{
   return surf->base.height;
}

GBM_EXPORT uint32_t
gbm_tbm_surface_get_format(struct gbm_tbm_surface *surf)
{
   return surf->base.format;
}

GBM_EXPORT uint32_t
gbm_tbm_surface_get_flags(struct gbm_tbm_surface *surf)
{
   return surf->base.flags;
}

GBM_EXPORT void
gbm_tbm_surface_set_user_data(struct gbm_tbm_surface *surf, void *data)
{
   surf->tbm_private = data;
}

GBM_EXPORT void *
gbm_tbm_surface_get_user_data(struct gbm_tbm_surface *surf)
{
   return surf->tbm_private;
}

GBM_EXPORT void
gbm_tbm_device_set_callback_surface_has_free_buffers(struct gbm_tbm_device *gbm_tbm, int (*callback)(struct gbm_surface *))
{
   gbm_tbm->base.surface_has_free_buffers = callback;
}

GBM_EXPORT void
gbm_tbm_device_set_callback_surface_lock_front_buffer(struct gbm_tbm_device *gbm_tbm, struct gbm_bo *(*callback)(struct gbm_surface *))
{
   gbm_tbm->base.surface_lock_front_buffer = callback;
}

GBM_EXPORT void
gbm_tbm_device_set_callback_surface_release_buffer(struct gbm_tbm_device *gbm_tbm, void (*callback)(struct gbm_surface *, struct gbm_bo *))
{
   gbm_tbm->base.surface_release_buffer = callback;
}

static int
__gbm_tbm_is_format_supported(struct gbm_device *gbm,
			    uint32_t format,
			    uint32_t usage)
{
   switch (format)
   {
   case GBM_BO_FORMAT_XRGB8888:
   case GBM_FORMAT_XRGB8888:
       break;
   case GBM_BO_FORMAT_ARGB8888:
   case GBM_FORMAT_ARGB8888:
       if (usage & GBM_BO_USE_SCANOUT)
	   return 0;
       break;
   default:
       return 0;
   }

   if (usage & GBM_BO_USE_CURSOR_64X64 &&
       usage & GBM_BO_USE_RENDERING)
       return 0;

   return 1;
}

static int
__gbm_tbm_bo_write(struct gbm_bo *_bo, const void *buf, size_t count)
{
   struct gbm_tbm_bo *bo = gbm_tbm_bo(_bo);
   tbm_surface_info_s info;

   if (TBM_SURFACE_ERROR_NONE == tbm_surface_map(bo->tbm_surf, TBM_OPTION_WRITE, &info))
   {
		memcpy(info.planes[0].ptr, buf, count);
		tbm_surface_unmap(bo->tbm_surf);
		return 1;
   }

   return 0;
}

static int
__gbm_tbm_bo_get_fd(struct gbm_bo *_bo)
{
   struct gbm_tbm_bo *bo = gbm_tbm_bo(_bo);
   tbm_bo tbo;
   tbm_bo_handle handle;

   tbo = tbm_surface_internal_get_bo(bo->tbm_surf, 0);
   if (!tbo)
   {
      fprintf(stderr, "%s::tbm_surface_internal_get_bo() failed.\n", __FUNCTION__);
	  return 0;
   }
   handle = tbm_bo_get_handle(tbo, TBM_DEVICE_MM);

   return handle.s32;
}

static void
__gbm_tbm_bo_destroy(struct gbm_bo *_bo)
{
   struct gbm_tbm_bo *bo = gbm_tbm_bo(_bo);

   if(bo->tbm_surf)
   {
	  tbm_surface_destroy(bo->tbm_surf);
      bo->tbm_surf = NULL;
   }

   free(bo);
}

struct gbm_bo *
__gbm_tbm_bo_import(struct gbm_device *gbm, uint32_t type,
					void *buffer, uint32_t usage)
{
   struct gbm_tbm_bo *bo;
   tbm_surface_h tbm_surf = NULL;
   tbm_surface_info_s info;
   tbm_bo tbo;
   tbm_bo_handle handle;
   uint32_t size, offset, stride;
#ifndef TPL_USING_WAYLAND_TBM /* USING WAYLAND_DRM */
   struct wl_drm_buffer *drm_buffer;
#endif

   bo = calloc(1, sizeof *bo);
   if (bo == NULL)
	  return NULL;

   switch (type)
   {
      case GBM_BO_IMPORT_WL_BUFFER:
#ifndef TPL_USING_WAYLAND_TBM
         drm_buffer = wayland_drm_buffer_get(NULL, (struct wl_resource *)buffer);
		 if (drm_buffer)
		 {
		    switch (drm_buffer->format)
			{
			   case WL_DRM_FORMAT_ARGB8888:
			      info.format = GBM_FORMAT_ARGB8888;
				  break;
			   case WL_DRM_FORMAT_XRGB8888:
			      info.format = GBM_FORMAT_XRGB8888;
				  break;
			   default:
			      free(bo);
				  return NULL;
			}
		    info.width = drm_buffer->width;
			info.height = drm_buffer->height;
			info.bpp = tbm_surface_internal_get_bpp(info.format);
			info.num_planes = 1;
			info.planes[0].offset = 0;
			info.planes[0].stride = drm_buffer->stride[0];

		    tbo = drm_buffer->driver_buffer;
			tbm_surf = tbm_surface_internal_create_with_bos(&info, &tbo, 1);
		 }
#endif
	     break;
      default:
         free(bo);
         return NULL;
   }

   if (!tbm_surf)
   {
      free(bo);
	  return NULL;
   }

   tbm_surface_internal_get_plane_data(tbm_surf, 0, &size, &offset, &stride);
   bo->base.gbm = gbm;
   bo->base.width = tbm_surface_get_width(tbm_surf);
   bo->base.height = tbm_surface_get_height(tbm_surf);
   bo->base.format = tbm_surface_get_format(tbm_surf);
   bo->base.stride = stride;
   bo->usage = usage;

   handle = tbm_bo_get_handle(tbo, TBM_DEVICE_DEFAULT);
   bo->base.handle.u64 = handle.u64;

   return &bo->base;
}

static struct gbm_bo *
__gbm_tbm_bo_create(struct gbm_device *gbm,
		  uint32_t width, uint32_t height,
		  uint32_t format, uint32_t usage)
{
   struct gbm_tbm_bo *bo;
   uint32_t size, offset, pitch;
   int flags = TBM_BO_DEFAULT;
   int surface_format;
   tbm_bo_handle handle;
   tbm_bo tbo;

   bo = calloc(1, sizeof *bo);
   if (bo == NULL)
       return NULL;

   bo->base.gbm = gbm;
   bo->base.width = width;
   bo->base.height = height;
   bo->base.format = format;
   bo->usage = usage;

   switch (format)
   {
   case GBM_FORMAT_RGB565:
       surface_format = TBM_FORMAT_BGR565;
       break;
   case GBM_FORMAT_XRGB8888:
   case GBM_BO_FORMAT_XRGB8888:
       surface_format = TBM_FORMAT_XRGB8888;
       break;
   case GBM_FORMAT_ARGB8888:
   case GBM_BO_FORMAT_ARGB8888:
   case GBM_FORMAT_ABGR8888:
       surface_format = TBM_FORMAT_ABGR8888;
       break;
   default:
       free(bo);
       return NULL;
   }

   if ((usage & GBM_BO_USE_SCANOUT) || (usage & GBM_BO_USE_CURSOR_64X64))
   {
       flags |= TBM_BO_SCANOUT;
   }

   bo->tbm_surf = tbm_surface_internal_create_with_flags(width, height, surface_format, flags);
   if (!bo->tbm_surf)
   {
      free(bo);
      return NULL;
   }

   if (!tbm_surface_internal_get_plane_data(bo->tbm_surf, 0, &size, &offset, &pitch))
   {
      tbm_surface_destroy(bo->tbm_surf);
      free(bo);
      return NULL;
   }

   bo->base.stride = pitch;
   tbo = tbm_surface_internal_get_bo(bo->tbm_surf, 0);
   handle = tbm_bo_get_handle(tbo, TBM_DEVICE_DEFAULT);
   bo->base.handle.u64 = handle.u64;

   return &bo->base;
}

static struct gbm_surface *
__gbm_tbm_surface_create(struct gbm_device *gbm,
		       uint32_t width, uint32_t height,
		       uint32_t format, uint32_t flags)
{
   struct gbm_tbm_surface *surf;

   surf = calloc(1, sizeof *surf);
   if (surf == NULL)
       return NULL;

   surf->base.gbm = gbm;
   surf->base.width = width;
   surf->base.height = height;
   surf->base.format = format;
   surf->base.flags = flags;

   return &surf->base;
}

static void
__gbm_tbm_surface_destroy(struct gbm_surface *_surf)
{
   struct gbm_tbm_surface *surf = gbm_tbm_surface(_surf);

   free(surf);
}

static void
__tbm_destroy(struct gbm_device *gbm)
{
   struct gbm_tbm_device *dri = gbm_tbm_device(gbm);

   if (dri->bufmgr)
	  tbm_bufmgr_deinit(dri->bufmgr);

   if (dri->driver_name)
	  free(dri->driver_name);

   free(dri);
}

static struct gbm_device *
__tbm_device_create(int fd)
{
   struct gbm_tbm_device *dri;

   dri = calloc(1, sizeof *dri);

   dri->driver_name = drmGetDeviceNameFromFd(fd);
   if (dri->driver_name == NULL)
      goto fail;

   dri->bufmgr = tbm_bufmgr_init(fd);
   if (dri->bufmgr == NULL)
      goto fail;

   dri->base.fd = fd;
   dri->base.bo_create = __gbm_tbm_bo_create;
   dri->base.bo_import = __gbm_tbm_bo_import;
   dri->base.is_format_supported = __gbm_tbm_is_format_supported;
   dri->base.bo_write = __gbm_tbm_bo_write;
   dri->base.bo_get_fd = __gbm_tbm_bo_get_fd;
   dri->base.bo_destroy = __gbm_tbm_bo_destroy;
   dri->base.destroy = __tbm_destroy;
   dri->base.surface_create = __gbm_tbm_surface_create;
   dri->base.surface_destroy = __gbm_tbm_surface_destroy;
   dri->base.name = "drm";

   return &dri->base;

fail:
   if (dri->bufmgr)
      tbm_bufmgr_deinit(dri->bufmgr);

   if (dri->driver_name)
      free(dri->driver_name);

   free(dri);
   return NULL;
}

struct gbm_backend gbm_backend = {
   .backend_name = "tbm",
   .create_device = __tbm_device_create,
};
