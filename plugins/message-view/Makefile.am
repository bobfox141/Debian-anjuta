# Plugin glade file
message_view_gladedir = $(anjuta_glade_dir)
dist_message_view_glade_DATA = anjuta-message-manager-plugin.ui

message_view_pixmapsdir = $(anjuta_image_dir)
dist_message_view_pixmaps_DATA = \
	anjuta-messages-plugin.svg \
	anjuta-messages-plugin-48.png \
	anjuta-go-message-prev-16.png \
	anjuta-go-message-prev-24.png \
	anjuta-go-message-next-16.png \
	anjuta-go-message-next-24.png

message_view_uidir = $(anjuta_ui_dir)
dist_message_view_ui_DATA = anjuta-message-manager.xml

plugin_in_files = anjuta-message-manager.plugin.in
%.plugin: %.plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

message_view_plugindir = $(anjuta_plugin_dir)
dist_message_view_plugin_DATA = $(plugin_in_files:.plugin.in=.plugin)

AM_CPPFLAGS= \
	$(WARN_CFLAGS) \
	$(DEPRECATED_FLAGS) \
	$(LIBANJUTA_CFLAGS)

plugindir = $(anjuta_plugin_dir)
plugin_LTLIBRARIES = libanjuta-message-view.la

libanjuta_message_view_la_LDFLAGS = $(ANJUTA_PLUGIN_LDFLAGS)

libanjuta_message_view_la_LIBADD = \
	$(GIO_LIBS) \
	$(LIBANJUTA_LIBS)

libanjuta_message_view_la_SOURCES = \
	plugin.c\
	plugin.h\
	anjuta-msgman.c\
	anjuta-msgman.h\
	message-view.c\
	message-view.h

gsettings_in_file = org.gnome.anjuta.plugins.message-manager.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_in_file:.xml.in=.xml)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(plugin_in_files) \
	$(gsettings_in_file)

CLEANFILES = \
	$(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
