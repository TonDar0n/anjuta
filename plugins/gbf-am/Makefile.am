# Plugin UI file
plugin_uidir = $(anjuta_ui_dir)
plugin_ui_DATA = 

# Plugin glade file
plugin_gladedir = $(anjuta_glade_dir)
plugin_glade_DATA = gbf-am-dialogs.ui

# Plugin icon file
plugin_pixmapsdir = $(anjuta_image_dir)
plugin_pixmaps_DATA = gbf-am-plugin-48.png

# Plugin scripts
scriptsdir = $(bindir)
scripts_SCRIPTS = gbf-am-parse

# Plugin description file
plugin_in_files = gbf-am.plugin.in
%.plugin: %.plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugindir = $(anjuta_plugin_dir)
plugin_DATA = $(plugin_in_files:.plugin.in=.plugin)

AM_CPPFLAGS = 					\
	$(WARN_CFLAGS) \
	$(DEPRECATED_FLAGS) \
	$(GIO_CFLAGS) \
	$(LIBANJUTA_CFLAGS) \
	-DSCRIPTS_DIR=\"$(scriptsdir)\"
	-DG_LOG_DOMAIN=\"libgbf-am\"

plugin_LTLIBRARIES = \
	libgbf-am.la

libgbf_am_la_SOURCES = \
	plugin.c \
	plugin.h \
	gbf-am-project.c \
	gbf-am-project.h \
	gbf-am-config.c \
	gbf-am-config.h \
	gbf-am-properties.c \
	gbf-am-properties.h 

libgbf_am_la_LDFLAGS = $(ANJUTA_PLUGIN_LDFLAGS)

libgbf_am_la_LIBADD = \
	$(GIO_LIBS) \
	$(LIBANJUTA_LIBS) \
  $(LIBXML_LIBS)

# Test program

noinst_PROGRAMS = test

test_SOURCES = \
	test.c \
	gbf-am-project.c \
	gbf-am-project.h \
	gbf-am-config.c \
	gbf-am-properties.c

test_LDADD = \
	$(LIBANJUTA_LIBS) \
	$(LIBXML_LIBS)
# This last line do nothing but it is needed to avoid the error
# gbf-am-*.o created with both libtool and without
# Need AM_PROG_CC_C_0 in configure.in too
test_CFLAGS = $(AM_CFLAGS)

EXTRA_DIST = \
	$(plugin_in_files) \
	$(plugin_DATA) \
	$(plugin_ui_DATA) \
	$(plugin_pixmaps_DATA) \
	$(plugin_glade_DATA)

DISTCLEANFILES = \
	$(plugin_DATA)
SUBDIRS = GBF


-include $(top_srcdir)/git.mk
