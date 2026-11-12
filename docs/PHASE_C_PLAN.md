# Phase C implementation and verification plan

1. Keep the v0.2.1 Real RF and Demo flows and freeze v1/v2 binary layouts.
2. Add an independent 1,244-byte v3 payload for five bands and strict conversion into a common display record.
3. Acquire the internal CC1101 once, then sequentially switch RX through five validated frequencies. Use a 50 ms dispatcher tick: one settle tick and three peak reads per 150 ms window, sixteen windows per band.
4. Generate a deterministic five-compartment composite seal from all 80 samples, plus an independent full-screen seal for each band. Individual seals are derived on demand so the v3 layout and stored composite remain unchanged.
5. Save only fully completed observations with `Create+Save`; preserve new-file-only collision handling and CRC32.
6. Add v3 list/view UI, per-band pages, host malformed-data tests, Release uFBT build, and TX symbol audits.

Cloud checks cover structure sizes, v1/v2 regression, v3 round trips and validation, deterministic/changed/flat/reordered inputs, CRC changes, compilation, imports, and documentation. Physical RSSI, exact timing under device load, radio contention/recovery, antenna differences, pulse capture, and microSD hardware failures remain device-only checks.
