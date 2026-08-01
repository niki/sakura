# キー割り当てプリセット

サクラエディタの「共通設定」→「キー割り当て」→「インポート」で読み込める `.key` ファイル集です。
それぞれ、他のエディタ/IDEのキーマップに近い操作感になるよう、対応するサクラエディタの機能を
主要なショートカットに割り当てています。

## 対応表

| ファイル | 対象キーマップ |
|---|---|
| VSCode.key | Visual Studio Code |
| VisualStudio.key | Visual Studio（現行版の既定キーマップ） |
| VisualStudio6.key | Visual Studio 6 / Visual C++ 6 |
| VisualBasic6.key | Visual Basic 6 |
| CSharp2005.key | Visual C# 2005（Visual Studio 2005） |
| ReSharper.key | ReSharper（Visual Studio 拡張） |
| VisualAssist.key | Visual Assist（Visual Studio 拡張） |
| VisualCpp2.key | Visual C++ 2（1990年代前半の旧IDE） |

## 使い方

共通設定ダイアログの「キー割り当て」ページで「インポート(I)」ボタンから該当ファイルを選択してください。
**インポートは、ファイルに記載されているキー(物理キー)単位で全モディファイア(Shift/Ctrl/Altの組み合わせ)を
まとめて上書きします。** そのキーに他の割り当てを残したい場合は、インポート前にエクスポートしてバックアップを
取ってください。

## 前提・限界

- サクラエディタはテキストエディタであり、コード補完・リファクタリング・デバッグ実行などのIDE機能は
  持っていません。そのため各キーマップの「ファイル操作・編集・検索・移動」に相当する部分だけを対象にしており、
  ビルド/実行/デバッグ/リファクタリング系のショートカットは含めていません。
- **ReSharper.key / VisualAssist.key** は、ReSharper・Visual Assistが素のVisual Studioから変更する
  ショートカットの大半が「コード解析・ナビゲーション・リファクタリング」関連(Go to Everything、Alt+Enterの
  クイックフィックス等)で、いずれもサクラエディタには対応する機能がないため、内容は`VisualStudio.key`と
  同じです(ファイル/編集/検索の基本操作はどちらの拡張機能も変更しないため)。
- **VisualStudio6.key** は、VC++6/VS6世代で確実に安定していたと確認できる基本操作のみに絞っています
  (Visual Studio 2005以降で追加された「すべて保存」「前后の場所へ移動」等は含めていません)。
- **VisualCpp2.key** は1990年代前半の非常に古いIDEで、当時の資料が少なく確認精度が低いです。
  当時のWindowsアプリで広く使われていたCUA準拠の代替キー(Shift+Delete=切り取り、Ctrl+Insert=コピー、
  Shift+Insert=貼り付け)を含めていますが、実際のVisual C++ 2のデフォルト割り当てと完全に一致する保証は
  ありません。参考程度にご利用ください。
- 行コメントの切り替え(Ctrl+/ 等)、複数選択、行の移動(Alt+↑/↓)など、サクラエディタに対応する機能が
  存在しないショートカットは割り当てていません。

## ファイル形式について

サクラエディタの `.key` ファイル(`KEYBIND_VERSION=SakuraKeyBind_Ver4`)形式で手書きしています。
実機でのインポート動作確認はできていません。文法上の詳細は
`sakura_core/typeprop/CImpExpManager.cpp`(`CImpExpKeybind::Import`/`Export`)と
`sakura_core/env/CShareData_IO.cpp`(`IO_KeyBind`)を参照してソースコードから起こしたものです。

各`KeyBind[NNN]=`行の末尾(キー名の後ろ)にはTAB区切りで、8モディファイア分の割り当て機能名を
参考情報として追記しています(未割り当てのスロット=`_`)。実際のキー割り当て(機能コード8個)には
一切影響しません。この注釈はNKMM_FIX_KEYBIND_EXPORT_FUNCNAME機能でインポート時に読み飛ばされ、
元のキー名に復元されます。同機能が無効なビルドでインポートすると、キー名欄にこの注釈がそのまま
連結されて表示されるのでご注意ください(キー割り当ての動作自体は変わりません)。
