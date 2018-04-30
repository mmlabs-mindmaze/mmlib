#!/bin/bash

if [ "$(git rev-parse --show-toplevel)" != "$PWD" ]
then
	echo "go to top dir"
	echo "git clean -dfx && ./autogen.sh && ./configure && make"
	exit 1
fi

FILES=$(sed -e 's/.. kernel-doc::\(.*\)/\1/;t;d' doc/*.rst)
expected=$(cat $FILES | grep -c API_EXPORTED)
html_exported=$(cat doc/html/*.html | grep -c "class=\"function\"" )
man_pages=$(ls doc/man/*.3 | wc -l)

{
	echo "Expected $expected API functions"
	echo "Found $html_exported html functions"
	echo "Found $man_pages man pages"
} 1>&2
