# Plugin glade file
cpp_java_gladedir = $(anjuta_glade_dir)
dist_cpp_java_glade_DATA = anjuta-language-cpp-java.ui

# Plugin UI file
cpp_java_uidir = $(anjuta_ui_dir)
dist_cpp_java_ui_DATA = anjuta-language-support-cpp-java.xml

# Plugin Icon file
cpp_java_pixmapsdir = $(anjuta_image_dir)
dist_cpp_java_pixmaps_DATA = \
 	anjuta-language-cpp-java-plugin.png \
	anjuta-indent-auto-16.png \
	anjuta-indent-auto-24.png

# Plugin description file
plugin_in_files = anjuta-language-cpp-java.plugin.in
%.plugin: %.plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

cpp_java_plugindir = $(anjuta_plugin_dir)
dist_cpp_java_plugin_DATA = $(plugin_in_files:.plugin.in=.plugin)

# NOTE :
# The naming convention is very intentional
# We are forced to use the prefix 'lib' by automake and libtool
#    There is probably a way to avoid it but it is not worth to effort
#    to find out.
# The 'anjuta_' prfix is a safety measure to avoid conflicts where the
#    plugin 'libpython.so' needs to link with the real 'libpython.so'

# Include paths
AM_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(DEPRECATED_FLAGS) \
	$(LIBANJUTA_CFLAGS) \
	$(EXTRA_CFLAGS) \
	-DG_LOG_DOMAIN=\"language-support-cpp-java\"

# Where to install the plugin
plugindir = $(anjuta_plugin_dir)

# The plugin
plugin_LTLIBRARIES = libanjuta-language-cpp-java.la

# Plugin sources
libanjuta_language_cpp_java_la_SOURCES = \
	plugin.c \
	plugin.h \
	cpp-packages.c \
	cpp-packages.h

libanjuta_language_cpp_java_la_LDFLAGS = $(ANJUTA_PLUGIN_LDFLAGS)

# Plugin dependencies
libanjuta_language_cpp_java_la_LIBADD = \
	$(LIBANJUTA_LIBS)
	
gsettings_in_file = org.gnome.anjuta.plugins.cpp.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_in_file:.xml.in=.xml)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@


EXTRA_DIST = \
	$(plugin_in_files) \
        $(gsettings_in_file)

CLEANFILES = \
        $(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
