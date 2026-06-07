#pragma once

#include <cstddef>
#include <cstdint>

namespace RiftOS {

enum class FileAttribute : uint8_t {
    Normal   = 0x00,
    ReadOnly = 0x01,
    Hidden   = 0x02,
    System   = 0x04,
    Directory= 0x10,
};

// VFSが管理するオープンされたファイルの構造体
struct OpenFileDescriptor {
    char filename[12];              // 8.3形式などを想定（ヌル終端含む）
    FileAttribute attr;
    size_t file_size;               // ファイルの総サイズ（バイト）
    uint32_t start_cluster;         // ストレージ上の開始クラスタ/セクタ位置
    uint32_t current_cluster;       // 現在の読み書き位置があるクラスタ
    size_t current_pos;             // ファイル先頭からの現在のオフセット（バイト）
    bool is_opened;                 // このスロットが使用中かどうか

    void Clear() {
        filename[0] = '\0';
        attr = FileAttribute::Normal;
        file_size = 0;
        start_cluster = 0;
        current_cluster = 0;
        current_pos = 0;
        is_opened = false;
    }
};

class VirtualFileSystem {
public:
    static constexpr size_t MAX_OPEN_FILES = 16;
    static constexpr size_t CLUSTER_SIZE = 512; // 1クラスタ辺りのバイト数（環境に合わせて調整）

    VirtualFileSystem();
    ~VirtualFileSystem() = default;

    // --- ファイル操作 API ---
    int CreateFile(const char* path, FileAttribute attr);
    int OpenFile(const char* path);
    int CloseFile(int fd);
    
    int ReadFile(int fd, void* buffer, size_t size);
    int WriteFile(int fd, const void* buffer, size_t size);
    int SeekFile(int fd, int offset, int whence);

private:
    OpenFileDescriptor file_table[MAX_OPEN_FILES];

    // 内部補助関数
    bool IsFilenameEqual(const char* path, const char* filename);
    uint32_t AllocateNewCluster();
    void WriteToStorage(uint32_t cluster, size_t offset, const void* buffer, size_t size);
    void ReadFromStorage(uint32_t cluster, size_t offset, void* buffer, size_t size);
};

} // namespace RiftOS