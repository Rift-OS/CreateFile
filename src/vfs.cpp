#include "vfs_improved.hpp"

namespace RiftOS {

// =============================================================================
// 初期化
// =============================================================================
VirtualFileSystem::VirtualFileSystem() : next_free_cluster(100) {
    for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
        file_table[i].Clear();
    }
}

// =============================================================================
// ヘルパー関数
// =============================================================================

bool VirtualFileSystem::IsFilenameEqual(const char* path, const char* filename) {
    if (!path || !filename) return false;
    return std::strncmp(path, filename, MAX_FILENAME_LENGTH) == 0;
}

bool VirtualFileSystem::IsValidFilename(const char* path) {
    if (!path || path[0] == '\0') return false;
    if (std::strlen(path) >= MAX_FILENAME_LENGTH) return false;
    
    // ファイル名として不正な文字をチェック（OS依存で調整可能）
    const char* invalid_chars = "<>:\"|?*";
    for (const char* p = path; *p; ++p) {
        for (const char* invalid = invalid_chars; *invalid; ++invalid) {
            if (*p == *invalid) return false;
        }
    }
    return true;
}

int VirtualFileSystem::FindFileInTable(const char* path) {
    for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
        if (file_table[i].is_opened && IsFilenameEqual(path, file_table[i].filename)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int VirtualFileSystem::FindFreeDescriptor() {
    for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
        if (!file_table[i].is_opened) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void VirtualFileSystem::SyncFileMetadata(int fd) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) return;
    if (!file_table[fd].is_opened || !file_table[fd].is_modified) return;

    // TODO: メタデータをディスク上のディレクトリエントリに書き込む
    // （ファイルサイズ、更新時刻、属性など）
    file_table[fd].is_modified = false;
}

// =============================================================================
// ファイル操作
// =============================================================================

int VirtualFileSystem::CreateFile(const char* path, FileAttribute attr) {
    // 入力検証
    if (!IsValidFilename(path)) {
        return static_cast<int>(FileSystemError::FilenameInvalid);
    }

    // 重複チェック
    if (FindFileInTable(path) != -1) {
        return static_cast<int>(FileSystemError::FileAlreadyExists);
    }

    // 空き記述子の検索
    int fd = FindFreeDescriptor();
    if (fd == -1) {
        return static_cast<int>(FileSystemError::TableFull);
    }

    // 新規ファイル用のクラスタ確保
    uint32_t new_cluster = AllocateNewCluster();
    if (new_cluster == INVALID_CLUSTER) {
        return static_cast<int>(FileSystemError::IOError);
    }

    // 記述子の初期化
    std::memset(file_table[fd].filename, 0, MAX_FILENAME_LENGTH);
    std::strncpy(file_table[fd].filename, path, MAX_FILENAME_LENGTH - 1);
    file_table[fd].filename[MAX_FILENAME_LENGTH - 1] = '\0';

    file_table[fd].attr = attr;
    file_table[fd].file_size = 0;
    file_table[fd].start_cluster = new_cluster;
    file_table[fd].current_cluster = new_cluster;
    file_table[fd].current_pos = 0;
    file_table[fd].is_opened = true;
    file_table[fd].open_count = 1;
    file_table[fd].is_modified = true;

    // ディスク上のディレクトリにも登録
    SyncFileMetadata(fd);

    return fd;
}

int VirtualFileSystem::OpenFile(const char* path) {
    // 入力検証
    if (!IsValidFilename(path)) {
        return static_cast<int>(FileSystemError::FilenameInvalid);
    }

    // ファイルテーブル内で検索
    int fd = FindFileInTable(path);
    if (fd != -1) {
        // 既に開かれている場合
        if (file_table[fd].open_count < 128) {  // 開き過ぎを防止
            file_table[fd].open_count++;
            file_table[fd].current_pos = 0;
            file_table[fd].current_cluster = file_table[fd].start_cluster;
            return fd;
        }
        return static_cast<int>(FileSystemError::PermissionDenied);
    }

    // ファイルが見つからない場合は新たな記述子を探す（ディスク検索は本来ここで実施）
    return static_cast<int>(FileSystemError::FileNotFound);
}

int VirtualFileSystem::CloseFile(int fd) {
    // バリデーション
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) {
        return static_cast<int>(FileSystemError::InvalidDescriptor);
    }

    OpenFileDescriptor& file = file_table[fd];
    if (!file.is_opened) {
        return static_cast<int>(FileSystemError::InvalidDescriptor);
    }

    // 参照カウントをデクリメント
    if (file.open_count > 0) {
        file.open_count--;
    }

    // 最後のクローザーがメタデータを同期
    if (file.open_count == 0) {
        if (file.is_modified) {
            SyncFileMetadata(fd);
        }
        file.Clear();
    }

    return static_cast<int>(FileSystemError::Success);
}

int VirtualFileSystem::DeleteFile(const char* path) {
    if (!IsValidFilename(path)) {
        return static_cast<int>(FileSystemError::FilenameInvalid);
    }

    int fd = FindFileInTable(path);
    if (fd == -1) {
        return static_cast<int>(FileSystemError::FileNotFound);
    }

    // 開かれているファイルは削除不可
    if (file_table[fd].open_count > 0) {
        return static_cast<int>(FileSystemError::PermissionDenied);
    }

    // TODO: クラスタをFATで解放
    // TODO: ディレクトリエントリから削除

    file_table[fd].Clear();
    return static_cast<int>(FileSystemError::Success);
}

// =============================================================================
// 読み書き操作
// =============================================================================

int VirtualFileSystem::ReadFile(int fd, void* buffer, size_t size) {
    // バリデーション
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) {
        return static_cast<int>(FileSystemError::InvalidDescriptor);
    }

    if (!buffer || size == 0) {
        return 0;
    }

    OpenFileDescriptor& file = file_table[fd];
    if (!file.is_opened) {
        return static_cast<int>(FileSystemError::InvalidDescriptor);
    }

    // EOFチェック
    if (file.current_pos >= file.file_size) {
        return static_cast<int>(FileSystemError::EndOfFile);
    }

    // 読み込み可能なサイズに調整
    size_t bytes_to_read = std::min(size, static_cast<size_t>(file.file_size - file.current_pos));

    // 複数クラスタにまたがる読み込みに対応
    size_t bytes_read = 0;
    uint32_t current_cluster = file.current_cluster;
    size_t cluster_offset = file.current_pos % CLUSTER_SIZE;

    while (bytes_to_read > 0) {
        // 現在のクラスタから読み込める最大サイズ
        size_t bytes_in_cluster = CLUSTER_SIZE - cluster_offset;
        size_t chunk_size = std::min(bytes_to_read, bytes_in_cluster);

        // ストレージから読み込み
        ReadFromStorage(current_cluster, cluster_offset, 
                       static_cast<uint8_t*>(buffer) + bytes_read, chunk_size);

        bytes_read += chunk_size;
        bytes_to_read -= chunk_size;
        file.current_pos += chunk_size;

        // 次のクラスタへ移動
        if (bytes_to_read > 0) {
            current_cluster = AllocateNewCluster();  // TODO: FAT参照に置き換え
            cluster_offset = 0;
        }
    }

    file.current_cluster = current_cluster;
    return static_cast<int>(bytes_read);
}

int VirtualFileSystem::WriteFile(int fd, const void* buffer, size_t size) {
    // バリデーション
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) {
        return static_cast<int>(FileSystemError::InvalidDescriptor);
    }

    if (!buffer || size == 0) {
        return 0;
    }

    OpenFileDescriptor& file = file_table[fd];
    if (!file.is_opened) {
        return static_cast<int>(FileSystemError::InvalidDescriptor);
    }

    // 読み取り専用チェック
    if ((static_cast<int>(file.attr) & static_cast<int>(FileAttribute::ReadOnly)) != 0) {
        return static_cast<int>(FileSystemError::PermissionDenied);
    }

    // 複数クラスタにまたがる書き込みに対応
    size_t bytes_written = 0;
    uint32_t current_cluster = file.current_cluster;
    size_t cluster_offset = file.current_pos % CLUSTER_SIZE;

    while (size > 0) {
        // 現在のクラスタに書き込める最大サイズ
        size_t bytes_in_cluster = CLUSTER_SIZE - cluster_offset;
        size_t chunk_size = std::min(size, bytes_in_cluster);

        // ストレージに書き込み
        WriteToStorage(current_cluster, cluster_offset,
                      static_cast<const uint8_t*>(buffer) + bytes_written, chunk_size);

        bytes_written += chunk_size;
        size -= chunk_size;
        file.current_pos += chunk_size;

        // 次のクラスタを割り当て
        if (size > 0) {
            current_cluster = AllocateNewCluster();
            if (current_cluster == INVALID_CLUSTER) {
                return static_cast<int>(FileSystemError::IOError);
            }
            cluster_offset = 0;
        }
    }

    // ファイルサイズを更新
    if (file.current_pos > file.file_size) {
        file.file_size = file.current_pos;
        file.is_modified = true;
    }

    file.current_cluster = current_cluster;
    return static_cast<int>(bytes_written);
}

// =============================================================================
// シーク操作
// =============================================================================

int VirtualFileSystem::SeekFile(int fd, int offset, int whence) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) {
        return static_cast<int>(FileSystemError::InvalidDescriptor);
    }

    OpenFileDescriptor& file = file_table[fd];
    if (!file.is_opened) {
        return static_cast<int>(FileSystemError::InvalidDescriptor);
    }

    uint32_t new_pos = file.current_pos;

    switch (whence) {
        case 0: // SEEK_SET: ファイル先頭から
            if (offset < 0) {
                return static_cast<int>(FileSystemError::InvalidOffset);
            }
            new_pos = static_cast<uint32_t>(offset);
            break;

        case 1: // SEEK_CUR: 現在位置から
            if ((static_cast<int>(file.current_pos) + offset) < 0) {
                return static_cast<int>(FileSystemError::InvalidOffset);
            }
            new_pos = file.current_pos + offset;
            break;

        case 2: // SEEK_END: ファイル末尾から
            if ((static_cast<int>(file.file_size) + offset) < 0) {
                return static_cast<int>(FileSystemError::InvalidOffset);
            }
            new_pos = file.file_size + offset;
            break;

        default:
            return static_cast<int>(FileSystemError::InvalidOffset);
    }

    // ファイルサイズ超過チェック
    if (new_pos > file.file_size) {
        return static_cast<int>(FileSystemError::InvalidOffset);
    }

    // クラスタチェーンの辿り直し（本来はFAT参照）
    // TODO: start_clusterからFATを参照して new_pos に対応するクラスタを計算
    file.current_pos = new_pos;
    file.current_cluster = file.start_cluster;  // 簡略化のため先頭クラスタに戻す

    return static_cast<int>(new_pos);
}

// =============================================================================
// ファイル情報取得
// =============================================================================

uint32_t VirtualFileSystem::GetFilePosition(int fd) const {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) return 0;
    if (!file_table[fd].is_opened) return 0;
    return file_table[fd].current_pos;
}

uint32_t VirtualFileSystem::GetFileSize(int fd) const {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) return 0;
    if (!file_table[fd].is_opened) return 0;
    return file_table[fd].file_size;
}

bool VirtualFileSystem::IsFileOpen(int fd) const {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) return false;
    return file_table[fd].is_opened;
}

const char* VirtualFileSystem::GetFilename(int fd) const {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) return nullptr;
    if (!file_table[fd].is_opened) return nullptr;
    return file_table[fd].filename;
}

// =============================================================================
// ストレージ操作
// =============================================================================

uint32_t VirtualFileSystem::AllocateNewCluster() {
    // 本来はFATやBitmapを参照して空きクラスタを探す
    // ここではダミー実装
    if (next_free_cluster >= 0xFFFFFFF0) {  // オーバーフロー防止
        return INVALID_CLUSTER;
    }
    return next_free_cluster++;
}

void VirtualFileSystem::WriteToStorage(uint32_t cluster, size_t offset, 
                                       const void* buffer, size_t size) {
    (void)cluster; (void)offset; (void)buffer; (void)size;
    // TODO: 実際のディスク書き込みドライバ（ATA, AHCI, SDカード）を呼び出す
    // 例: disk_driver_write_sector(cluster, buffer, size);
}

void VirtualFileSystem::ReadFromStorage(uint32_t cluster, size_t offset,
                                        void* buffer, size_t size) {
    (void)cluster; (void)offset; (void)buffer; (void)size;
    // TODO: 実際のディスク読み込みドライバを呼び出す
    // 例: disk_driver_read_sector(cluster, buffer, size);
}

} // namespace RiftOS
