/* GStreamer
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2012 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 * Copyright (C) 2014 Collabora Ltd.
 * Copyright (C) 2015 Renesas Electronics Corporation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "waylandpool.h"
#include "waylandkmspool.h"
#include "wldisplay.h"
#include "wlvideoformat.h"

#include <fcntl.h>              /* O_CLOEXEC */
#include "xf86drm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

GST_DEBUG_CATEGORY_EXTERN (gstwayland_debug);
#define GST_CAT_DEFAULT gstwayland_debug

#define GST_WAYLAND_BUFFER_POOL_NUM  3

static void
gst_wl_kms_meta_free (GstWlKmsMeta * meta, GstBuffer * buffer)
{
  GstMemory *mem;
  gboolean is_dmabuf;
  guint n_mem;
  struct kms_bo *kms_bo;
  guint i;

  if (meta->kms_bo_array) {
    n_mem = gst_buffer_n_memory (buffer);
    for (i = 0; i < n_mem; i++) {
      mem = gst_buffer_get_memory (buffer, i);

      kms_bo = (struct kms_bo *) g_ptr_array_index (meta->kms_bo_array, i);

      is_dmabuf = gst_is_dmabuf_memory (mem);
      if (!is_dmabuf)
        kms_bo_unmap (kms_bo);
      else
        close (gst_dmabuf_memory_get_fd (mem));
      kms_bo_destroy (&kms_bo);
    }
    g_ptr_array_unref (meta->kms_bo_array);
  }

  g_mutex_lock (&meta->base.pool->buffers_map_mutex);
  g_hash_table_remove (meta->base.pool->buffers_map, meta->base.wbuffer);
  g_mutex_unlock (&meta->base.pool->buffers_map_mutex);

  wl_buffer_destroy (meta->base.wbuffer);
  wl_display_flush (meta->display->display);
  wl_display_roundtrip (meta->display->display);

  g_object_unref (meta->display);
}

const GstMetaInfo *
gst_wl_kms_meta_get_info (void)
{
  static const GstMetaInfo *wl_meta_info = NULL;

  if (g_once_init_enter (&wl_meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_WL_META_API_TYPE, "GstWlKmsMeta",
        sizeof (GstWlKmsMeta), (GstMetaInitFunction) NULL,
        (GstMetaFreeFunction) gst_wl_kms_meta_free,
        (GstMetaTransformFunction) NULL);
    g_once_init_leave (&wl_meta_info, meta);
  }
  return wl_meta_info;
}


/* bufferpool */
static gboolean gst_wayland_kms_buffer_pool_stop (GstBufferPool * pool);
static gboolean gst_wayland_kms_buffer_pool_start (GstBufferPool * pool);
static GstFlowReturn gst_wayland_kms_buffer_pool_alloc (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params);
static gboolean gst_wayland_kms_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config);

#define gst_wayland_kms_buffer_pool_parent_class parent_class
G_DEFINE_TYPE (GstWaylandKmsBufferPool, gst_wayland_kms_buffer_pool,
    GST_TYPE_WAYLAND_BUFFER_POOL);

static void
gst_wayland_kms_buffer_pool_class_init (GstWaylandKmsBufferPoolClass * klass)
{
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;
  gstbufferpool_class->stop = gst_wayland_kms_buffer_pool_stop;
  gstbufferpool_class->start = gst_wayland_kms_buffer_pool_start;
  gstbufferpool_class->alloc_buffer = gst_wayland_kms_buffer_pool_alloc;
  gstbufferpool_class->set_config = gst_wayland_kms_buffer_pool_set_config;
}

static void
gst_wayland_kms_buffer_pool_init (GstWaylandKmsBufferPool * self)
{
  self->kms_drv = NULL;
  self->allocator = NULL;
}

static gboolean
gst_wayland_buffer_pool_video_meta_map (GstVideoMeta * meta, guint plane,
    GstMapInfo * info, gpointer * data, gint * stride, GstMapFlags flags)
{
  GstBuffer *buffer = meta->buffer;
  GstMemory *mem;
  gint n_mem;

  n_mem = gst_buffer_n_memory (buffer);
  if (n_mem <= plane) {
    GST_ERROR ("plane is out of range (plane:%d)", plane);
    return FALSE;
  }

  mem = gst_buffer_get_memory (buffer, plane);
  if (!gst_memory_map (mem, info, flags)) {
    GST_ERROR ("failed to map memory (plane:%d)", plane);
    return FALSE;
  }

  *data = info->data;
  *stride = meta->stride[plane];

  return TRUE;
}

static gboolean
gst_wayland_buffer_pool_video_meta_unmap (GstVideoMeta * meta, guint plane,
    GstMapInfo * info)
{
  GstBuffer *buffer = meta->buffer;
  GstMemory *mem;
  gint n_mem;

  n_mem = gst_buffer_n_memory (buffer);
  if (n_mem <= plane) {
    GST_ERROR ("plane is out of range (plane:%d)", plane);
    return FALSE;
  }

  mem = gst_buffer_peek_memory (buffer, plane);

  gst_memory_unmap (mem, info);
  gst_memory_unref (mem);

  return TRUE;
}

static gboolean
gst_wayland_kms_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config)
{
  GstCaps *caps;
  guint size;

/* Always set the buffer pool min/max buffers to the defined value */
  if (gst_buffer_pool_config_get_params (config, &caps, &size, NULL, NULL)) {
    gst_buffer_pool_config_set_params (config, caps, size,
        GST_WAYLAND_BUFFER_POOL_NUM, GST_WAYLAND_BUFFER_POOL_NUM);
  }

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config);
}

static void
buffer_release (void *data, struct wl_buffer *wl_buffer)
{
  GstWaylandBufferPool *self = data;
  GstBuffer *buffer;
  GstWlMeta *meta;

  g_mutex_lock (&self->buffers_map_mutex);
  buffer = g_hash_table_lookup (self->buffers_map, wl_buffer);

  GST_LOG_OBJECT (self, "wl_buffer::release (GstBuffer: %p)", buffer);

  if (buffer) {
    meta = gst_buffer_get_wl_meta (buffer);
    if (meta->used_by_compositor) {
      meta->used_by_compositor = FALSE;
      /* unlock before unref because gst_wl_kms_meta_free() may be
         called from here */
      g_mutex_unlock (&self->buffers_map_mutex);
      gst_buffer_unref (buffer);
      return;
    }
  }
  g_mutex_unlock (&self->buffers_map_mutex);
}

static const struct wl_buffer_listener buffer_listener = {
  buffer_release
};

static gboolean
gst_wayland_kms_buffer_pool_start (GstBufferPool * pool)
{
  GstWaylandKmsBufferPool *self = GST_WAYLAND_KMS_BUFFER_POOL (pool);

  if (kms_create (self->base.display->drm_fd, &self->kms_drv)) {
    GST_ERROR_OBJECT (self, "kms_create failed");
    return FALSE;
  }
/* need the BASE class, not the WaylandBufferPool class here */
  return
      GST_BUFFER_POOL_CLASS (g_type_class_peek_parent (parent_class))->start
      (pool);
}

static gboolean
gst_wayland_kms_buffer_pool_stop (GstBufferPool * pool)
{
  GstWaylandKmsBufferPool *self = GST_WAYLAND_KMS_BUFFER_POOL (pool);

  GST_DEBUG_OBJECT (self, "Stopping wayland buffer pool");

  if (self->kms_drv)
    kms_destroy (&self->kms_drv);

  return
      GST_BUFFER_POOL_CLASS (g_type_class_peek_parent (parent_class))->stop
      (pool);
}

static gboolean
gst_wayland_buffer_pool_create_mp_buffer (GstWaylandBufferPool * wpool,
    GstBuffer * buffer, gint dmabuf[GST_VIDEO_MAX_PLANES],
    GstAllocator * allocator, gint width, gint height,
    void *data[GST_VIDEO_MAX_PLANES], gint in_stride[GST_VIDEO_MAX_PLANES],
    gsize offset[GST_VIDEO_MAX_PLANES], GstVideoFormat format, gint n_planes)
{
  GstWlKmsMeta *meta;
  GstWlMeta *wmeta;
  GstVideoMeta *vmeta;
  size_t size, maxsize;
  gboolean is_dmabuf;
  gint i;

  is_dmabuf = (allocator &&
      g_strcmp0 (allocator->mem_type, GST_ALLOCATOR_DMABUF) == 0);
  if (!is_dmabuf && data == NULL) {
    GST_WARNING_OBJECT (wpool, "couldn't get data pointer");
    return FALSE;
  }

  meta = (GstWlKmsMeta *) gst_buffer_add_meta
      (buffer, GST_WL_KMS_META_INFO, NULL);

  meta->kms_bo_array = NULL;

  wmeta = (GstWlMeta *) meta;

  wmeta->pool = wpool;
  wmeta->used_by_compositor = FALSE;

  /*
   * Wayland protocal APIs require that all (even unused) file descriptors be
   * valid. Instead of sending random dummy values, copy the valid fds from
   * the other planes.
   */
  if (n_planes == 1)
    dmabuf[1] = dmabuf[2] = dmabuf[0];
  else if (n_planes == 2)
    dmabuf[2] = dmabuf[0];

  wmeta->wbuffer =
      wl_kms_create_mp_buffer (wpool->display->kms, width, height,
      gst_video_format_to_wayland_format (format), dmabuf[0], in_stride[0],
      dmabuf[1], in_stride[1], dmabuf[2], in_stride[2]);

  for (i = 0; i < n_planes; i++) {
    size = GST_VIDEO_INFO_COMP_STRIDE (&wpool->info, i) *
        GST_VIDEO_INFO_COMP_HEIGHT (&wpool->info, i);

    if (is_dmabuf) {
      gst_buffer_append_memory (buffer,
          gst_dmabuf_allocator_alloc (allocator, dmabuf[i], size));
    } else {
      maxsize = in_stride[i] * height;

      gst_buffer_append_memory (buffer,
          gst_memory_new_wrapped (GST_MEMORY_FLAG_NO_SHARE, data[i],
              maxsize, 0, size, NULL, NULL));
    }
  }

  /* configure listening to wl_buffer.release */
  g_mutex_lock (&wpool->buffers_map_mutex);
  g_hash_table_insert (wpool->buffers_map, wmeta->wbuffer, buffer);
  g_mutex_unlock (&wpool->buffers_map_mutex);

  vmeta = gst_buffer_add_video_meta_full (buffer, GST_VIDEO_FRAME_FLAG_NONE,
      format, width, height, n_planes, offset, in_stride);
  vmeta->map = gst_wayland_buffer_pool_video_meta_map;
  vmeta->unmap = gst_wayland_buffer_pool_video_meta_unmap;

  wl_proxy_set_queue ((struct wl_proxy *) wmeta->wbuffer,
      wpool->display->queue);
  wl_buffer_add_listener (wmeta->wbuffer, &buffer_listener, wpool);

  return TRUE;
}

GstBuffer *
gst_wayland_buffer_pool_create_buffer_from_dmabuf (GstWaylandBufferPool * wpool,
    gint dmabuf[GST_VIDEO_MAX_PLANES], GstAllocator * allocator, gint width,
    gint height, gint in_stride[GST_VIDEO_MAX_PLANES], GstVideoFormat format,
    gint n_planes)
{
  GstBuffer *buffer;
  GstWlKmsMeta *wmeta;
  gsize offset[GST_VIDEO_MAX_PLANES] = { 0 };

  buffer = gst_buffer_new ();

  if (!gst_wayland_buffer_pool_create_mp_buffer (wpool, buffer, dmabuf,
          allocator, width, height, NULL, in_stride, offset, format,
          n_planes)) {
    GST_WARNING_OBJECT (wpool, "failed to create_mp_buffer");
    gst_buffer_unref (buffer);
    return NULL;
  }

  wmeta = gst_buffer_get_wl_kms_meta (buffer);

  wmeta->kms_bo_array = NULL;
  wmeta->display = g_object_ref (wpool->display);

  /* To avoid deattaching meta data when a buffer returns to the buffer pool */
  GST_META_FLAG_SET (wmeta, GST_META_FLAG_POOLED);

  return buffer;
}

static GstFlowReturn
gst_wayland_kms_buffer_pool_alloc (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstWaylandBufferPool *self = GST_WAYLAND_BUFFER_POOL_CAST (pool);
  GstWaylandKmsBufferPool *kms_self = GST_WAYLAND_KMS_BUFFER_POOL_CAST (pool);
  GstWlKmsMeta *meta;
  gint width, height;
  enum wl_kms_format format;
  gsize offset[GST_VIDEO_MAX_PLANES] = { 0 };
  gint stride[GST_VIDEO_MAX_PLANES] = { 0 };
  gint err;
  void *data[GST_VIDEO_MAX_PLANES] = { NULL };
  guint32 handle;
  gint dmabuf_fd[GST_VIDEO_MAX_PLANES];
  struct kms_bo *kms_bo[GST_VIDEO_MAX_PLANES];
  guint n_planes;
  guint i;
  unsigned attr[] = {
    KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
    KMS_WIDTH, 0,
    KMS_HEIGHT, 0,
    KMS_TERMINATE_PROP_LIST
  };

  width = GST_VIDEO_INFO_WIDTH (&self->info);
  height = GST_VIDEO_INFO_HEIGHT (&self->info);
  format =
      gst_video_format_to_wayland_format (GST_VIDEO_INFO_FORMAT (&self->info));

  *buffer = gst_buffer_new ();

  n_planes = GST_VIDEO_INFO_N_PLANES (&self->info);
  for (i = 0; i < n_planes; i++) {
    /* dumb BOs are treated with 32bpp format as hard-coded in libkms,
     * so a pixel stride we actually desire should be divided by 4 and
     * specified as a KMS BO width, which has been implemented
     * as a macro definition.
     */
    attr[3] = gst_wl_get_kms_bo_width (&self->info, i);
    attr[5] = GST_VIDEO_INFO_COMP_HEIGHT (&self->info, i);

    err = kms_bo_create (kms_self->kms_drv, attr, &kms_bo[i]);
    if (err) {
      GST_ERROR ("Failed to create kms bo");
      return GST_FLOW_ERROR;
    }

    kms_bo_get_prop (kms_bo[i], KMS_PITCH, (guint *) & stride[i]);

    kms_bo_get_prop (kms_bo[i], KMS_HANDLE, &handle);

    err = drmPrimeHandleToFD (self->display->drm_fd, handle, DRM_CLOEXEC,
        &dmabuf_fd[i]);
    if (err) {
      GST_ERROR_OBJECT (self, "drmPrimeHandleToFD failed. %s\n",
          strerror (errno));
      gst_buffer_unref (*buffer);
      return GST_FLOW_ERROR;
    }

    if (kms_self->allocator == NULL ||
        g_strcmp0 (kms_self->allocator->mem_type, GST_ALLOCATOR_DMABUF) != 0) {
      err = kms_bo_map (kms_bo[i], &data[i]);
      if (err) {
        GST_ERROR ("Failed to map kms bo");
        return GST_FLOW_ERROR;
      }
    }
  }


  if (!gst_wayland_buffer_pool_create_mp_buffer (self, *buffer, dmabuf_fd,
          kms_self->allocator, width, height, data, stride, offset,
          GST_VIDEO_INFO_FORMAT (&self->info), n_planes)) {
    GST_WARNING_OBJECT (self, "failed to create_mp_buffer");
    return GST_FLOW_ERROR;
  }


  meta = gst_buffer_get_wl_kms_meta (*buffer);

  meta->kms_bo_array = g_ptr_array_new ();
  for (i = 0; i < n_planes; i++)
    g_ptr_array_add (meta->kms_bo_array, (gpointer) kms_bo[i]);

  GST_DEBUG_OBJECT (self, "Allocating wl_kms buffer of size %" G_GSSIZE_FORMAT
      " (%d x %d), format %s", GST_VIDEO_INFO_SIZE (&self->info), width, height,
      gst_wayland_format_to_string (format));

  meta->display = g_object_ref (self->display);

  return GST_FLOW_OK;
}

#if 0
GstBufferPool *
gst_wayland_buffer_pool_new (GstWlDisplay * display)
{
  GstWaylandBufferPool *pool;

  g_return_val_if_fail (GST_IS_WL_DISPLAY (display), NULL);
  pool = g_object_new (GST_TYPE_WAYLAND_BUFFER_POOL, NULL);
  pool->display = g_object_ref (display);

  return GST_BUFFER_POOL_CAST (pool);
}
#endif
