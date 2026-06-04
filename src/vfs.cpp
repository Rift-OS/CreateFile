#include "vfs.hpp"

namespace RiftOS {

VirtualFileSystem::VirtualFileSystem() {
    // コンストラクタでテーブルが初期化されるため明示的な処理は不要
}

int VirtualFileSystem::CreateFile(const char* path, FileAttribute attr) {
    // 1. 空いているファイルハンドルを検索
    int fd = -1;
    for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
        if (!file_table[i].is_opened) {
            fd = static_cast<int>(i);
            break;
        }
    }

    if (fd == -1) {
        return -1; // 空きハンドルなし
    }

    // 2. ファイル名のコピー（安全なループ処理）
    size_t j = 0;
    for (j = 0; j < 11 && path[j] != '\0'; ++j) {
        file_table[fd].filename[j] = path[j];
    }
    file_table[fd].filename[j] = '\0';

    // 3. メタデータの設定
    file_table[fd].attr = attr;
    file_table[fd].file_size = 0;
    file_table[fd].start_cluster = 0;
    file_table[fd].current_pos = 0;
    file_table[fd].is_opened = true;

    return fd;
}

} // namespace RiftOS
