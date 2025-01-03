#!/usr/bin/env bash

# Check if a directory is provided as an argument
if [ -z "$1" ]; then
  echo "Usage: $0 <directory>"
  exit 1
fi

DIRECTORY=$1

# Function to format files using clang-format
format_files() {
  local file=$1

  # Format using clang-format
  clang-format -i "$file"

  sed -i 's|//\(.*\)|/* \1 */|' "$file"
}

export -f format_files

# Find and format all .c and .h files
find "$DIRECTORY" -type f \( -name "*.c" -o -name "*.h" \) -exec bash -c 'format_files "$0"' {} \;

echo "Formatting complete."
