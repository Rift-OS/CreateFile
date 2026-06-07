# Rift-OS Kernel File System Module (VFS)

組織名 **Rift-OS** が開発する、x86_64自作OS向けの仮想ファイルシステム（VFS）およびファイル管理モジュールです。

C++のオブジェクト指向に基づき、メモリ上のファイル記述子（FD）を安全にカプセル化して管理します。下位の具体的なファイルシステム（FAT32等）やストレージドライバ（IDE/AHCI/SDHC等）へ繋ぐための標準的な抽象インターフェースを提供します。

---

## 🛠 ファイル構成

* **`src/vfs.hpp`** - VFS、ファイル記述子構造体、属性列挙型の定義
* **`src/vfs.cpp`** - ファイル作成・オープン・読み書き・シーク・クローズ機能の実装
* **`src/kernel.cpp`** - カーネルメイン処理での組み込みサンプル
* **`README.md`** - このドキュメント

---

##  組み込み方法

### 1. ソースコードの配置
ソースファイルをプロジェクトの `src/` ディレクトリに配置します。

### 2. ビルドへの追加
コンパイル対象に `src/vfs.cpp` を追加してください。
自作OSカーネル向けに、例外（Exception）と実行時型情報（RTTI）を無効化するオプションを推奨します。

```makefile
# Makefileの記述例
SRCS = src/kernel.cpp src/vfs.cpp
OBJS = src/kernel.o src/vfs.o

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -fno-exceptions -fno-rtti -c $< -o $@
