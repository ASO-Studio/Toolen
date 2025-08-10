#!/bin/bash
# allYes.sh - 

ROOT_DIR="$(dirname $0)/.."
GENDIR="${ROOT_DIR}/generated"

function generate() {
	dir="$1"
	output="$2"
	default="$3"
	delete="$4"

	# If it was already exist, then delete
	if [ -f "$output" ] && [ "$delete" = "true" ]; then
		rm -rf "$output"
	fi

	for cmd in $(find "$dir" -name "*.c"); do
		printf "$cmd " >> $output
	done
}

# .config file is exist, exit
if [ -f ".config" ]; then
	exit 0
fi

if [ ! -d "$GENDIR" ]; then
	mkdir "${GENDIR}"
fi

output="${GENDIR}/sources.mk"

printf "SOURCES=" > "$output"

# Generate for basic commands
generate "$ROOT_DIR/basic" "$output" "y" "false"

# Generate for file operations
generate "$ROOT_DIR/file" "$output" "y" "false"

# Generate for system operations
generate "$ROOT_DIR/sys" "$output" "y" "false"

# Generate for string operations
generate "$ROOT_DIR/string" "$output" "y" "false"

# Generate for other commands
generate "$ROOT_DIR/others" "$output" "y" "false"

printf "\nSOURCES += \$(shell find lib -name '*.c') \$(shell find main -name '*.c')\n\n" >> "$output"
