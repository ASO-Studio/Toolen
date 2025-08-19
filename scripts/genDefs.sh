#!/bin/bash

# Input configuration file
CONFIG_FILE="$1"
# Output makefile
OUTPUT_FILE="$2"

# Check if .config file exists
if [ ! -f "$CONFIG_FILE" ]; then
	echo "Error: Configuration file $CONFIG_FILE not found!" >&2
	exit 1
fi

# Clear or create the output file
> "$OUTPUT_FILE"

# Process enabled configuration items (CONFIG_DEF_xxx=y or CONFIG_DEF_xxx=value)
grep -E '^CONFIG_DEF_[a-zA-Z0-9_]+=' "$CONFIG_FILE" | while IFS='=' read -r key value; do
	# Remove CONFIG_DEF_ prefix
	clean_key=${key#CONFIG_DEF_}
	# Remove quotes from the value
	clean_value=$(echo "$value" | tr -d '"')
	
	# Convert y/n to 1/0
	if [ "$clean_value" = "y" ]; then
		clean_value="1"
	elif [ "$clean_value" = "n" ]; then
		clean_value="0"
	fi
	
	# Format as CFLAGS assignment and append to output file
	echo "C_FLAGS += -D${clean_key}=${clean_value}" >> "$OUTPUT_FILE"
done

# Process disabled configuration items (# CONFIG_DEF_xxx is not set)
grep -E '^# CONFIG_DEF_[a-zA-Z0-9_]+ is not set$' "$CONFIG_FILE" | while read -r line; do
	# Extract the configuration name
	full_key=$(echo "$line" | awk '{print $2}')
	# Remove CONFIG_DEF_ prefix
	clean_key=${full_key#CONFIG_DEF_}
	# Format as CFLAGS assignment with value 0
	echo "C_FLAGS += -D${clean_key}=0" >> "$OUTPUT_FILE"
done

