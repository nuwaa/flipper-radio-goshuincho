# Phase E collection design

## Goshuin Book

`Goshuin Book` scans the same validated `.rgs` files as `Saved Stamps`, newest first. It downsamples each stored 128×64 bitmap to 64×32 using a 2×2 logical OR, preserving thin one-pixel strokes. Left and Right wrap through the collection; OK opens the original record and Back returns to the same book page. The reader accepts v1 through v5 and never rewrites a file.

## Seasonal accents

The stored RTC month deterministically selects one accent:

| Months | Season | Accent |
| --- | --- | --- |
| March–May | Spring | paired blossom diamonds |
| June–August | Summer | opposing wave crests |
| September–November | Autumn | moon and falling leaves |
| December–February | Winter | corner snowflakes |

Accents stay near the margins so the Phase D central crest remains readable. New Demo and Real RF bitmaps receive the accent while retaining their frozen v2 container. Five-band saves use v5 and explicitly record the season ID.

## Collectible titles

Classification uses features extracted independently inside each band. It does not rank absolute RSSI between bands.

| ID | Title | Deterministic condition |
| ---: | --- | --- |
| 0 | Travel Seal | no special condition |
| 1 | Quiet Seal | every band range is at most 4 dB |
| 2 | Spark Seal | total local peaks are at least twice the band count |
| 3 | Storm Seal | maximum range is at least 45 dB or total direction turns are at least 30 |
| 4 | Five Harmony | five-band input contains at least four distinct shape signatures |

Five Harmony takes precedence, followed by Quiet, Storm, Spark, and Travel. Shape signatures distinguish quiet, peak-heavy, rapidly alternating, rising, falling, and other sequences. The title is an artistic description of the sample shape, not a statement about a transmitter or protocol.

## Compatibility and validation

The v5 payload retains the v4 size and common offsets but assigns bytes 20 and 21 to season and rarity. It uses source ID 4 and design algorithm 2. Decoding validates:

- exact envelope size and CRC32;
- RTC-derived season;
- rarity recomputed from all five stored sample arrays;
- v3-compatible band order, frequencies, masks, counts, timing, and statistics;
- deterministic seed including season and rarity;
- exact regenerated bitmap.

The v1–v4 decoders and generators are unchanged. No automatic migration, overwrite, or deletion is performed.

## Verification

`tests/run_host_tests.sh` covers all four seasons, all five rarity classes, deterministic v5 round trips, corrupted season/rarity/bitmap rejection, and the existing v1–v4 regression suite. `tests/generate_collectible_previews.sh` emits exact one-bit 128×64 images and 8× comparison sheets. Flipper Zero hardware is still required to verify book controls, microSD failures, real RF results, and radio-resource restoration.
