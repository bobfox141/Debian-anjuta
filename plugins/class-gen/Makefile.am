# Plugin glade file
clsgen_gladedir = $(anjuta_glade_dir)
dist_clsgen_glade_DATA = anjuta-class-gen-plugin.ui

# Templates
SUBDIRS = templates

# Plugin Icon file
plugin_in_files = anjuta-class-gen.plugin.in
%.plugin: %.plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache


# Plugin pixmaps
clsgen_pixmapsdir = $(anjuta_image_dir)
dist_clsgen_pixmaps_DATA = \
	anjuta-class-gen-plugin.svg \
	anjuta-class-gen-plugin-48.png

clsgen_plugindir = $(anjuta_plugin_dir)
dist_clsgen_plugin_DATA = $(plugin_in_files:.plugin.in=.plugin)

plugindir = $(anjuta_plugin_dir)
plugin_LTLIBRARIES = libanjuta-class-gen.la

AM_CPPFLAGS = \
	-I${top_srcdir} \
	$(WARN_CFLAGS) \
	$(DEPRECATED_FLAGS) \
	$(LIBANJUTA_CFLAGS)

libanjuta_class_gen_la_LDFLAGS = $(ANJUTA_PLUGIN_LDFLAGS)

libanjuta_class_gen_la_LIBADD = \
	$(GIO_LIBS) \
	$(LIBANJUTA_LIBS)

libanjuta_class_gen_la_SOURCES= \
	cell-renderer-flags.h \
	cell-renderer-flags.c \
	combo-flags.h \
	combo-flags.c \
	element-editor.c \
	element-editor.h \
	generator.c \
	generator.h \
	plugin.c \
	plugin.h \
	transform.c \
	transform.h \
	validator.c \
	validator.h \
	window.c \
	window.h
	
EXTRA_DIST = \
	$(plugin_in_files)

-include $(top_srcdir)/git.mk
