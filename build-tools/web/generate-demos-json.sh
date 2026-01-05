#!/bin/bash
# Generate demos.json from demo/layouts directory

DEMO_DIR="${1:-demo/layouts}"
OUTPUT="${2:-demos.json}"

echo "[" > "$OUTPUT"

first=true
for dir in "$DEMO_DIR"/*/; do
    if [ -d "$dir" ]; then
        name=$(basename "$dir")
        # Skip directories that start with . or _
        if [[ "$name" == .* ]] || [[ "$name" == _* ]]; then
            continue
        fi
        # Check if app.yaml exists
        if [ ! -f "$dir/app.yaml" ]; then
            continue
        fi

        if [ "$first" = true ]; then
            first=false
        else
            echo "," >> "$OUTPUT"
        fi

        # Create a nice display name (capitalize first letter of each word)
        display_name=$(echo "$name" | sed 's/-/ /g' | awk '{for(i=1;i<=NF;i++) $i=toupper(substr($i,1,1)) tolower(substr($i,2))}1')

        printf '  {"name": "%s", "path": "%s"}' "$display_name" "$name" >> "$OUTPUT"
    fi
done

echo "" >> "$OUTPUT"
echo "]" >> "$OUTPUT"

echo "Generated $OUTPUT with demos from $DEMO_DIR"
