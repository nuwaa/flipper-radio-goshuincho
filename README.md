# Radio Goshuincho / 電波御朱印帳

## 日本語

Radio Goshuincho（電波御朱印帳）は、Flipper Zero内蔵Sub-GHz受信機で観測したRSSIから、場所と日時に結び付いた128×64モノクロ印章を作る受信専用アプリです。v0.3.0では5つの固定中心周波数を順番に観測する`5 Bands`を追加しました。送信、プロトコルデコード、発信源の識別は行いません。

### モード

- `5 Bands`: 315.000 / 426.075 / 429.350 / 433.920 / 922.500 MHzを順次観測。各バンド16窓、各150 ms窓内で50 msごとに3回RSSIを読み、ピーク値を保存（合計80サンプル、約12.25秒）
- `Real RF`: Phase B互換。433.920 MHzを200 ms間隔×50回（約10秒）観測し、version 2で保存
- `Demo`: 決定的なダミーRSSIを生成し、version 2で保存
- `Saved Stamps`: version 1/2/3を一覧・再表示。`5B`、`R`、`D`で測定種別を区別

RSSIは受信フィルタ帯域内の強度指標です。特定機器の存在、通信内容、または電波が存在しないことを断定できません。アンテナやフロントエンドの感度が帯域ごとに異なるため、異なるバンドの値を単純比較しないでください。

### ビルドとインストール

公式Releaseファームウェア、Python 3.8以上、公式[uFBT](https://github.com/flipperdevices/flipperzero-ufbt)、保存用microSDが必要です。

```sh
python3 -m venv .venv-ufbt
. .venv-ufbt/bin/activate
python3 -m pip install --upgrade ufbt
ufbt update --channel=release
ufbt
tests/run_host_tests.sh
```

FAPは`dist/radio_goshuincho.fap`に生成されます。USBなら`ufbt launch`、qFlipperなら`/ext/apps/Tools/radio_goshuincho.fap`へコピーします。本体とSDKのRelease世代を合わせてください。保存先は`/ext/apps_data/radio_goshuincho/`です。

### 5 Bandsの操作

1. `5 Bands`を選び、英数字・空白・ハイフンで場所名を入力します。
2. 5バンドが順次表示されます。Backで中断できます。中断・失敗データは保存されず、Demoへ自動切替しません。
3. 80サンプル完了後に`Create+Save`を押します。この操作で決定的な印章をmicroSDへ保存します。
4. 情報画面で左右キーを使うと各バンドの実設定周波数、サンプル数、min/avg/maxを切り替えられます。OKで情報と印章を切り替えます。

既存ファイルは上書き・変換しません。同名時は連番を付けます。v1/v2の保存済みbitmapは再生成せず、そのまま表示します。

### Phase C実機テスト手順（未実施）

このクラウド環境にはFlipper Zeroが接続されていないため、次を実機で確認してください。

1. 公式ReleaseファームウェアとmicroSDへv0.3.0を導入し、`About`の版と`RX only`を確認します。
2. 既存のDemo、Real RF、v1/v2保存印章の一覧・再表示が維持されていることを確認します。
3. `5 Bands`を開始し、315→426→429→433→920 MHzの順、各16サンプル、全体80サンプル、グラフとUI応答を確認します。
4. 測定中にBackで中断し、取消表示後に戻れること、その後に標準Sub-GHzアプリが使えることを確認します。
5. 完了後に`Create+Save`を押し、`5B`一覧、5バンド情報、印章を確認します。再起動後も一致することを確認します。
6. microSD未装着/書込み不能、無線使用不可、途中失敗、破損`.rgs`でクラッシュせず、Demoへ自動移行しないことを確認します。
7. 各requested Hzとactual Hz、総所要時間、帯域別のRSSI偏り、短パルス取り逃がしを記録します。

## English

Radio Goshuincho is a receive-only Flipper Zero app that turns RSSI observations into deterministic 128×64 travel seals. v0.3.0 adds `5 Bands`, which sequentially observes five fixed center frequencies. It never transmits, decodes protocols, or identifies emitters.

### Modes

- `5 Bands`: sequentially observe 315.000, 426.075, 429.350, 433.920, and 922.500 MHz. Each band has sixteen 150 ms windows; three 50 ms RSSI reads produce one peak per window (80 samples, about 12.25 seconds).
- `Real RF`: unchanged Phase B 433.920 MHz observation, 50 samples at 200 ms, saved as version 2.
- `Demo`: deterministic dummy RSSI, saved as version 2.
- `Saved Stamps`: reads versions 1, 2, and 3; list labels distinguish `5B`, `R`, and `D`.

RSSI is a strength indication inside the receiver filter bandwidth. It cannot establish transmitter identity, content, or the absence of radio energy. Different bands have different antenna/front-end sensitivity and should not be compared as equal measurements.

### Build and install

Use matching official Release firmware, Python 3.8+, official uFBT, and a microSD card:

```sh
python3 -m venv .venv-ufbt
. .venv-ufbt/bin/activate
python3 -m pip install --upgrade ufbt
ufbt update --channel=release
ufbt
tests/run_host_tests.sh
```

The output is `dist/radio_goshuincho.fap`. Install with `ufbt launch` or copy it with qFlipper to `/ext/apps/Tools/radio_goshuincho.fap`. Records are stored in `/ext/apps_data/radio_goshuincho/`.

### 5 Bands controls

Select `5 Bands`, enter a place, and wait for all 80 samples. Back cancels without saving or silently falling back to Demo. After completion, `Create+Save` generates and writes the seal. Left/Right cycles through per-band actual frequency and statistics; OK toggles metadata and seal. Existing files are never overwritten or migrated, and saved v1/v2 bitmaps are displayed unchanged.

### Phase C device tests (not yet run)

No Flipper Zero is attached to this cloud environment. On matching official Release firmware, verify the v0.2.1 Demo/Real/v1/v2 regression path; five-band order, timing, progress, and sample counts; Back cancellation and later standard Sub-GHz use; v3 save/reboot/reload; radio and microSD failures without fallback; requested-versus-actual frequencies; and real antenna sensitivity/pulse-capture limitations.

## License

MIT. See [LICENSE](LICENSE).
