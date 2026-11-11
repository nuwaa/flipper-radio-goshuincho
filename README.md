# Radio Goshuincho / 電波御朱印帳

## 日本語

Radio Goshuincho（電波御朱印帳）は、旅先で観測した電波の特徴から、その場所と日時だけのモノクロ印章を作るFlipper Zero向け外部アプリです。本リポジトリのPhase Aは、実際のSub-GHz受信の代わりに再現可能なダミーRSSI列を使用します。

### Phase Aでできること

- 128×64モノクロ画面向けのメインメニュー
- 英数字・空白・ハイフンによる場所名入力
- 場所名とRTC日時から16点のダミーRSSI測定値を生成
- 場所・日時・測定値から決定的な128×64印章を生成
- 印章、場所、日時、測定情報をmicroSDへ保存
- 保存済み印章の一覧表示と再閲覧
- バージョン、ペイロード長、CRC32を持つ拡張可能な保存形式

Phase Aは送受信を行いません。Phase BでSub-GHz受信を追加する予定ですが、送信機能は将来も実装しません。

### 必要なもの

- Flipper Zero公式ファームウェア Release版
- Python 3.8以上
- Flipper Devices公式 [uFBT](https://github.com/flipperdevices/flipperzero-ufbt)
- Flipper Zero本体で保存機能を使う場合はmicroSD

### ビルド

```sh
python3 -m pip install --upgrade ufbt
ufbt update --channel=release
ufbt
```

成功すると `dist/radio_goshuincho.fap` が生成されます。

### 実機への転送

USB接続したFlipper ZeroへuFBTで転送・起動する場合:

```sh
ufbt launch
```

またはqFlipperやmicroSDカードリーダーを使い、FAPをmicroSDの `apps/Tools/` にコピーしてください。FAPとFlipper本体の公式ファームウェアRelease世代を合わせてください。

保存データはアプリ専用領域 `/ext/apps_data/radio_goshuincho/` に `.rgs` ファイルとして格納されます。読み込み時にはサイズ、形式バージョン、CRC32を検証し、不完全なレコードを表示しません。

### 操作

1. `Create Stamp` を選びます。
2. 場所名を入力して確定します。
3. 印章が生成され、microSDへ保存されます。
4. `OK` または左右キーで印章と測定情報を切り替えます。
5. `Saved Stamps` から以前の印章を再閲覧できます。

### 実機確認手順（Phase A v0.1.4）

以下は公式ファームウェアRelease 1.5.1とmicroSDを装着したFlipper Zeroで確認した手順です。

1. `ufbt` で生成した `dist/radio_goshuincho.fap` をPCへ用意します。
2. Flipper ZeroをUSB接続し、qFlipperのFile Managerを開きます。
3. 古い版がある場合は削除し、FAPをmicroSDの `/ext/apps/Tools/radio_goshuincho.fap` へコピーします。
4. 本体の `Applications` → `Tools` → `Radio Goshuincho` を起動します。
5. `About` を開き、`Version 0.1.4 / microSD` と表示されることを確認します。
6. `Create Stamp` を選択し、英数字の場所名を入力して `Save` を押します。
7. 情報画面に `Dummy RSSI / Saved` と表示されることを確認します。この時点で保存は完了しています。
8. `Seal` を押し、128×64の印章が描画されることを確認します。
9. `Back` で戻り、`Saved Stamps` に日時と場所名が表示されることを確認します。
10. 保存項目を開き、情報と印章を再表示できることを確認します。

保存ファイルは `/ext/apps_data/radio_goshuincho/` にあります。qFlipperから `.rgs` ファイルをバックアップできます。実機で問題が起きた場合は、ファームウェアとFAPのRelease世代が一致していること、microSDが認識されていること、`About` に期待するアプリバージョンが表示されることを確認してください。

## English

Radio Goshuincho is a Flipper Zero external app that turns observed radio characteristics into a unique monochrome travel stamp tied to a place and time. Phase A uses a reproducible dummy RSSI sequence instead of the real Sub-GHz receiver.

### Phase A features

- Main menu designed for the 128×64 monochrome display
- Place-name input using letters, digits, spaces, and hyphens
- Sixteen deterministic dummy RSSI samples derived from place and RTC time
- Reproducible 128×64 seal generation from place, time, and measurements
- microSD persistence for the seal and all source metadata
- Saved-stamp list and viewer
- Extensible file envelope with a version, payload length, and CRC32

Phase A does not transmit or receive radio signals. Phase B may add Sub-GHz reception. Transmission will never be implemented.

### Requirements

- Official Flipper Zero firmware, Release channel
- Python 3.8 or newer
- Official Flipper Devices [uFBT](https://github.com/flipperdevices/flipperzero-ufbt)
- A microSD card for persistence on the device

### Build

```sh
python3 -m pip install --upgrade ufbt
ufbt update --channel=release
ufbt
```

The output is `dist/radio_goshuincho.fap`.

### Install on a device

With a Flipper Zero connected over USB, build, copy, and launch with:

```sh
ufbt launch
```

Alternatively, use qFlipper or a microSD reader and copy the FAP to `apps/Tools/` on the card. Keep the FAP SDK release aligned with the official firmware release installed on the device.

Stamp records are stored as `.rgs` files in `/ext/apps_data/radio_goshuincho/`. The reader validates the size, format version, and CRC32 and rejects incomplete records.

### Controls

1. Select `Create Stamp`.
2. Enter and confirm a place name.
3. The app generates and saves the stamp to microSD.
4. Press `OK`, Left, or Right to switch between the seal and its metadata.
5. Open `Saved Stamps` to revisit previous seals.

### On-device verification (Phase A v0.1.4)

The following procedure was verified on a Flipper Zero with a microSD card and official firmware Release 1.5.1.

1. Build `dist/radio_goshuincho.fap` with `ufbt` and make the file available on the PC.
2. Connect the Flipper Zero over USB and open qFlipper's File Manager.
3. Remove any older build, then copy the FAP to `/ext/apps/Tools/radio_goshuincho.fap` on the microSD card.
4. On the device, open `Applications` → `Tools` → `Radio Goshuincho`.
5. Open `About` and confirm that it shows `Version 0.1.4 / microSD`.
6. Select `Create Stamp`, enter an alphanumeric place name, and press `Save`.
7. Confirm that the information screen shows `Dummy RSSI / Saved`. Persistence is complete at this point.
8. Press `Seal` and confirm that the 128×64 seal is rendered.
9. Press `Back`, then confirm that `Saved Stamps` lists the timestamp and place name.
10. Open the saved item and confirm that its metadata and seal can be displayed again.

Records are stored in `/ext/apps_data/radio_goshuincho/` and can be backed up through qFlipper. If device verification fails, confirm that the firmware and FAP target the same Release generation, the microSD card is mounted, and `About` shows the expected app version.

## License

MIT. See [LICENSE](LICENSE).
