# reTerminal E1002 e-Paper Display Application

XIAO-ESP32-S3を使用してreTerminal E1002の7インチe-Paperディスプレイ（GDEP073E01）を制御するESP-IDFアプリケーションです。

## ハードウェア要件

- **マイコンボード**: XIAO-ESP32-S3
- **ディスプレイ**: GDEP073E01 (7.3インチ E-Ink Spectra6 フルカラーePaper、800x480ピクセル)
- **電源**: 3.3V（USB-C経由）

## ソフトウェア要件

- ESP-IDF v5.0以降
- Python 3.8以降

## 環境構築

### 1. ESP-IDFのインストール

```bash
# ESP-IDFのインストール（Linux/macOS）
mkdir -p ~/esp
cd ~/esp
git clone -b v5.3.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
```

### 2. 環境変数の設定

```bash
# ESP-IDF環境の有効化
. ~/esp/esp-idf/export.sh
```

## ビルド方法

### 1. プロジェクトのクローン

```bash
git clone [repository-url]
cd reTerminal
```

### 2. ターゲットデバイスの設定

```bash
# ESP32-S3をターゲットに設定
idf.py set-target esp32s3
```

### 3. プロジェクト設定（オプション）

```bash
# メニュー形式で設定を変更
idf.py menuconfig

# reTerminal E1002 Configuration メニューで以下を設定可能：
# - GPIO ピン番号
# - ディスプレイサイズ
# - 機能の有効/無効
```

### 4. ビルド

```bash
# プロジェクトをビルド
idf.py build
```

### 5. 書き込みと実行

```bash
# デバイスのポートを確認（Linux）
ls /dev/ttyUSB*
# または（macOS）
ls /dev/cu.*

# フラッシュ書き込みとシリアルモニター起動
idf.py -p /dev/ttyUSB0 flash monitor

# Windowsの場合
idf.py -p COM3 flash monitor
```

### 6. シリアルモニターの終了

`Ctrl + ]` でシリアルモニターを終了

## クリーンビルド

```bash
# ビルドディレクトリのクリーン
idf.py fullclean

# 再ビルド
idf.py build
```

## トラブルシューティング

### 書き込みエラーの場合

1. USBケーブルがデータ通信対応か確認
2. BOOTボタンを押しながらRESETボタンを押して書き込みモードに入る
3. 書き込み速度を下げる：
   ```bash
   idf.py -p /dev/ttyUSB0 -b 115200 flash
   ```

### ビルドエラーの場合

1. ESP-IDF環境が正しく設定されているか確認：
   ```bash
   idf.py --version
   ```

2. サブモジュールを更新：
   ```bash
   git submodule update --init --recursive
   ```

3. 依存関係を再インストール：
   ```bash
   cd ~/esp/esp-idf
   ./install.sh esp32s3
   ```

## GPIO接続図

| XIAO-ESP32-S3 | GDEP073E01 | 機能 |
|---------------|------------|------|
| GPIO9 | DIN | SPI MOSI |
| GPIO8 | CLK | SPI SCLK |
| GPIO7 | CS | Chip Select |
| GPIO6 | DC | Data/Command |
| GPIO5 | RST | Reset |
| GPIO4 | BUSY | Busy信号 |
| 3.3V | VCC | 電源 |
| GND | GND | グランド |

## プロジェクト構造

```
reTerminal/
├── main/                    # メインアプリケーション
│   ├── main.c              # エントリーポイント
│   ├── CMakeLists.txt      # ビルド設定
│   └── Kconfig.projbuild   # 設定メニュー
├── components/             # コンポーネント
│   ├── epaper_driver/     # e-Paperドライバ
│   ├── display_hal/       # ディスプレイHAL
│   ├── ui_manager/        # UI管理
│   └── system/            # システム機能
├── sdkconfig.defaults     # デフォルト設定
├── CMakeLists.txt         # プロジェクトCMake
└── README.md              # このファイル
```

## 開発状況

- [x] プロジェクト初期構築
- [x] E-Paperドライバ基本実装
- [ ] Display HAL実装
- [ ] UI Manager実装
- [ ] 実機動作確認

## ライセンス

[ライセンス情報を記載]

## 参考資料

- [reTerminal E1002 Wiki](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1002/)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [XIAO ESP32-S3 Wiki](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)