# Phase B implementation plan and verification map

1. Preserve Phase A behavior and freeze its version 1 binary layout.
2. Add a version 2 canonical model for 50 samples, frequency, interval, source, status, statistics, seed, and seal.
3. Isolate deterministic Demo/Real seal generation and version conversion so they can be host-tested.
4. Add explicit Real RF and Demo flows. Run 200 ms RSSI sampling on the UI dispatcher for exactly 50 samples.
5. Acquire the internal CC1101 through the public SDK abstraction, enter RX only, and use one idempotent cleanup path for success, cancellation, failure, and teardown.
6. Extend observation/status UI, preserve microSD browsing, reject malformed records, and avoid overwrites.
7. Run host tests, uFBT lint/build, artifact inspection, and a transmit-symbol audit. Leave RF and device recovery checks to the documented hardware procedure.

Cloud-verifiable items: compilation, API imports, deterministic generation, flat/changing sample handling, v1 conversion, size/version rejection, CRC32, and absence of TX symbol references.

Device-only items: actual RSSI values, graph/timing behavior on hardware, Back cancellation under use, radio-resource contention, microSD hardware failures, and radio availability to other apps after exit.
