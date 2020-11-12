#!/bin/bash
# Quick & dirty hack to turn Mallard help pages into HTML on win32.

APP_ID=$1

MSYSTEM=MSYS2 . /etc/profile

while read -r lang
do
	if [[ ${lang:0:1} != "#" && ${lang:0:1} != " " ]]
	then
		dest="${MESON_INSTALL_PREFIX}"/share/help/$lang/$APP_ID
		echo "Converting help for $lang to HTML ..."
		(cd "$dest" && yelp-build html -o . .) || exit 1
		rm "$dest"/*.page
	fi
done < "${MESON_SOURCE_ROOT}"/help/LINGUAS
