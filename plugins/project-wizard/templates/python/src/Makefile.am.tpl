[+ autogen5 template +]
## Process this file with automake to produce Makefile.in
## Created by Anjuta

## The main script
bin_SCRIPTS = [+NameHLower+].py

## Directory where .class files will be installed
[+NameCLower+]dir = $(pythondir)/[+NameHLower+]

[+NameCLower+]_PYTHON = \
	[+NameHLower+].py
