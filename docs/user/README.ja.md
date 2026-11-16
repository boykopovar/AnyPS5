[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | **日本語**

# 概要

実行ファイルを Linux と Windows に自動的に移植するためのツールです。

relinker のみのビルドは、Apple シリコンを含む macOS で動作します。

[**Web コンバーター**](https://boykopovar.github.io/AnyPS5/) - ファイルはお使いのデバイスの中にとどまります。

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

実行ファイルを対象システムのネイティブな形式に変換する [relinker](../../core/relinker) と、動的リンクに適した[システムの prx ライブラリ](../../core/libs/prx)の実装が含まれています。エミュレーションや、別の runtime のプロセスはありません。

[**Discord**](https://discord.gg/x9naWVzC6A)、[使い方](USAGE.md)、[ビルド手順](../dev/BUILD.md)、[アーキテクチャ](../dev/ARCHITECTURE.md)、[プロジェクトの技術的負債](../dev/TechnicalDebt.md)、[コードスタイルの規約](../dev/CONVENTIONS.md)、[コントリビュートの方法](../../CONTRIBUTING.md)

## 状況

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shader forms decoded, counted from the code](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[進捗の履歴](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* システムライブラリ：これまでにプロジェクトが把握している関数（[core/libs/prx](../../core/libs/prx) で宣言されているもの）に対する割合であり、PS5 のすべてのシステム関数に対する割合ではありません。宣言される関数が増えるにつれて、総数も増えていきます。</sub>

サポートされていない状態や予期しない状態では、C++ の例外（通常は `std::runtime_error`）が投げられます。どこにも捕捉されない例外は、その `what()` を stderr に出力して、プロセスを終了します。

[シェーダーの再コンパイラー](../../core/shader/recompiler/Recompiler.cpp)は、SPIR-V を正しく生成します（`ANYPS5_ENABLE_SPIRV_TOOLS` を指定してビルドした場合は、[Spirv-Tools](../../3rdparty/SPIRV-Tools) で検証されます）。

## 互換性

ゲームごとの状況とテストの報告は、[Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up) のディスカッションに、タイトルごとに 1 つのスレッドとしてまとめられています。

## 入力のマッピング

アナログスティックとトリガーを含め、SDL でマッピングされたゲームコントローラーに対応しています。キーボードとマウスの操作は、`anyps5-input.ini` ファイルで設定できます。対応しているデバイスと設定の形式については、[入力のマッピング](INPUT_MAPPING.md)をご覧ください。

## 免責事項

このプロジェクトは、相互運用性、研究、保存、互換性を目的としています。著作権で保護されたソフトウェア、ファームウェア、暗号鍵、プロプライエタリなライブラリを含んでおらず、配布もせず、必要ともしません。このプロジェクトで使用するバイナリが、適用される法律とそれぞれのライセンス条項に従って入手・使用されていることを確認する責任は、ユーザーにあります。

AnyPS5 は独立したプロジェクトであり、Sony Interactive Entertainment と提携しておらず、その認可や支持も受けていません。「PlayStation」と「PS5」は Sony Interactive Entertainment Inc. の商標です。

## ライセンス

このプロジェクトは GNU General Public License version 2 only のもとでライセンスされています。
