# `.rgs` save format

All integers are little-endian. Every file starts with a packed 16-byte envelope:

| Field | Type | Meaning |
| --- | --- | --- |
| magic | 4 bytes | ASCII `RGSF` |
| format_version | `uint16_t` | `1` (Phase A) or `2` (Phase B) |
| header_size | `uint16_t` | Currently `16` |
| payload_size | `uint32_t` | Exact payload byte count |
| payload_crc32 | `uint32_t` | IEEE CRC32 of payload |

Readers validate magic, known version, exact size, CRC32, sample bounds, source, and status. Unknown or malformed records are rejected.

## Version 1 compatibility

The 1,085-byte Phase A payload stays frozen: RTC date/time, source, sample count, 16 signed RSSI samples, min/max/average, seed, 25-byte ASCII place, reserved bytes, and a 1,024-byte seal. It is converted in memory and never rewritten automatically.

## Version 2

The packed 1,127-byte payload stores RTC date/time; source (`0` Demo, `1` Real RF); measurement status; valid/planned sample counts; interval in ms; frequency in Hz (zero for Demo); signed min/max/average; deterministic seed; 25-byte NUL-terminated place; 50 signed RSSI slots; and the 1,024-byte 128×64 seal.

Counts must not exceed 50. Completed saved observations use status `1`; values `0`, `2`, and `3` represent unknown, interrupted, and failed for forward use. The seed hashes place, timestamp, source, frequency, valid count, and all valid samples. Saving the bitmap preserves exact display if a future generator changes.
