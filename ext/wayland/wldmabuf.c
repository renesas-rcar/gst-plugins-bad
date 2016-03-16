/* GStreamer Wayland video sink
 *
 * Copyright (C) 2014-2015 Collabora Ltd.
 * Copyright (C) 2016 Renesas Electronics Corporation
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Based on wldmabufsink by Collabora Ltd.
 *
 */

#include <gst/allocators/gstdmabuf.h>

#include "wldmabuf.h"
#include "wlbuffer.h"
#include "wlvideoformat.h"
#include "linux-dmabuf-client-protocol.h"

typedef struct
{
  GMutex lock;
  GCond cond;
  struct wl_buffer *wbuf;
  gboolean done;
  GstWaylandSink *sink;
} ConstructBufferData;

static void
linux_dmabuf_buffer_created (void *data,
    struct zlinux_buffer_params *params, struct wl_buffer *buffer)
{
  ConstructBufferData *d = data;

  g_mutex_lock (&d->lock);
  GST_DEBUG_OBJECT (d->sink, "wl_buffer %p created", buffer);
  d->wbuf = buffer;
  zlinux_buffer_params_destroy (params);
  d->done = TRUE;
  g_cond_signal (&d->cond);
  g_mutex_unlock (&d->lock);
}

static void
linux_dmabuf_buffer_failed (void *data, struct zlinux_buffer_params *params)
{
  ConstructBufferData *d = data;

  g_mutex_lock (&d->lock);
  GST_DEBUG_OBJECT (d->sink, "failed to create wl_buffer");
  d->wbuf = NULL;
  zlinux_buffer_params_destroy (params);
  d->done = TRUE;
  g_cond_signal (&d->cond);
  g_mutex_unlock (&d->lock);
}

static const struct zlinux_buffer_params_listener buffer_params_listener = {
  linux_dmabuf_buffer_created,
  linux_dmabuf_buffer_failed,
};

static struct wl_buffer *
gst_wl_dmabuf_construct_wl_buffer (GstWaylandSink * sink, GstBuffer * buf,
    const GstVideoInfo * info)
{
  struct zlinux_buffer_params *params;
  ConstructBufferData data;
  GstVideoMeta *vidmeta;
  gint i, n_planes, fd;
  gint64 timeout;
  GstMemory *mem;

  GST_DEBUG_OBJECT (sink, "Creating wl_buffer for buffer %p", buf);

  data.sink = sink;
  g_cond_init (&data.cond);
  g_mutex_init (&data.lock);
  g_mutex_lock (&data.lock);

  params = zlinux_dmabuf_create_params (sink->display->dmabuf);
  zlinux_buffer_params_add_listener (params, &buffer_params_listener, &data);

  vidmeta = gst_buffer_get_video_meta (buf);
  n_planes = vidmeta ? vidmeta->n_planes : GST_VIDEO_INFO_N_PLANES (info);

  for (i = 0; i < n_planes; i++) {
    mem = gst_buffer_peek_memory (buf, i);
    fd = gst_dmabuf_memory_get_fd (mem);
    zlinux_buffer_params_add (params, fd, i,    /* plane id */
        mem->offset,            /* memory offset */
        vidmeta ? vidmeta->stride[i] : GST_VIDEO_INFO_PLANE_STRIDE (info, i),
        0, 0);
  }

  zlinux_buffer_params_create (params, info->width, info->height,
      gst_video_format_to_wl_dmabuf_format (info->finfo->format),
      ZLINUX_BUFFER_PARAMS_FLAGS_Y_INVERT);
  wl_display_flush (sink->display->display);

  data.done = FALSE;
  timeout = g_get_monotonic_time () + G_TIME_SPAN_SECOND;
  while (!data.done) {
    if (!g_cond_wait_until (&data.cond, &data.lock, timeout)) {
      GST_ERROR_OBJECT (sink, "timed out while waiting for "
          "zlinux_buffer_params event");
      goto error;
    }
  }

done:
  g_mutex_unlock (&data.lock);
  g_mutex_clear (&data.lock);
  g_cond_clear (&data.cond);
  return data.wbuf;
error:
  zlinux_buffer_params_destroy (params);
  data.wbuf = NULL;
  goto done;
}

GstFlowReturn
gst_wl_dmabuf_create_wl_buffer (GstWaylandSink * sink, GstBuffer * buffer,
    GstBuffer ** to_render)
{
  struct wl_buffer *wbuf = NULL;

  if (!gst_is_dmabuf_memory (gst_buffer_peek_memory (buffer, 0))) {
    GST_WARNING_OBJECT (sink, "buffer does not have dmabuf memory");
    return GST_FLOW_NOT_NEGOTIATED;
  }

  wbuf = gst_wl_dmabuf_construct_wl_buffer (sink, buffer, &sink->video_info);
  if (!wbuf) {
    GST_ELEMENT_ERROR (sink, RESOURCE, FAILED,
        ("failed to create wl_buffer out of dmabuf memory, cannot continue"),
        (NULL));
    return GST_FLOW_ERROR;
  }

  gst_buffer_add_wl_buffer (buffer, wbuf, sink->display);
  *to_render = buffer;

  return GST_FLOW_OK;
}
