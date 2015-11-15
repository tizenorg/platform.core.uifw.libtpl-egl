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

#ifndef _GBM_TBM_INTERNAL_H_
#define _GBM_TBM_INTERNAL_H_

#include "gbm_tbm.h"
#include <gbm/gbmint.h>
#include <tbm_bufmgr.h>
#include <tbm_surface.h>

struct gbm_tbm_device {
   struct gbm_device base;
   tbm_bufmgr bufmgr;
   char *driver_name;
};

struct gbm_tbm_bo {
   struct gbm_bo base;
   tbm_surface_h tbm_surf;
   uint32_t usage;
};

struct gbm_tbm_surface {
   struct gbm_surface base;
   void *tbm_private;
};

static inline struct gbm_tbm_device *
gbm_tbm_device(struct gbm_device *gbm)
{
   return (struct gbm_tbm_device *) gbm;
}

static inline struct gbm_tbm_bo *
gbm_tbm_bo(struct gbm_bo *bo)
{
   return (struct gbm_tbm_bo *) bo;
}

static inline struct gbm_tbm_surface *
gbm_tbm_surface(struct gbm_surface *surface)
{
   return (struct gbm_tbm_surface *) surface;
}

#endif
