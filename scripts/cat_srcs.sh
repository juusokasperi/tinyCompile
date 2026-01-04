#!/bin/sh

OUTPUT="all_sources.txt"

: > "$OUTPUT"

find . \
  -type d -name tests -prune -o \
  -type f \( -name "*.c" -o -name "*.h" -o -name "*.def" \) -print \
| sort | while IFS= read -r file
do
    echo "==================================================" >> "$OUTPUT"
    echo "FILE: $file" >> "$OUTPUT"
    echo "==================================================" >> "$OUTPUT"
    cat "$file" >> "$OUTPUT"
    echo "\n" >> "$OUTPUT"
done

echo "Done. Output written to $OUTPUT"
