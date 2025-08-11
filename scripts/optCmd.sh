#!/bin/bash
# optCmd.sh - generate a Kconfig file

ROOT_DIR="$(dirname $0)/.."
GENDIR="${ROOT_DIR}/generated"

function generate() {
	dir="$1"
	output="$2"
	default="$3"

	echo > "$output"

	for cmd in $(find "$dir" -name "*.c"); do
		aCmd=$(basename "$cmd" | awk -F'.' '{print $1}')
		cat << EOF >> "$output"
config $aCmd
	bool "$aCmd"
	default $default
	help
		Command '$aCmd' support
EOF
done
}

# .config file is exist, exit
if [ -f ".config" ]; then
	exit 0
fi

if [ ! -d "$GENDIR" ]; then
	mkdir "${GENDIR}"
fi

# Generate for basic commands
basic_output="${GENDIR}/basic.conf"
generate "$ROOT_DIR/basic" "$basic_output" "y"

# Generate for file operations
file_output="${GENDIR}/file.conf"
generate "$ROOT_DIR/file" "$file_output" "y"

# Generate for system operations
sys_output="${GENDIR}/sys.conf"
generate "$ROOT_DIR/sys" "$sys_output" "y"

# Generate for string operations
string_output="${GENDIR}/string.conf"
generate "$ROOT_DIR/string" "$string_output" "y"

# Generate for other commands
others_output="${GENDIR}/others.conf"
generate "$ROOT_DIR/others" "$others_output" "y"

# Generate for tools in development
devel_output="${GENDIR}/devel.conf"
generate "$ROOT_DIR/devel" "$devel_output" "n"
