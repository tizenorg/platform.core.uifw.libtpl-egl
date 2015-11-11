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

#include <wayland-drm.h>

GBM_EXPORT tbm_bo
gbm_tbm_bo_get_tbm_bo(struct gbm_tbm_bo *bo)
{
   return bo->bo;
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
   gbm_tbm->base.base.surface_has_free_buffers = callback;
}

GBM_EXPORT void
gbm_tbm_device_set_callback_surface_lock_front_buffer(struct gbm_tbm_device *gbm_tbm, struct gbm_bo *(*callback)(struct gbm_surface *))
{
   gbm_tbm->base.base.surface_lock_front_buffer = callback;
}

GBM_EXPORT void
gbm_tbm_device_set_callback_surface_release_buffer(struct gbm_tbm_device *gbm_tbm, void (*callback)(struct gbm_surface *, struct gbm_bo *))
{
   gbm_tbm->base.base.surface_release_buffer = callback;
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
   void *mapped = NULL;

   mapped = (void *)tbm_bo_map(bo->bo, TBM_DEVICE_CPU, TBM_OPTION_WRITE).ptr;
   memcpy(mapped, buf, count);
   tbm_bo_unmap(bo->bo);

   return 0;
}

static int
__gbm_tbm_bo_get_fd(struct gbm_bo *_bo)
{
   struct gbm_tbm_bo *bo = gbm_tbm_bo(_bo);
   tbm_bo_handle handle;

   handle = tbm_bo_get_handle(bo->bo, TBM_DEVICE_MM);

   return handle.s32;
}

static void
__gbm_tbm_bo_destroy(struct gbm_bo *_bo)
{
   struct gbm_tbm_bo *bo = gbm_tbm_bo(_bo);

   if(bo->bo)
   {
      tbm_bo_unref(bo->bo);
      bo->bo = NULL;
   }

   free(bo);
}

struct gbm_bo *
__gbm_tbm_bo_import(struct gbm_device *gbm, uint32_t type,
		  void *buffer, uint32_t usage)
{
   struct gbm_tbm_device *dri = gbm_tbm_device(gbm);
   struct gbm_tbm_bo *bo;
   struct wl_drm_buffer *drm_buffer;
   tbm_bo tbo;
   uint32_t width, height;
   unsigned int stride, format;
   tbm_bo_handle handle;

   bo = calloc(1, sizeof *bo);
   if (bo == NULL)
       return NULL;

   switch (type)
   {
   case GBM_BO_IMPORT_WL_BUFFER:
   {
      if (!dri->wl_drm)
      {
	 free(bo);
	 return NULL;
      }

      drm_buffer = wayland_drm_buffer_get((struct wl_drm *)dri->wl_drm, (struct wl_resource *)buffer);
      if (!drm_buffer)
      {
	 free(bo);
	 return NULL;
      }

      switch (drm_buffer->format)
      {
      case WL_DRM_FORMAT_ARGB8888:
	 format = GBM_FORMAT_ARGB8888;
	 break;
      case WL_DRM_FORMAT_XRGB8888:
	 format = GBM_FORMAT_XRGB8888;
	 break;
      default:
	 free(bo);
	 return NULL;
      }

      width = drm_buffer->width;
      height = drm_buffer->height;
      stride = drm_buffer->stride[0];

      tbo = drm_buffer->driver_buffer;
      break;
   }
   default:
      free(bo);
      return NULL;
   }

   bo->base.base.gbm = gbm;
   bo->base.base.width = width;
   bo->base.base.height = height;
   bo->base.base.format = format;
   bo->base.base.stride = stride;

   bo->format = format;
   bo->usage = usage;

   bo->bo = tbm_bo_ref(tbo);
   handle = tbm_bo_get_handle(bo->bo, TBM_DEVICE_DEFAULT);

   bo->base.base.handle.u64 = handle.u64;
   return &bo->base.base;
}

static struct gbm_bo *
__gbm_tbm_bo_create(struct gbm_device *gbm,
		  uint32_t width, uint32_t height,
		  uint32_t format, uint32_t usage)
{
   struct gbm_tbm_device *dri = gbm_tbm_device(gbm);
   struct gbm_tbm_bo *bo;
   uint32_t size, offset, pitch;
   int flags = TBM_BO_DEFAULT;
   int surface_format;
   tbm_bo_handle handle;
   tbm_surface_h surface;

   bo = calloc(1, sizeof *bo);
   if (bo == NULL)
       return NULL;

   bo->base.base.gbm = gbm;
   bo->base.base.width = width;
   bo->base.base.height = height;
   bo->base.base.format = format;

   bo->format = format;
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

   surface = tbm_surface_internal_create_with_flags(width, height, surface_format, flags);
   if (!surface)
   {
      free(bo);
      return NULL;
   }

   bo->bo = tbm_surface_internal_get_bo(surface, 0);
   tbm_bo_ref(bo->bo);

   if (!tbm_surface_internal_get_plane_data(surface, 0, &size, &offset, &pitch))
   {
      free(bo);
      return NULL;
   }

   tbm_surface_internal_destroy(surface);

   bo->base.base.stride = pitch;
   handle = tbm_bo_get_handle(bo->bo, TBM_DEVICE_DEFAULT);

   bo->base.base.handle.ptr = handle.ptr;

   return &bo->base.base;
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

    if (dri->base.driver_name)
	free(dri->base.driver_name);

    free(dri);
}

static struct gbm_device *
__tbm_device_create(int fd)
{
   struct gbm_tbm_device *dri;

   dri = calloc(1, sizeof *dri);

   dri->base.driver_name = drmGetDeviceNameFromFd(fd);
   if (dri->base.driver_name == NULL)
      goto bail;

   dri->bufmgr = tbm_bufmgr_init(fd);
   if (dri->bufmgr == NULL)
      goto bail;

   dri->base.base.fd = fd;
   dri->base.base.bo_create = __gbm_tbm_bo_create;
   dri->base.base.bo_import = __gbm_tbm_bo_import;
   dri->base.base.is_format_supported = __gbm_tbm_is_format_supported;
   dri->base.base.bo_write = __gbm_tbm_bo_write;
   dri->base.base.bo_get_fd = __gbm_tbm_bo_get_fd;
   dri->base.base.bo_destroy = __gbm_tbm_bo_destroy;
   dri->base.base.destroy = __tbm_destroy;
   dri->base.base.surface_create = __gbm_tbm_surface_create;
   dri->base.base.surface_destroy = __gbm_tbm_surface_destroy;

   dri->base.type = GBM_DRM_DRIVER_TYPE_DRI;
   dri->base.base.name = "drm";

   return &dri->base.base;

bail:
   if (dri->bufmgr)
      tbm_bufmgr_deinit(dri->bufmgr);

   if (dri->base.driver_name)
      free(dri->base.driver_name);

   free(dri);
   return NULL;
}

struct gbm_backend gbm_backend = {
   .backend_name = "tbm",
   .create_device = __tbm_device_create,
};
