#include "vfs.hpp"

RiftOS::VirtualFileSystem g_vfs;

extern "C" void kernel_main() {
    // 起動時にファイルシステムモジュールからファイルを新規作成
    int fd = g_vfs.CreateFile("SYS.LOG", RiftOS::FileAttribute::Normal);

    if (fd >= 0) {
        // ファイル作成成功時の処理
    } else {
        // エラー処理
    }

    while (true) {
        __asm__ volatile("hlt");
    }
}
