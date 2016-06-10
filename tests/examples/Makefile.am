if USE_UVCH264
UVCH264_DIR=uvch264
else
UVCH264_DIR=
endif

if HAVE_GTK
GTK_EXAMPLES=mxf camerabin2 $(UVCH264_DIR)
else
GTK_EXAMPLES=
endif

if USE_DIRECTFB
DIRECTFB_DIR=directfb
else
DIRECTFB_DIR=
endif

OPENCV_EXAMPLES=opencv

if USE_OPENGL
GL_DIR=gl
else
GL_DIR=
endif

if USE_WAYLAND
WAYLAND_DIR=wayland
else
WAYLAND_DIR=
endif

SUBDIRS= mpegts $(DIRECTFB_DIR) $(GTK_EXAMPLES) $(OPENCV_EXAMPLES) $(GL_DIR) \
	$(WAYLAND_DIR)
DIST_SUBDIRS= mpegts camerabin2 directfb mxf opencv uvch264 gl wayland

include $(top_srcdir)/common/parallel-subdirs.mak
