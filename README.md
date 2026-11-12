# Radio Goshuincho / 電波御朱印帳

## 日本語

Radio Goshuincho（電波御朱印帳）は、Flipper Zero内蔵Sub-GHz受信機で観測したRSSIから、場所と日時に結び付いた128×64モノクロ御朱印を作る受信専用アプリです。v0.4.0では、RSSIの平均・範囲・ピーク・時系列変化を使う7種類の紋章意匠と、5 Bands測定後のデザイン選択を追加しました。送信、プロトコルデコード、発信源の識別は行いません。

### モード

- `5 Bands`: 315.000 / 426.075 / 429.350 / 433.920 / 922.500 MHzを順次観測。各バンド16窓、各150 ms窓内で50 msごとに3回RSSIを読み、ピーク値を保存（合計80サンプル、約12.25秒）。測定後、五波統合印または帯域別の5意匠を選び、version 4で保存
- `Real RF`: 433.920 MHzを200 ms間隔×50回（約10秒）観測し、車輪・走行軌跡の意匠をversion 2で保存
- `Demo`: 決定的なダミーRSSIから二重枠の角印を生成し、version 2で保存
- `Saved Stamps`: version 1/2/3/4を一覧・再表示。旧bitmapは変更しない

### Phase Dの意匠

Demoは角印、315 MHzは鍵穴と波紋、426 MHzは制御盤とアンテナ塔、429 MHzは歯車と六角形、433 MHzは車輪と走行軌跡、920 MHzはネットワークノード、5 Bandsは五つの紋を一つの円形構図へ統合します。平均強度は各帯域の意匠内だけで段階化し、帯域間の優劣には使用しません。変動幅、相対閾値以上の割合、局所ピーク、方向転換、始点から終点への変化が線数・半径・密度・向きへ反映されます。同一入力は同一bitmapになります。

[7テーマの8倍比較画像](previews/phase_d/themes_comparison_8x.png)（各テーマの128×64実寸PNG/PBMも`previews/phase_d/`に収録）

特徴量と意匠の対応、互換性境界、テスト行列は[Phase D設計資料](docs/PHASE_D_DESIGN.md)を参照してください。

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
tests/generate_theme_previews.sh
```

FAPは`dist/radio_goshuincho.fap`に生成されます。USBなら`ufbt launch`、qFlipperなら`/ext/apps/Tools/radio_goshuincho.fap`へコピーします。本体とSDKのRelease世代を合わせてください。保存先は`/ext/apps_data/radio_goshuincho/`です。

### 5 Bandsの操作

1. `5 Bands`を選び、英数字・空白・ハイフンで場所名を入力します。
2. 5バンドが順次表示されます。Backで中断できます。中断・失敗データは保存されず、Demoへ自動切替しません。
3. 80サンプル完了後に`Choose`を押し、`5 Bands`または315/426/429/433/920 MHzの意匠を選びます。追加受信は行いません。
4. プレビューでOK（`Save`）を押すとversion 4で保存されます。保存後にBackで選択画面へ戻り、同じ観測から別意匠も保存できます。
5. 保存後の情報画面では左右キーで各バンドの実設定周波数、サンプル数、min/avg/maxを切り替え、OKで情報と印影を切り替えます。

既存ファイルは上書き・変換しません。同名時は連番を付けます。v1/v2/v3の保存済みbitmapは変更せず、そのまま表示します。旧v3の生成器と厳密な再生成検証も維持しています。新意匠だけがテーマIDとアルゴリズム版を持つv4を使用します。

### Phase D実機テスト手順（未実施）

このクラウド環境にはFlipper Zeroが接続されていないため、次を実機で確認してください。

1. 公式ReleaseファームウェアとmicroSDへv0.4.0を導入し、`About`の版と`RX only`を確認します。
2. 既存のDemo、Real RF、v1/v2/v3保存印章の一覧・再表示とbitmapが維持されていることを確認します。
3. `5 Bands`を開始し、315→426→429→433→920 MHzの順、各16サンプル、全体80サンプル、グラフとUI応答を確認します。
4. 測定中にBackで中断し、取消表示後に戻れること、その後に標準Sub-GHzアプリが使えることを確認します。
5. 完了後に6候補を順にプレビューし、テーマ差を確認します。二つ以上を保存し、その間に再受信がないことを確認します。
6. v4一覧のテーマ名、5バンド情報、印影を確認し、再起動後も一致することを確認します。
7. microSD未装着/書込み不能、無線使用不可、途中失敗、破損`.rgs`でクラッシュせず、Demoへ自動移行しないことを確認します。
8. 保存・中断・アプリ終了後に標準Sub-GHzアプリが正常に使用できることを確認します。

## English

Radio Goshuincho is a receive-only Flipper Zero app that turns RSSI observations into deterministic 128×64 travel seals. v0.4.0 adds seven distinct crest designs driven by RSSI features and a post-observation design selector. It never transmits, decodes protocols, or identifies emitters.

### Modes

- `5 Bands`: observes 315.000, 426.075, 429.350, 433.920, and 922.500 MHz (80 samples, about 12.25 seconds), then generates any of six v4 designs from that single observation.
- `Real RF`: 433.920 MHz observation with the wheel/track design, saved as version 2.
- `Demo`: deterministic dummy RSSI with the traditional square-seal design, saved as version 2.
- `Saved Stamps`: reads versions 1 through 4 without rewriting older files or bitmaps.

The themes are: Demo square seal; 315 MHz keyhole/ripples; 426 MHz control panel/tower; 429 MHz gear/hexagon; 433 MHz wheel/tracks; 920 MHz network/constellation; and one cohesive five-crest integrated seal. Average strength is bucketed only inside each band's own motif, never used to rank bands. Range, relative high-sample ratio, peaks, turns, and time trend control geometry. Identical input always produces identical bits.

[8× comparison of all seven themes](previews/phase_d/themes_comparison_8x.png); exact 128×64 PNG and PBM files are under `previews/phase_d/`.

See the [Phase D design notes](docs/PHASE_D_DESIGN.md) for feature-to-motif mapping, compatibility boundaries, and the test matrix.

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
tests/generate_theme_previews.sh
```

The output is `dist/radio_goshuincho.fap`. Install with `ufbt launch` or copy it with qFlipper to `/ext/apps/Tools/radio_goshuincho.fap`. Records are stored in `/ext/apps_data/radio_goshuincho/`.

### 5 Bands controls

Select `5 Bands`, enter a place, and wait for all 80 samples. Back cancels without saving or silently falling back to Demo. Press `Choose`, select the integrated design or one of five band themes, preview it, and press OK/`Save`. Back returns to the selector so another design can be saved from the same observation without receiving again. Left/Right cycles per-band metadata after saving; OK toggles metadata and seal. Existing files are never overwritten or migrated. Saved v1/v2/v3 bitmaps and the legacy v3 regeneration validator remain unchanged.

### Phase D device tests (not yet run)

No Flipper Zero is attached to this cloud environment. On matching official Release firmware, verify Demo/Real and v1/v2/v3 regression; five-band order, timing, progress, and cancellation; all six post-observation choices; saving multiple designs without another RF run; v4 save/reboot/reload; radio and microSD failures without fallback; and normal standard Sub-GHz operation after cancellation or exit.

## License

MIT. See [LICENSE](LICENSE).
