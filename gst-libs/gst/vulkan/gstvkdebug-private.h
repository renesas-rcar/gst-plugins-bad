/*
 * GStreamer
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
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

#ifndef __GST_VULKAN_DEBUG_PRIVATE_H__
#define __GST_VULKAN_DEBUG_PRIVATE_H__

#include <gst/gst.h>
#include <vulkan/vulkan.h>
#include <gst/vulkan/vulkan-prelude.h>

G_BEGIN_DECLS

gchar *                     gst_vulkan_memory_property_flags_to_string      (VkMemoryPropertyFlags prop_bits);
gchar *                     gst_vulkan_memory_heap_flags_to_string          (VkMemoryHeapFlags prop_bits);
gchar *                     gst_vulkan_queue_flags_to_string                (VkQueueFlags queue_bits);
gchar *                     gst_vulkan_sample_count_flags_to_string         (VkSampleCountFlags sample_count_bits);

G_END_DECLS

#endif /* __GST_VULKAN_DEBUG_PRIVATE_H__ */