/* GStreamer Wayland buffer pool
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2012 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 * Copyright (C) 2014 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_WAYLAND_KMS_BUFFER_POOL_H__
#define __GST_WAYLAND_KMS_BUFFER_POOL_H__

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>

#include <gst/allocators/gstdmabuf.h>

#include "wldisplay.h"

#include "libkms.h"

G_BEGIN_DECLS

#define GST_TYPE_WAYLAND_KMS_BUFFER_POOL \
     (gst_wayland_kms_buffer_pool_get_type())
#define GST_IS_WAYLAND_KMS_BUFFER_POOL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_WAYLAND_KMS_BUFFER_POOL))
#define GST_WAYLAND_KMS_BUFFER_POOL(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_WAYLAND_KMS_BUFFER_POOL, GstWaylandKmsBufferPool))
#define GST_WAYLAND_KMS_BUFFER_POOL_CAST(obj) ((GstWaylandKmsBufferPool*)(obj))

typedef struct _GstWaylandKmsBufferPool GstWaylandKmsBufferPool;
typedef struct _GstWaylandKmsBufferPoolClass GstWaylandKmsBufferPoolClass;

/* buffer meta */
typedef struct _GstWlKmsMeta GstWlKmsMeta;

const GstMetaInfo * gst_wl_kms_meta_get_info (void);
#define GST_WL_KMS_META_INFO  (gst_wl_kms_meta_get_info())

#define gst_buffer_get_wl_kms_meta(b) ((GstWlKmsMeta*)gst_buffer_get_meta((b),GST_WL_META_API_TYPE))

#define gst_wl_get_kms_bo_width(i, p) \
    ((((GST_VIDEO_INFO_PLANE_STRIDE (i, p) + 3) / 4 + 31) >> 5) << 5)

struct _GstWlKmsMeta {
  GstWlMeta base;
  GPtrArray *kms_bo_array;
  GstWlDisplay *display;
};

/* buffer pool */
struct _GstWaylandKmsBufferPool
{
  GstWaylandBufferPool base;
  struct kms_driver *kms_drv;
  GstAllocator *allocator;
};

struct _GstWaylandKmsBufferPoolClass
{
  GstWaylandBufferPoolClass parent_class;
  GstBufferPoolClass *grandparent_class;
};

GType gst_wayland_kms_buffer_pool_get_type (void);
GstBuffer *
gst_wayland_buffer_pool_create_buffer_from_dmabuf (GstWaylandBufferPool * wpool,
    gint dmabuf[GST_VIDEO_MAX_PLANES], GstAllocator * allocator, gint width,
    gint height, gint in_stride[GST_VIDEO_MAX_PLANES], GstVideoFormat format,
    gint n_planes);

G_END_DECLS

#endif /*__GST_WAYLAND_KMS_BUFFER_POOL_H__*/
