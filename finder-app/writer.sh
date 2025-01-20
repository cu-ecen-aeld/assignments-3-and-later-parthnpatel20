#!/bin/bash

# Check that there are 2 arguments
if [ $# -ne 2 ]; then
    echo "Error: Two arguments required - <file path> and <text string>"
    exit 1
fi

writefile=$1
writestr=$2


# Create the directory path if its not exists
mkdir -p "$(dirname "$writefile")"

# Write the text string to the file
echo "$writestr" > "$writefile"

# Check if the file is created
if [ $? -ne 0 ]; then
    echo "Error: Could not create file $writefile"
    exit 1
fi

echo "File created successfully: $writefile"

