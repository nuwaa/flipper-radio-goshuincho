# Phase D design and verification notes

## Deterministic feature model

The renderer scans each band's signed RSSI sequence once and stores only eight small features: minimum, maximum, integer average, end-minus-start trend, range, local peak count, count at or above `average + range / 4`, and direction-turn count for changes larger than 2 dB. No heap allocation is used by the renderer.

Average RSSI is mapped to five buckets after clamping to -120..-40 dBm. This bucket changes details inside that band's motif only. The integrated design does not rank bands or compare their absolute RSSI values; each of its five crests is parameterized independently. Range, peaks, relative-threshold count, turns, and trend carry most of the observation-specific variation.

| Theme | Stable motif | Observation-driven details |
| --- | --- | --- |
| Demo | double square seal, central diamonds | diamond layers, center radius, paired marks |
| 315 MHz | keyhole, concentric ripples | ring count, aperture radius, side-wave count |
| 426 MHz | control grids, antenna tower | panel level, radio rays, center diamond |
| 429 MHz | gear, hexagons, industrial diagonals | gear radius/teeth, hub radius, tooth orientation |
| 433 MHz | wheel and three flowing tracks | wheel radius/spokes, curve direction and bends |
| 920 MHz | constellation/network | node count/type, cross-links, hub radius |
| 5 Bands | circular five-crest composition | independent crest radii/directions and seeded center crest |

Place, timestamp, theme/design version, selected band identity, actual frequency, and RSSI samples feed an FNV-1a seed. The integrated theme hashes all five bands; a band-specific theme hashes that selected band. Integer-only drawing and a fixed xorshift-compatible seed policy make output bit-for-bit reproducible.

## Compatibility boundary

The v1 and v2 readers display stored bitmaps without regeneration. The complete v3 generator and strict decoder remain unchanged. New five-band designs use the separate 1,248-byte v4 payload, whose decoder validates metadata, per-band statistics, seed, and regenerated bitmap. No reader migrates, overwrites, or deletes an older record.

## Cloud verification matrix

`tests/run_host_tests.sh` covers:

- weak, nearly flat samples;
- strong, nearly flat samples;
- multiple short peaks;
- large alternating variation;
- five bands with different trends;
- repeat generation producing identical bytes;
- pairwise theme differences and non-empty/limited-density bitmaps;
- v1/v2 stored-bitmap preservation and the frozen v3 strict validation path;
- v4 round trip plus size, reserved-byte, statistics, theme, and bitmap corruption rejection.

`tests/generate_theme_previews.sh` uses those observation characteristics across its five input bands and emits exact 128×64 one-bit PBM/PNG images plus nearest-neighbor 8× images. Device-only verification remains necessary for UI controls, microSD failures, actual RF observations, cancellation, and radio-resource restoration.
