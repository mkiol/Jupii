#!/bin/sh

aclocal
libtoolize --copy
automake --add-missing --copy
autoconf
