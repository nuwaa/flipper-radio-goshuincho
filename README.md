# Radio Goshuincho / 電波御朱印帳

## 日本語

Radio Goshuincho（電波御朱印帳）は、Flipper Zero内蔵Sub-GHz受信機で観測したRSSI（受信信号強度）から、場所と日時に結び付いた128×64モノクロ印章を作る外部アプリです。Phase B v0.2.1は受信専用です。送信APIや信号源を特定する機能は実装していません。

### Phase Bの機能

- `Real RF`: 433.92 MHzを10秒間、200 ms間隔で50回観測
- 周波数、現在RSSI、経過時間、進捗、簡易グラフの表示
- Backによる安全な中断と、失敗時の明示（Demoへの自動切替なし）
- `Demo`: Phase Aと同様の決定的なダミーRSSI
- 場所、RTC日時、RSSI列、統計、周波数、測定種別から決定的な印章を生成
- CRC32付きmicroSD保存、一覧、再表示
- Phase A形式version 1の`.rgs`を読込み、新規データはversion 2で保存
- 同名レコードには連番を付け、既存ファイルを上書きしない

RSSIは受信帯域内の総合的な強度指標です。特定送信機の存在・識別、通信内容、または電波が存在しないことを判定するものではありません。

### ビルド

公式Releaseファームウェア、Python 3.8以上、公式[uFBT](https://github.com/flipperdevices/flipperzero-ufbt)、保存用microSDが必要です。

```sh
python3 -m venv .venv-ufbt
. .venv-ufbt/bin/activate
python3 -m pip install --upgrade ufbt
ufbt update --channel=release
ufbt
tests/run_host_tests.sh
```

FAPは`dist/radio_goshuincho.fap`に生成されます。USBなら`ufbt launch`、qFlipperならmicroSDの`/ext/apps/Tools/radio_goshuincho.fap`へコピーします。本体とSDKのRelease世代を合わせてください。

### 操作

1. `Real RF`または`Demo`を選び、英数字・空白・ハイフンで場所名を入力します。
2. Real RFでは10秒待ちます。Backで中断できます。完了後に`Create`を押すと生成・保存します。
3. Demoでは確定直後に生成・保存します。
4. 情報画面の`Seal`（OK）で印章と情報を切り替えます。
5. `Saved Stamps`では`R`（Real RF）/`D`（Demo）付き一覧から再表示します。

保存先は`/ext/apps_data/radio_goshuincho/`です。

### Phase B実機テスト手順（未実施）

このクラウド環境にはFlipper Zeroが接続されていないため、以下は実機で行う必要があります。

1. 公式Releaseファームウェアの本体とmicroSDへv0.2.1を導入し、`About`の`RX only`と版を確認します。
2. Demoの生成・保存・一覧・再表示を確認します。
3. Phase A v0.1.4の`.rgs`を残し、`Saved Stamps`から開けることを確認します。
4. Real RFで433.92 MHz、RSSI、時間、グラフ、進捗が更新されることを確認します。
5. 測定中にBackを押し、取消表示後に戻れることと、他のSub-GHzアプリが使えることを確認します。
6. Real RFを完了し、`Create`後に50 samples、印章、保存一覧を確認します。
7. microSD未装着/書込み不能でエラー表示を確認します。
8. `.rgs`を1 byte変更し、破損レコードが一覧から除外されることを確認します。
9. 無線リソースを利用できない状況でエラーになり、Demoへ自動移行しないことを確認します。

## English

Radio Goshuincho turns RSSI observations from Flipper Zero's built-in Sub-GHz receiver into a deterministic 128×64 travel seal tied to a place and time. Phase B v0.2.1 is receive-only: it has no transmit API calls and does not identify signal sources.

### Phase B features

- `Real RF`: observe 433.92 MHz for 10 seconds at 200 ms intervals (50 samples)
- Live frequency, RSSI, elapsed time, progress, and compact graph
- Safe Back cancellation and explicit errors; no silent Demo fallback
- Deterministic `Demo` mode, seal generation, and CRC32-protected microSD persistence
- Phase A version 1 `.rgs` compatibility; new extended version 2 records
- Collision suffixes prevent overwriting existing records

RSSI is an aggregate strength indication in the receiver bandwidth. It cannot prove the identity or presence of a transmitter, reveal content, or establish that no radio energy exists.

### Build and install

Use official Release firmware, Python 3.8+, official uFBT, and a microSD card:

```sh
python3 -m venv .venv-ufbt
. .venv-ufbt/bin/activate
python3 -m pip install --upgrade ufbt
ufbt update --channel=release
ufbt
tests/run_host_tests.sh
```

The output is `dist/radio_goshuincho.fap`. Install with `ufbt launch`, or copy it with qFlipper to `/ext/apps/Tools/radio_goshuincho.fap`. Match the device firmware and SDK Release generation. Records live in `/ext/apps_data/radio_goshuincho/`.

### Controls

Choose `Real RF` or `Demo` and enter a place. Real RF runs for ten seconds and can be cancelled with Back; press `Create` after completion. Demo generates immediately. Use `Seal`/OK to toggle the seal, and `Saved Stamps` to reopen `R` (real) or `D` (demo) records.

### Phase B device tests (not yet run)

No Flipper Zero is attached to this cloud environment. On matching official Release firmware, verify About/version, Demo save/reload, Phase A record loading, live Real RF progress, Back cancellation and later use by another Sub-GHz app, completed 50-sample save, microSD failures, corrupted-record rejection, and explicit radio-resource failure without Demo fallback.

## License

MIT. See [LICENSE](LICENSE).
