[+ autogen5 template +]
## Process this file with automake to produce Makefile.in

## Created by Anjuta

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\"

AM_CFLAGS =\
	 -Wall\
	 -g

bin_PROGRAMS = [+NameHLower+]

[+NameCLower+]_SOURCES = \
	main.c

[+NameCLower+]_LDFLAGS = 

[+NameCLower+]_LDADD = 
