NULL = 

SUBDIRS = 

INCLUDES =						\
	-DG_LOG_DOMAIN=\"gvfs-dav-mount\"	        \
	-DDATADIR=\"$(datadir)\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(WARN_CFLAGS)                                  \
	-Werror                                         \
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(GTK_CFLAGS)					\
	$(XML_CFLAGS)                                   \
	$(SOUP_CFLAGS)                                  \
	$(NULL)

bin_PROGRAMS = gvfs-dav-mount

gvfs_dav_mount_SOURCES = \
        gvfsdavmount.c   \
        $(NULL)

gvfs_dav_mount_LDADD = \
       $(GTK_LIBS)     \
       $(XML_LIBS)     \
       $(SOUP_LIBS)    \
       $(NULL)

EXTRA_DIST =                \
	intltool-extract.in \
	intltool-update.in  \
	intltool-merge.in   \
        $(NULL)

STCLEANFILES = \
	intltool-extract \
	intltool-update \
	intltool-merge \
        $(NULL)
