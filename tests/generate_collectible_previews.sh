#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(dirname -- "$test_dir")
preview_dir="$repo_dir/previews/phase_e"
output_file="${TMPDIR:-/tmp}/radio_goshuincho_collectible_preview"

mkdir -p "$preview_dir"
cc -std=c11 -Wall -Wextra -Werror \
    -I"$test_dir/include" -I"$repo_dir" \
    -x c "$test_dir/collectible_preview.c.txt" "$repo_dir/goshuin_themes.c" \
    -o "$output_file"
"$output_file" "$preview_dir"

names="season_spring season_summer season_autumn season_winter rarity_travel rarity_quiet rarity_spark rarity_storm rarity_harmony"
for name in $names; do
    magick "$preview_dir/$name.pbm" -type bilevel -define png:bit-depth=1 \
        "$preview_dir/$name.png"
    magick "$preview_dir/$name.pbm" -filter point -resize 800% \
        -type bilevel -define png:bit-depth=1 "$preview_dir/${name}_8x.png"
done

magick montage \
    "$preview_dir/season_spring_8x.png" "$preview_dir/season_summer_8x.png" \
    "$preview_dir/season_autumn_8x.png" "$preview_dir/season_winter_8x.png" \
    -tile 2x -geometry +16+16 -type bilevel -define png:bit-depth=1 \
    "$preview_dir/seasons_comparison_8x.png"

magick montage \
    "$preview_dir/rarity_travel_8x.png" "$preview_dir/rarity_quiet_8x.png" \
    "$preview_dir/rarity_spark_8x.png" "$preview_dir/rarity_storm_8x.png" \
    "$preview_dir/rarity_harmony_8x.png" \
    -tile 2x -geometry +16+16 -type bilevel -define png:bit-depth=1 \
    "$preview_dir/rarities_comparison_8x.png"

echo "Collectible previews: $preview_dir"

