# Phase C SDK and receive-only design notes

Checked against official Release 1.5.1 uFBT SDK, API 88.2. The public `lib/subghz/devices` functions used in Phase B remain exported: registry init/deinit, internal-device lookup, begin/end, reset/idle/sleep, preset loading, frequency validation/setting, RX flush/mode, and RSSI reads.

Phase C never calls async RX because periodic RSSI reads only require direct RX mode. Band switching follows: idle, validate requested Hz, load `FuriHalSubGhzPresetOok650Async`, set frequency, validate the returned actual Hz and a 1 kHz maximum quantization error, flush RX, then enter RX. Completion, Back cancellation, errors, and teardown share the idempotent release path.

The CC1101 formula in the matching official source predicts these actual frequencies; hardware verification remains required:

| Requested Hz | Predicted actual Hz | Difference |
| ---: | ---: | ---: |
| 315,000,000 | 314,999,664 | 336 Hz |
| 426,075,000 | 426,074,920 | 80 Hz |
| 429,350,000 | 429,349,914 | 86 Hz |
| 433,920,000 | 433,919,830 | 170 Hz |
| 922,500,000 | 922,499,725 | 275 Hz |

No TX mode, async TX, packet write, transmitter, replay, or protocol decoder is used. The OOK preset defines the receiver filter configuration; RSSI values do not identify a modulation or source. Public low-level APIs remain firmware-coupled, so every Release must be rebuilt and tested on matching firmware.

Individual band seals are a display/generation feature only. They use the already stored RSSI values and do not perform additional RF access when browsing a record.
