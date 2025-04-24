[+ autogen5 template +]
## Process this file with automake to produce Makefile.in

## Created by Anjuta

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\"[+IF (=(get "HavePackage") "1")+] \
	$([+NameCUpper+]_CFLAGS)[+ENDIF+]

AM_CFLAGS =\
	 -Wall\
	 -g

bin_PROGRAMS = [+NameHLower+]

[+NameCLower+]_SOURCES = \
	main.cc

[+NameCLower+]_LDFLAGS = 

[+NameCLower+]_LDADD = [+IF (=(get "HavePackage") "1")+]$([+NameCUpper+]_LIBS)[+ENDIF+]

[+IF (=(get "HaveWindowsSupport") "1")+]
if NATIVE_WIN32
[+NameCLower+]_LDFLAGS += -mwindows
endif[+
ENDIF+]
