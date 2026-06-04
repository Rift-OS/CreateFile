#include "timer.h" // 既存のタイマー（C互換）
#include "vfs.hpp"

// C++のグローバルオブジェクトとしてVFSを定義
RiftOS::VirtualFileSystem g_vfs;

extern "C" void kernel_main() {
    // タイマー初期化
    init_sleep_timer();

    // C++のオブジェクト指向スタイルでファイルを新規作成
    int fd = g_vfs.CreateFile("LOG.TXT", RiftOS::FileAttribute::Normal);

    if (fd >= 0) {
        // 作成成功
    } else {
        // 作成失敗
    }

    while (true) {
        msleep(1000);
    }
}
