# Phase B SDK feasibility and safety notes

Investigated against official Release 1.5.1 uFBT SDK (API 88.2) and matching firmware source.

The public SDK exports the `lib/subghz/devices` abstraction used by the official Sub-GHz app. Phase B uses only initialization/name lookup, reset/idle/preset loading, frequency validation and setting, RX flush/mode, RSSI reads, and idle/sleep/end/deinitialization.

The internal `cc1101_int` device has no `begin` hook, so the wrapper returns `false`; the official application likewise calls `begin` and ignores that value. Frequency 433,920,000 Hz is validated before setting. The CC1101 rounds it to its synthesizer step (433,919,830 Hz on this SDK), and the returned actual frequency is accepted and saved. Direct receive mode is sufficient for periodic RSSI reads; asynchronous packet capture is not started.

No `set_tx`, async-TX, packet-write, or transmit function is referenced. Sampling runs on the ViewDispatcher thread. Completion, cancellation, acquisition failure, and application teardown use the same idempotent release path.

These functions appear in public SDK headers and the exported symbol table; they are not guessed private symbols. They remain low-level and firmware-coupled, so every future Release should be rebuilt and tested with matching firmware. Actual RF behavior, radio contention, and post-cancellation restoration require device testing.
