#!/bin/sh

x=$(ldd $SHELL|awk '$1~/^libc\./{print $3}')
echo ---------------$x----------------- >&2
[ -f "$x" ] && dirname "$x" || echo /usr/lib
