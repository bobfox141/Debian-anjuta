# Plugin UI files
python_plugin_uidir = $(anjuta_ui_dir)
dist_python_plugin_ui_DATA = anjuta-language-support-python.xml

# Plugin Glade file
python_plugin_gladedir = $(anjuta_glade_dir)
dist_python_plugin_glade_DATA = anjuta-language-support-python.ui

# Plugin Icon file
python_plugin_pixmapsdir = $(anjuta_image_dir)
dist_python_plugin_pixmaps_DATA = anjuta-language-support-python-plugin.png

# Plugin Python Scripts
scriptsdir = $(anjuta_plugin_dir)
dist_scripts_SCRIPTS = anjuta-python-autocomplete.py

# Plugin description file
plugin_in_files = anjuta-language-support-python.plugin.in
%.plugin: %.plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

python_plugin_plugindir = $(anjuta_plugin_dir)
dist_python_plugin_plugin_DATA = $(plugin_in_files:.plugin.in=.plugin)

AM_CPPFLAGS = \
	$(XML_CFLAGS) \
	$(LIBANJUTA_CFLAGS) \
	-DSCRIPTS_DIR=\"$(scriptsdir)\" \
	-DG_LOG_DOMAIN=\"language-support-python\"

# Where to install the plugin
plugindir = $(anjuta_plugin_dir)

# The plugin
plugin_LTLIBRARIES = libanjuta-language-support-python.la

# Plugin sources
libanjuta_language_support_python_la_SOURCES = plugin.c plugin.h python-assist.c python-assist.h 

libanjuta_language_support_python_la_LDFLAGS = $(ANJUTA_PLUGIN_LDFLAGS)

# Plugin dependencies
libanjuta_language_support_python_la_LIBADD = \
	$(LIBANJUTA_LIBS) 

gsettings_in_file = org.gnome.anjuta.plugins.python.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_in_file:.xml.in=.xml)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(plugin_in_files) \
        $(gsettings_in_file)

CLEANFILES = \
        $(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
