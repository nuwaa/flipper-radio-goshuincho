# `.rgs` save format

All integers are little-endian. Every file begins with the same packed 16-byte envelope:

| Offset | Field | Type | Meaning |
| ---: | --- | --- | --- |
| 0 | magic | 4 bytes | ASCII `RGSF` |
| 4 | format_version | `uint16_t` | `1`, `2`, `3`, `4`, or `5` |
| 6 | header_size | `uint16_t` | Must be `16` |
| 8 | payload_size | `uint32_t` | Exact payload size for the version |
| 12 | payload_crc32 | `uint32_t` | IEEE CRC32 of payload |

Readers validate magic, known version, exact size, CRC32, and version-specific field ranges before display. Unknown or malformed records are rejected.

## Frozen compatibility formats

- Version 1: exactly 1,085 bytes. Phase A RTC/source/16 RSSI/statistics/seed/place/reserved/1,024-byte bitmap layout.
- Version 2: exactly 1,127 bytes. Phase B RTC/source/status/counts/interval/frequency/statistics/seed/place/50 RSSI slots/bitmap layout.

These layouts and their packed C structures are frozen. They are read into a display model; files and saved bitmaps are never rewritten or regenerated.

## Version 3 payload

`GoshuinStampPayloadV3` is packed and exactly 1,244 bytes:

| Offset | Field | Size |
| ---: | --- | ---: |
| 0 | year, month, day, hour, minute, second | 7 |
| 7 | measurement_source (`2` = MultiBandReal) | 1 |
| 8 | measurement_status (`1` = Completed) | 1 |
| 9 | band_count (`5`) | 1 |
| 10 | algorithm_version (`1`) | 1 |
| 11 | samples_per_band (`16`) | 1 |
| 12 | preset_id (`1` = OOK 650 kHz async preset used for RSSI) | 1 |
| 13 | observation_window_ms (`150`) | 2 |
| 15 | settle_time_ms (`50`) | 2 |
| 17 | peak_read_interval_ms (`50`) | 2 |
| 19 | reserved0 (zero) | 1 |
| 20 | five `GoshuinBandPayloadV3` records | 170 |
| 190 | deterministic seed | 4 |
| 194 | NUL-terminated ASCII place | 25 |
| 219 | reserved1 (zero) | 1 |
| 220 | 128×64 1bpp seal bitmap | 1,024 |

Each packed band record is exactly 34 bytes:

| Offset | Field | Size |
| ---: | --- | ---: |
| 0 | band_id (`315`, `426`, `429`, `433`, or `920`) | 2 |
| 2 | valid_count (`16`) | 1 |
| 3 | reserved0 (zero) | 1 |
| 4 | valid_mask (`0xFFFF`) | 2 |
| 6 | requested_frequency_hz | 4 |
| 10 | actual_frequency_hz returned by the CC1101 API | 4 |
| 14 | rssi_min, rssi_max, rssi_average | 3 |
| 17 | reserved1 (zero) | 1 |
| 18 | sixteen signed RSSI peak samples | 16 |

The average is signed integer division truncated toward zero. Decoding rejects wrong counts, masks, timings, source/status, algorithm/preset, unknown or duplicate band IDs, mismatched requested frequencies, invalid actual frequencies, frequency error over 1 kHz, inconsistent statistics, seed, or bitmap. The seed hashes place, timestamp, algorithm version, and every band ID/requested Hz/actual Hz/sample array in stored order.

Version 3 is frozen. Its original five-graph generator and strict regenerate-and-compare decoder remain in the application so existing v3 records continue to validate exactly.

## Version 4 themed payload

`GoshuinStampPayloadV4` is packed and exactly 1,248 bytes. It stores the same five completed band records as v3, but uses a separately versioned design generator:

| Offset | Field | Size |
| ---: | --- | ---: |
| 0 | year, month, day, hour, minute, second | 7 |
| 7 | measurement_source (`3` = themed multi-band observation) | 1 |
| 8 | measurement_status (`1` = Completed) | 1 |
| 9 | band_count (`5`) | 1 |
| 10 | theme_id | 1 |
| 11 | design_algorithm_version (`1`) | 1 |
| 12 | samples_per_band (`16`) | 1 |
| 13 | preset_id (`1`) | 1 |
| 14 | observation_window_ms | 2 |
| 16 | settle_time_ms | 2 |
| 18 | peak_read_interval_ms | 2 |
| 20 | reserved0 (two zero bytes) | 2 |
| 22 | five unchanged 34-byte band records | 170 |
| 192 | deterministic design seed | 4 |
| 196 | NUL-terminated ASCII place | 25 |
| 221 | reserved1 (three zero bytes) | 3 |
| 224 | 128×64 1bpp seal bitmap | 1,024 |

Theme IDs are `1` integrated 5 Bands, `2` 315 MHz, `3` 426 MHz, `4` 429 MHz, `5` 433 MHz, and `6` 920 MHz. Demo uses theme ID `0` in the common renderer but is stored in the existing v2 Demo record, not v4.

For the integrated theme, the seed hashes place, timestamp, theme/design versions, and all five band IDs, actual frequencies, and sample arrays. A band-specific theme hashes only its selected band's ID, actual frequency, and samples in addition to the shared metadata. The v4 decoder validates all band metadata and statistics using the frozen v3 rules, recomputes the theme seed and bitmap, and requires an exact match. Reserved bytes must be zero. A v4 write never migrates or overwrites v1/v2/v3 files.

## Version 5 collectible payload

Version 5 is also exactly 1,248 bytes. It preserves all v4 offsets except that bytes 20 and 21 are now meaningful and the design algorithm is version 2:

| Offset | Field | Size |
| ---: | --- | ---: |
| 0–19 | RTC, source/status, theme/design, sample/preset and timing fields | 20 |
| 20 | season_id (`1` spring, `2` summer, `3` autumn, `4` winter) | 1 |
| 21 | rarity_id (`0` travel, `1` quiet, `2` spark, `3` storm, `4` five harmony) | 1 |
| 22 | five unchanged 34-byte band records | 170 |
| 192 | deterministic collectible seed | 4 |
| 196 | NUL-terminated ASCII place | 25 |
| 221 | reserved1 (three zero bytes) | 3 |
| 224 | themed, seasonal 128×64 1bpp bitmap | 1,024 |

`measurement_source` is `4` and `design_algorithm_version` is `2`. Season is derived from the stored month. Rarity is recomputed from the five band-local feature sets; it never ranks absolute RSSI across bands. Both IDs are included in the seed and verified on read. The decoder also applies all v3 band/statistics validation and requires exact regenerated bitmap equality. Versions 1–4 remain immutable and are never migrated.
