# CreateFile - Kernel File System Module

組織名 Rift-OS が開発する、x86_64自作OS向けの仮想ファイルシステム（VFS）およびファイル作成モジュールです。

C++のオブジェクト指向に基づき、メモリ上のファイル記述子を安全にカプセル化して管理します。ファイルシステム（FAT32等）やストレージドライバへ繋ぐための標準的なインターフェースを提供します。

## ファイル構成

*   src/vfs.hpp - 仮想ファイルシステムおよびクラスの定義
*   src/vfs.cpp - CreateFile機能の実装
*   src/kernel.cpp - カーネルメイン処理での組み込みサンプル
*   README.md - このドキュメント

## 組み込み方法

### 1. ソースコードの配置
ソースファイルをプロジェクトの `src/` ディレクトリに配置します。

### 2. ビルドへの追加
コンパイル対象に `src/vfs.cpp` を追加してください。
```makefile
# Makefileの記述例
SRCS = src/kernel.cpp src/vfs.cpp
OBJS = src/kernel.o src/vfs.o

%.o: %.cpp
	\$(CXX) \$(CXXFLAGS) -fno-exceptions -fno-rtti -c \$< -o \$@
```

## 使い方

カーネルのエントリーポイントからグローバルまたはローカルの `VirtualFileSystem` インスタンスを呼び出し、`CreateFile` でファイルを新規作成します。

```cpp
#include "vfs.hpp"

RiftOS::VirtualFileSystem g_vfs;

extern "C" void kernel_main() {
    // "SYS.LOG" というファイルを新規作成
    int fd = g_vfs.CreateFile("SYS.LOG", RiftOS::FileAttribute::Normal);
}
```

## 技術仕様

1.  ファイル記述子管理: 固定長（最大16ファイル）のハンドルテーブルによりファイル属性や現在のオフセットを隠蔽管理します。
2.  属性フラグ: 通常ファイル (Normal)、読み取り専用 (ReadOnly)、隠しファイル (Hidden)、ディレクトリ (Directory) を enum class で厳格に分離しています。

## ライセンス

This project is licensed under the GNU General Public License v3.0 (GPL-3.0) - see the LICENSE file for details.

---
Copyright (c) 2026 Rift-OS
