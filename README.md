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

## HTTPサーバー機能

WiFi接続後、ESP32-S3上でHTTPサーバーが自動起動し、SDカード上のファイルをWebブラウザから操作できます。

### アクセス方法

1. シリアルモニターでESP32のIPアドレスを確認
```
Access the server at: http://192.168.1.xxx
```

2. ブラウザで上記URLにアクセス

### 利用可能なHTTP機能

#### 📁 ファイルブラウジング
- **URL**: `http://ESP32_IP/`
- **機能**: SDカード上のファイル・ディレクトリ一覧をJSON形式で表示
- **メソッド**: GET
- **レスポンス**:
```json
{
  "files": [
    {"name": "test.bmp", "size": 1440054, "isDir": false, "modified": 1695523200},
    {"name": "images", "size": 0, "isDir": true, "modified": 1695523100}
  ],
  "path": "/"
}
```

#### 📥 ファイルダウンロード
- **URL**: `http://ESP32_IP/path/to/file.ext`
- **機能**: SDカード上の任意のファイルをダウンロード
- **メソッド**: GET
- **対応形式**: HTML, CSS, JS, 画像(PNG/JPG/GIF), PDF, テキストファイル等
- **例**: `http://192.168.1.100/test.bmp` でtest.bmpファイルをダウンロード

#### 📤 ファイルアップロード
- **URL**: `http://ESP32_IP/path/to/upload/file.ext`
- **機能**: SDカードに任意のファイルをアップロード
- **メソッド**: POST
- **制限**: 最大ファイルサイズ 5MB
- **例**: `curl -X POST -T myfile.txt http://192.168.1.100/uploaded_file.txt`

#### 🗑️ ファイル削除
- **URL**: `http://ESP32_IP/path/to/file.ext`
- **機能**: SDカード上のファイルを削除
- **メソッド**: DELETE
- **例**: `curl -X DELETE http://192.168.1.100/old_file.txt`

#### 🖼️ e-Paper表示更新
- **URL**: `http://ESP32_IP/api/update`
- **機能**: SDカードの`test.bmp`をe-Paperディスプレイに表示
- **メソッド**: POST
- **例**: `curl -X POST http://192.168.1.100/api/update`
- **レスポンス**: `{"status":"success","message":"Image displayed successfully"}`

### セキュリティ機能

- **パストラバーサル対策**: `../`を含むパスは拒否
- **ファイルサイズ制限**: アップロードは5MBまで
- **パス長制限**: 最大1024文字まで

### 技術仕様

- **ポート**: 80 (HTTP)
- **最大同時接続数**: 5
- **対応メソッド**: GET, POST, DELETE
- **MIME自動判定**: ファイル拡張子に基づく適切なContent-Type設定
- **チャンク転送**: 大きなファイルの効率的な転送

### 使用例

#### cURLでの操作例
```bash
# ファイル一覧取得
curl http://192.168.1.100/

# ファイルダウンロード
curl -O http://192.168.1.100/test.bmp

# ファイルアップロード
curl -X POST -T localfile.txt http://192.168.1.100/remotefile.txt

# ファイル削除
curl -X DELETE http://192.168.1.100/unwanted.txt

# e-Paper表示更新
curl -X POST http://192.168.1.100/api/update
```

#### Webブラウザでの利用
- ブラウザで `http://ESP32_IP/` にアクセス
- JSONファイル一覧から目的のファイルURLを確認
- `http://ESP32_IP/filename.ext` で直接ファイルアクセス

## WiFi設定

### SDカード上のConfigファイル

WiFi接続とIP設定は、SDカード上の `config` ファイルで設定します。

#### 設定ファイルの場所
```
/sdcard/config
```

#### 設定項目

| 項目 | 説明 | 必須 | デフォルト値 |
|------|------|------|-------------|
| `SSID` | WiFiネットワーク名 | ✅ | - |
| `PASSWORD` | WiFiパスワード | ✅ | - |
| `IP_MODE` | IP取得方式（`dhcp` または `static`） | ❌ | `dhcp` |
| `STATIC_IP` | 固定IPアドレス | ※ | - |
| `STATIC_NETMASK` | サブネットマスク | ※ | - |
| `STATIC_GATEWAY` | ゲートウェイアドレス | ※ | - |
| `STATIC_DNS` | DNSサーバーアドレス | ❌ | - |
| `HIDDEN_SSID` | 隠されたSSID（`true`/`false`） | ❌ | `false` |
| `BSSID` | 特定のアクセスポイント指定 | ❌ | - |

※ `IP_MODE=static` の場合は必須

#### 設定例

**DHCP使用（推奨）**:
```ini
SSID=MyWiFiNetwork
PASSWORD=MySecurePassword
IP_MODE=dhcp
```

**固定IP使用**:
```ini
SSID=MyWiFiNetwork
PASSWORD=MySecurePassword
IP_MODE=static
STATIC_IP=192.168.1.100
STATIC_NETMASK=255.255.255.0
STATIC_GATEWAY=192.168.1.1
STATIC_DNS=8.8.8.8
```

**隠しSSID with 固定IP**:
```ini
SSID=HiddenNetwork
PASSWORD=HiddenPassword
HIDDEN_SSID=true
IP_MODE=static
STATIC_IP=10.0.1.50
STATIC_NETMASK=255.255.255.0
STATIC_GATEWAY=10.0.1.1
STATIC_DNS=1.1.1.1
```

### WiFi接続の確認

シリアルモニターで接続状況を確認できます：

```
I (3456) WIFI: Connected to AP successfully
I (3467) WIFI: Static IP configured: 192.168.1.100, Netmask: 255.255.255.0, Gateway: 192.168.1.1
I (3478) MAIN: IP Address: 192.168.1.100
I (3489) MAIN: Access the server at: http://192.168.1.100
```

### トラブルシューティング

#### WiFi接続に失敗する場合

1. **設定ファイルの確認**
   - SDカードに `config` ファイルが存在するか
   - ファイル内容に構文エラーがないか
   - SSIDとパスワードが正しいか

2. **固定IP設定の確認**
   - IPアドレスがネットワーク範囲内にあるか
   - 他のデバイスと重複していないか
   - ゲートウェイアドレスが正しいか

3. **シリアルモニターでエラー確認**
   ```bash
   idf.py monitor
   ```

#### よくあるエラーと対処法

| エラーメッセージ | 原因 | 対処法 |
|-----------------|------|--------|
| `Failed to read config file` | SDカードまたはconfigファイルが見つからない | SDカードの挿入とファイル存在確認 |
| `Failed to parse config file` | 設定ファイルの書式エラー | ファイル内容の確認・修正 |
| `Static IP is required when IP_MODE is static` | 固定IP設定が不完全 | STATIC_IP, STATIC_NETMASK, STATIC_GATEWAYを設定 |
| `Failed to connect to AP` | WiFi認証失敗 | SSIDとパスワードの確認 |

## SDカード要件

- **対応形式**: FAT32ファイルシステム
- **容量**: 32GBまで推奨
- **接続**: SPI経由（MISO:8, MOSI:9, CLK:7, CS:14）
- **必須ファイル**: `/config` （WiFi設定ファイル）

## 開発状況

- [x] プロジェクト初期構築
- [x] E-Paperドライバ基本実装
- [x] WiFi接続機能
- [x] WiFi DHCP/固定IP対応 🆕
- [x] SDカード読み書き機能
- [x] HTTPサーバー機能
- [ ] Display HAL実装
- [ ] UI Manager実装
- [ ] 実機動作確認

## ライセンス

[ライセンス情報を記載]

## 参考資料

- [reTerminal E1002 Wiki](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1002/)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [XIAO ESP32-S3 Wiki](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)