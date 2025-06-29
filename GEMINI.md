# GEMINI.md

## プロジェクトの概要

このプロジェクトは、`libuv` を基盤とした高性能な非同期HTTPサーバー「Aurora」です。PHPのビルトイン開発サーバーの代替となることを目指しており、C言語で記述されたPHP拡張機能として実装されています。

主な機能は以下の通りです。

-   `libuv` を利用したノンブロッキングI/Oによる高いパフォーマンス
-   OpenSSLを利用したHTTPS/TLSのサポート
-   PHPファイルの自動実行と静的ファイルの配信
-   `$_SERVER`、`$_GET`、`$_POST` などのスーパーグローバルの自動設定
-   設定不要で、すぐに利用可能なCLIインターフェース

## プロジェクト構造

```
├── aurora.c           # C言語で記述された拡張機能のコア部分
├── php_aurora.h       # 拡張機能のヘッダーファイル
├── config.m4          # ビルド設定
├── llhttp.c           # HTTPパーサー (llhttp)
├── llhttp.h           # HTTPパーサーのヘッダー
├── bin/
│   └── aurora-server.php # サーバーを起動するためのPHPスクリプト
├── public/            # デモ用のWebファイル
│   ├── index.php
│   └── test.html
└── tests/             # テストスクリプト
```

## ビルドと実行

### 依存関係

-   PHP 8.4+
-   libuv
-   OpenSSL

### ビルド手順

1.  **依存ライブラリのインストール (Debian/Ubuntuの場合)**
    ```bash
    sudo apt-get install php8.4-dev libuv1-dev libssl-dev
    ```

2.  **拡張機能のビルド**
    ```bash
    phpize
    ./configure
    make
    ```

### サーバーの起動

-   **HTTPサーバーの起動**
    ```bash
    php bin/aurora-server.php --host=127.0.0.1 --port=8080 --docroot=public
    ```

-   **HTTPSサーバーの起動**
    ```bash
    # 自己署名証明書の生成
    openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt \
      -days 365 -nodes -subj "/C=US/ST=Test/L=Test/O=Aurora/CN=localhost"

    # サーバーを起動
    php bin/aurora-server.php --port=8443 --docroot=public --tls-cert=server.crt --tls-key=server.key
    ```

## テスト

プロジェクトには、サーバーの動作を確認するための簡単なテストスクリプトが含まれています。

-   `tests/server_test.php`: 基本的なHTTPサーバーのテスト
-   `tests/test_tls.php`: HTTPSサーバーのテスト

これらのスクリプトは、PHPのCLIから直接実行することでテストできます。

```bash
php tests/server_test.php
```

## 主要な技術要素

-   **PHP拡張**: コア機能はC言語で記述されたPHP拡張として実装されています。
-   **libuv**: 非同期I/Oを実現するためのコアライブラリです。
-   **OpenSSL**: HTTPS/TLS通信をサポートするために使用されます。
-   **llhttp**: Node.jsで使用されている高速なHTTPパーサーです。

## コントリビューションガイドライン

### コミットメッセージ

本プロジェクトでは、コミットメッセージの規約として [Conventional Commits](https://www.conventionalcommits.org/) を採用します。これにより、変更履歴の可読性を高め、バージョン管理を自動化しやすくします。

コミットメッセージは以下の形式に従ってください。

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

-   **type**: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore` など。
-   **scope**: 変更が影響する範囲（例: `http`, `tls`, `build`）。
-   **description**: 変更内容の簡潔な説明。

**例:**

```
feat(http): add support for chunked transfer encoding
```


```