# `.rgs` save format

All integers are little-endian. Every file begins with the same packed 16-byte envelope:

| Offset | Field | Type | Meaning |
| ---: | --- | --- | --- |
| 0 | magic | 4 bytes | ASCII `RGSF` |
| 4 | format_version | `uint16_t` | `1`, `2`, or `3` |
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

The stored 1,024-byte bitmap remains the five-band composite seal. Five individual band seals are deterministically derived on demand from the v3 place, timestamp, algorithm version, selected band ID/frequencies, and its sixteen samples. They are not additional payload fields, so existing v3 records remain byte-compatible and gain the individual views without rewriting.
