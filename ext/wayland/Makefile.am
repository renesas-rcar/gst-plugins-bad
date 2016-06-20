plugin_LTLIBRARIES = libgstwaylandsink.la

libgstwaylandsink_la_SOURCES =  \
	gstwaylandsink.c \
	wlshmallocator.c \
	wlbuffer.c \
	wldisplay.c \
	wlwindow.c \
	wlvideoformat.c \
	wldmabuf.c \
	scaler-protocol.c \
	linux-dmabuf-unstable-v1-protocol.c

libgstwaylandsink_la_CFLAGS = $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) \
                               $(WAYLAND_CFLAGS) $(GST_PLUGINS_BAD_CFLAGS)
libgstwaylandsink_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) \
	-lgstvideo-$(GST_API_VERSION) \
	-lgstallocators-$(GST_API_VERSION) \
	$(WAYLAND_LIBS) \
	$(top_builddir)/gst-libs/gst/wayland/libgstwayland-$(GST_API_VERSION).la
libgstwaylandsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstwaylandsink_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = \
	gstwaylandsink.h \
	wlshmallocator.h \
	wlbuffer.h \
	wldisplay.h \
	wlwindow.h \
	wlvideoformat.h \
	wldmabuf.h \
	scaler-client-protocol.h \
	linux-dmabuf-unstable-v1-client-protocol.h

EXTRA_DIST = scaler.xml
CLEANFILES = scaler-protocol.c scaler-client-protocol.h \
	linux-dmabuf-unstable-v1-protocol.c \
	linux-dmabuf-unstable-v1-client-protocol.h

gstwaylandsink.c: scaler-client-protocol.h linux-dmabuf-unstable-v1-client-protocol.h

waylandpool.c: scaler-client-protocol.h

wlbuffer.c: scaler-client-protocol.h

wldisplay.c: scaler-client-protocol.h linux-dmabuf-unstable-v1-client-protocol.h

wlwindow.c: scaler-client-protocol.h

wldmabuf.c: linux-dmabuf-unstable-v1-client-protocol.h

wlshmallocator.c: scaler-client-protocol.h
.SECONDEXPANSION:

define protostability
$(if $(findstring unstable,$1),unstable,stable)
endef

define protoname
$(shell echo $1 | sed 's/\([a-z\-]\+\)-[a-z]\+-v[0-9]\+/\1/')
endef

WAYLAND_GEN_PROTOCOL_CODE = $(wayland_scanner) code < $< > $@
WAYLAND_GEN_PROTOCOL_HEADER = $(wayland_scanner) client-header < $< > $@

scaler-protocol.c : scaler.xml
	$(WAYLAND_GEN_PROTOCOL_CODE)

scaler-client-protocol.h : scaler.xml
	$(WAYLAND_GEN_PROTOCOL_HEADER)

%-protocol.c : $(WAYLAND_PROTOCOLS_DATADIR)/$$(call protostability,$$*)/$$(call protoname,$$*)/$$*.xml
	$(WAYLAND_GEN_PROTOCOL_CODE)

%-client-protocol.h : $(WAYLAND_PROTOCOLS_DATADIR)/$$(call protostability,$$*)/$$(call protoname,$$*)/$$*.xml
	$(WAYLAND_GEN_PROTOCOL_HEADER)
