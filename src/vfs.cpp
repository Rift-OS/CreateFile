#include "vfs.hpp"
#include <cstring> // memcpy, strncmp 等の代用（OS自作時は自前実装に置き換え）

namespace RiftOS {

bool VirtualFileSystem::IsFilenameEqual(const char* path, const char* filename) {
    return std::strncmp(path, filename, 11) == 0;
}

// 仮想的なクラスタ割り当て関数（本来はFATやBitmapを操作する）
uint32_t VirtualFileSystem::AllocateNewCluster() {
    static uint32_t next_free_cluster = 100; // ダミーの開始番号
    return next_free_cluster++;
}

// ストレージ（下位ドライバ）への書き込みを仲介するスタブ
void VirtualFileSystem::WriteToStorage(uint32_t cluster, size_t offset, const void* buffer, size_t size) {
    // TODO: ここで実際のディスクドライバ（ATA, AHCI, SDカード等）の
    // セクタ書き込み関数を呼び出します。
    (void)cluster; (void)offset; (void)buffer; (void)size;
}

// ストレージ（下位ドライバ）からの読み込みを仲介するスタブ
void VirtualFileSystem::ReadFromStorage(uint32_t cluster, size_t offset, void* buffer, size_t size) {
    // TODO: ここで実際のディスクドライバのセクタ読み込み関数を呼び出します。
    (void)cluster; (void)offset; (void)buffer; (void)size;
}


VirtualFileSystem::VirtualFileSystem() {
    for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
        file_table[i].Clear();
    }
}

// 1. ファイルの新規作成
int VirtualFileSystem::CreateFile(const char* path, FileAttribute attr) {
    if (!path) return -1;

    // 重複チェック: 既に同じ名前のファイルが登録・オープンされていないか
    for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
        if (file_table[i].is_opened && IsFilenameEqual(path, file_table[i].filename)) {
            return -1; // エラー: 既に存在する
        }
    }

    // 空き記述子（FD）の検索
    int fd = -1;
    for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
        if (!file_table[i].is_opened) {
            fd = static_cast<int>(i);
            break;
        }
    }
    if (fd == -1) return -1; // エラー: テーブル満杯

    // 新規ファイル用のストレージ領域（初期クラスタ）の確保
    uint32_t new_cluster = AllocateNewCluster();

    // 構造体の初期化
    size_t j = 0;
    for (j = 0; j < 11 && path[j] != '\0'; ++j) {
        file_table[fd].filename[j] = path[j];
    }
    file_table[fd].filename[j] = '\0';

    file_table[fd].attr = attr;
    file_table[fd].file_size = 0;
    file_table[fd].start_cluster = new_cluster;
    file_table[fd].current_cluster = new_cluster;
    file_table[fd].current_pos = 0;
    file_table[fd].is_opened = true;

    // TODO: 本来はここで「親ディレクトリのセクタ」にもこのファイル名と初期クラスタを書き込む

    return fd;
}

// 2. 既存ファイルを開く
int VirtualFileSystem::OpenFile(const char* path) {
    if (!path) return -1;

    // 本来はディスク上（ディレクトリ内）を走査して探しますが、
    // ここでは簡易的に「既にfile_tableに登録されているもの」から探します
    for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
        if (file_table[i].is_opened && IsFilenameEqual(path, file_table[i].filename)) {
            // 見つかったらシーク位置を先頭に戻してFDを返す
            file_table[i].current_pos = 0;
            file_table[i].current_cluster = file_table[i].start_cluster;
            return static_cast<int>(i);
        }
    }
    return -1; // ファイルが見つからない
}

// 3. ファイルを閉じる
int VirtualFileSystem::CloseFile(int fd) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) return -1;
    if (!file_table[fd].is_opened) return -1;

    // TODO: キャッシュしているデータがあればここでディスクにフラッシュする
    
    file_table[fd].Clear();
    return 0;
}

// 4. ファイルからの読み込み
int VirtualFileSystem::ReadFile(int fd, void* buffer, size_t size) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) return -1;
    OpenFileDescriptor& file = file_table[fd];
    if (!file.is_opened) return -1;

    // 読み込みサイズがファイル末尾を超えないように調整
    if (file.current_pos + size > file.file_size) {
        size = file.file_size - file.current_pos;
    }
    if (size == 0) return 0; // 既にEOF（末尾）

    // クラスタ内のオフセット計算
    size_t cluster_offset = file.current_pos % CLUSTER_SIZE;
    
    // 実際の低レイヤ読み込み処理を呼び出し
    ReadFromStorage(file.current_cluster, cluster_offset, buffer, size);

    // シーク位置の更新
    file.current_pos += size;
    
    // もし次のクラスタに跨る場合は、本来ここでFAT等を参照して current_cluster を更新する
    // file.current_cluster = GetNextCluster(file.current_cluster);

    return static_cast<int>(size); // 読み込んだバイト数を返す
}

// 5. ファイルへの書き込み
int VirtualFileSystem::WriteFile(int fd, const void* buffer, size_t size) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) return -1;
    OpenFileDescriptor& file = file_table[fd];
    if (!file.is_opened) return -1;
    if (size == 0) return 0;

    size_t cluster_offset = file.current_pos % CLUSTER_SIZE;

    // もし書き込むことでクラスタの境界を超える場合、
    // 本来はループ処理で新しいクラスタを割り当てながら書き込みますが、ここでは1クラスタ内として処理
    WriteToStorage(file.current_cluster, cluster_offset, buffer, size);

    // シーク位置の更新
    file.current_pos += size;

    // 追記された場合はファイルサイズを更新
    if (file.current_pos > file.file_size) {
        file.file_size = file.current_pos;
    }

    return static_cast<int>(size); // 書き込んだバイト数を返す
}

// 6. シーク（読み書き位置の移動）
int VirtualFileSystem::SeekFile(int fd, int offset, int whence) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) return -1;
    OpenFileDescriptor& file = file_table[fd];
    if (!file.is_opened) return -1;

    size_t new_pos = file.current_pos;

    switch (whence) {
        case 0: // SEEK_SET: ファイル先頭から
            if (offset < 0) return -1;
            new_pos = static_cast<size_t>(offset);
            break;
        case 1: // SEEK_CUR: 現在位置から
            if (static_cast<int>(file.current_pos) + offset < 0) return -1;
            new_pos = file.current_pos + offset;
            break;
        case 2: // SEEK_END: ファイル末尾から
            if (static_cast<int>(file.file_size) + offset < 0) return -1;
            new_pos = file.file_size + offset;
            break;
        default:
            return -1;
    }

    // ファイルサイズを超えるシークを禁止する場合（OSの仕様に依存）
    if (new_pos > file.file_size) {
        return -1;
    }

    file.current_pos = new_pos;
    
    // 本来はここで、新しい new_pos に合わせて start_cluster からクラスタチェーンを辿り、
    // file.current_cluster も正しい位置に同期させる処理が必要になります。

    return static_cast<int>(file.current_pos);
}

} // namespace RiftOS