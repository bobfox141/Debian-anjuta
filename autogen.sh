#!/usr/bin/bash
# Run this to generate all the initial makefiles, etc.
echo "[INFO] starting autogen setting source"
srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

echo "[INFO] Generating initial interface files"
bash -c "cd $srcdir/libanjuta/interfaces && \
perl anjuta-idl-compiler.pl libanjuta && \
touch iface-built.stamp"
echo "[INFO] testing srcdir implementation"
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.
(
 cd "$srcdir" &&
 gtkdocize &&
 echo "[INFO] gtkdocsized" &&
 autopoint --force && 
 echo "[INFO] autopoint" &&
 AUTOPOINT='intltoolize --automake --copy' autoreconf --force --install
) || exit
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
