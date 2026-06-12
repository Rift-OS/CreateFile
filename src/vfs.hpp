#pragma once

#include <cstring>
#include <cstdint>
#include <algorithm>

namespace RiftOS {

// 定数定義を集約
constexpr uint32_t MAX_OPEN_FILES = 32;
constexpr uint32_t MAX_FILENAME_LENGTH = 256;
constexpr uint32_t CLUSTER_SIZE = 4096;
constexpr uint32_t INVALID_CLUSTER = 0xFFFFFFFF;

// エラーコードの列挙
enum class FileSystemError {
    Success = 0,
    FileNotFound = -1,
    FilenameInvalid = -2,
    TableFull = -3,
    FileAlreadyExists = -4,
    InvalidDescriptor = -5,
    InvalidOffset = -6,
    IOError = -7,
    EndOfFile = -8,
    PermissionDenied = -9
};

// ファイル属性
enum class FileAttribute {
    Regular = 0x00,
    Directory = 0x01,
    ReadOnly = 0x02,
    Hidden = 0x04,
    System = 0x08
};

// オープンファイル記述子
struct OpenFileDescriptor {
    char filename[MAX_FILENAME_LENGTH];
    FileAttribute attr;
    uint32_t file_size;
    uint32_t start_cluster;
    uint32_t current_cluster;
    uint32_t current_pos;
    bool is_opened;
    int open_count;  // 複数回オープン対応
    bool is_modified;  // ダーティフラグ

    void Clear() {
        std::memset(filename, 0, MAX_FILENAME_LENGTH);
        attr = FileAttribute::Regular;
        file_size = 0;
        start_cluster = INVALID_CLUSTER;
        current_cluster = INVALID_CLUSTER;
        current_pos = 0;
        is_opened = false;
        open_count = 0;
        is_modified = false;
    }
};

class VirtualFileSystem {
private:
    OpenFileDescriptor file_table[MAX_OPEN_FILES];
    uint32_t next_free_cluster;

    // ヘルパー関数
    bool IsFilenameEqual(const char* path, const char* filename);
    bool IsValidFilename(const char* path);
    int FindFileInTable(const char* path);
    int FindFreeDescriptor();
    void SyncFileMetadata(int fd);

public:
    VirtualFileSystem();
    
    // ファイル操作
    int CreateFile(const char* path, FileAttribute attr = FileAttribute::Regular);
    int OpenFile(const char* path);
    int CloseFile(int fd);
    int DeleteFile(const char* path);
    
    // 読み書き
    int ReadFile(int fd, void* buffer, size_t size);
    int WriteFile(int fd, const void* buffer, size_t size);
    
    // シーク
    int SeekFile(int fd, int offset, int whence);
    uint32_t GetFilePosition(int fd) const;
    uint32_t GetFileSize(int fd) const;
    
    // ファイル情報
    bool IsFileOpen(int fd) const;
    const char* GetFilename(int fd) const;
    
    // ストレージ操作
    uint32_t AllocateNewCluster();
    void WriteToStorage(uint32_t cluster, size_t offset, const void* buffer, size_t size);
    void ReadFromStorage(uint32_t cluster, size_t offset, void* buffer, size_t size);
};

} // namespace RiftOS
