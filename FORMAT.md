# `.rgs` save format (version 1)

All integer fields are little-endian. The file begins with a packed 16-byte envelope:

| Field | Type | Meaning |
| --- | --- | --- |
| magic | 4 bytes | ASCII `RGSF` |
| format_version | `uint16_t` | Currently `1` |
| header_size | `uint16_t` | Envelope size; currently `16` |
| payload_size | `uint32_t` | Number of payload bytes |
| payload_crc32 | `uint32_t` | IEEE CRC32 of the payload |

The version 1 payload stores the RTC date/time, measurement source, sample count, sixteen signed RSSI samples, min/max/average RSSI, deterministic seed, a fixed-size ASCII place field, reserved bytes, and the 1024-byte 128×64 monochrome bitmap.

Readers must reject unknown versions they cannot parse and validate lengths and CRC before displaying a record. Future versions may add new measurement sources or payload fields while retaining the envelope.
