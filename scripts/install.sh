#!/bin/sh

dirn="$(dirname $0)"
installDir="$1"
if [ ! -f "${dirn}/toolen" ]; then
	echo "Cannot access Toolen, please make it first" >&2
	exit 1
fi

if [ -z "$installDir" ]; then
	echo "Usage: $(basename $0) <install directory>" >&2
	exit 1;
fi

echo "Install Toolen to directory: $installDir"
if [ ! -d "$installDir" ]; then
	mkdir "$installDir"
	if [ ! $? -eq 0 ]; then
		echo "Failed to create directory, please check your environment" >&2
		exit 1;
	fi
fi

cp "${dirn}/toolen" "$installDir"
for tool in $("${dirn}/toolen" -l); do
	ln -v -sf "./toolen" "$installDir/$tool"
	if [ ! $? -eq 0 ]; then
		echo "Failed to create symlink for '$tool'"
		exit 1;
	fi
done
