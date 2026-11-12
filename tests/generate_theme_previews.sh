#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(dirname -- "$test_dir")
preview_dir="$repo_dir/previews/phase_d"
output_file="${TMPDIR:-/tmp}/radio_goshuincho_theme_preview"

mkdir -p "$preview_dir"
cc -std=c11 -Wall -Wextra -Werror \
    -I"$test_dir/include" -I"$repo_dir" \
    -x c "$test_dir/theme_preview.c.txt" "$repo_dir/goshuin_themes.c" \
    -o "$output_file"
"$output_file" "$preview_dir"

for name in demo 315mhz 426mhz 429mhz 433mhz 920mhz 5bands; do
    magick "$preview_dir/$name.pbm" -type bilevel -define png:bit-depth=1 \
        "$preview_dir/$name.png"
    magick "$preview_dir/$name.pbm" -filter point -resize 800% \
        -type bilevel -define png:bit-depth=1 "$preview_dir/${name}_8x.png"
done

magick montage \
    "$preview_dir/demo_8x.png" "$preview_dir/315mhz_8x.png" \
    "$preview_dir/426mhz_8x.png" "$preview_dir/429mhz_8x.png" \
    "$preview_dir/433mhz_8x.png" "$preview_dir/920mhz_8x.png" \
    "$preview_dir/5bands_8x.png" \
    -tile 2x -geometry +16+16 -type bilevel -define png:bit-depth=1 \
    "$preview_dir/themes_comparison_8x.png"

echo "Theme previews: $preview_dir"
