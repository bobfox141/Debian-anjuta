# Plugin glade file
file_manager_gladedir = $(anjuta_glade_dir)
dist_file_manager_glade_DATA = file-manager.ui

# Plugin UI file
file_manager_uidir = $(anjuta_ui_dir)
dist_file_manager_ui_DATA = file-manager.xml

# Plugin Icon file
file_manager_pixmapsdir = $(anjuta_image_dir)
dist_file_manager_pixmaps_DATA = \
	anjuta-file-manager-plugin.svg \
	anjuta-file-manager-plugin-48.png \
	anjuta-file-manager-plugin-16.png \
	vcs-added.png \
	vcs-conflict.png \
	vcs-deleted.png \
	vcs-ignored.png \
	vcs-modified.png \
	vcs-unversioned.png \
	vcs-updated.png

# Plugin description file
plugin_in_files = file-manager.plugin.in
%.plugin: %.plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

file_manager_plugindir = $(anjuta_plugin_dir)
dist_file_manager_plugin_DATA = $(plugin_in_files:.plugin.in=.plugin)

# Include paths
AM_CPPFLAGS = \
	$(LIBANJUTA_CFLAGS) \
	$(GDL_CFLAGS) \
	$(GIO_CFLAGS)

# Where to install the plugin
plugindir = $(anjuta_plugin_dir)

# The plugin
plugin_LTLIBRARIES = libfile-manager.la

BUILT_SOURCES = file-view-marshal.h file-view-marshal.c

# Plugin sources
libfile_manager_la_SOURCES = \
	plugin.c \
	plugin.h \
	file-view-marshal.h \
	file-view-marshal.c \
	file-view.h \
	file-view.c \
	file-model.c \
	file-model.h

libfile_manager_la_LDFLAGS = $(ANJUTA_PLUGIN_LDFLAGS)

# Plugin dependencies
libfile_manager_la_LIBADD = \
	$(LIBANJUTA_LIBS) \
	$(GDL_LIBS) \
	$(GIO_LIBS)

file-view-marshal.h: $(srcdir)/file-view-marshal.list
	$(AM_V_GEN)if $(GLIB_GENMARSHAL) --prefix=file_view_cclosure_marshal \
	   --header $(srcdir)/file-view-marshal.list >.genmarshal-$@; \
	then mv -f .genmarshal-$@ $@; \
	else rm -f .genmarshal-$@; fi

file-view-marshal.c: $(srcdir)/file-view-marshal.list
	$(AM_V_GEN)if { echo '#include "file-view-marshal.h"' && $(GLIB_GENMARSHAL) \
	     --prefix=file_view_cclosure_marshal --body $(srcdir)/file-view-marshal.list; \
	   } >.genmarshal-$@; \
	then mv -f .genmarshal-$@ $@; \
	else rm -f .genmarshal-$@; fi

gsettings_in_file = org.gnome.anjuta.file-manager.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_in_file:.xml.in=.xml)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(plugin_in_files) \
	file-view-marshal.list \
        $(gsettings_in_file)

CLEANFILES = \
        $(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
