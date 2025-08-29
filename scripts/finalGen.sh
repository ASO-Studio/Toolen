#!/bin/bash

# finalGen.sh - Convert config file to sources list

CONFIG_FILE="$1"
OUTPUT_FILE="$2"

echo "It will take some seconds to finish something..."

# Check config file is exsit
if [ ! -f "$CONFIG_FILE" ]; then
	echo "Error: Configuration file $CONFIG_FILE not found!"
	exit 1
fi

# Create output file
echo "# Automatically generated from $CONFIG_FILE" > $OUTPUT_FILE
echo "# DO NOT EDIT MANUALLY" >> $OUTPUT_FILE
echo "" >> $OUTPUT_FILE

# Get all enabled config
ENABLED_CONFIGS=$(grep -E '^[[:space:]]*CONFIG_[A-Za-z0-9_]+[[:space:]]*=[[:space:]]*y' $CONFIG_FILE)

# Add sources list
echo "SOURCES = \\" >> $OUTPUT_FILE
echo "$ENABLED_CONFIGS" | while read line; do
	var=$(echo "$line" | cut -d '=' -f 1 | sed 's/[[:space:]]//g')
	module=$(echo "$var" | sed 's/^CONFIG_//')
	source_file=$(echo "$module" | tr '[:upper:]' '[:lower:]').c
	echo "	$(find -name "$source_file") \\" >> $OUTPUT_FILE
done

sed -i '$ d' $OUTPUT_FILE
echo "" >> $OUTPUT_FILE

printf "SOURCES += \$(shell find lib -name '*.c') \$(shell find main -name '*.c')\n\n" >> $OUTPUT_FILE
