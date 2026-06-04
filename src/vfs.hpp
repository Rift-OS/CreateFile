#ifndef VFS_HPP
#define VFS_HPP

#include <stdint.h>
#include <stddef.h>

namespace RiftOS {

enum class FileAttribute : uint8_t {
    Normal    = 0x00,
    ReadOnly  = 0x01,
    Hidden    = 0x02,
    Directory = 0x10
};

class FileHandle {
public:
    char filename[12];
    FileAttribute attr;
    uint32_t start_cluster;
    uint32_t file_size;
    uint32_t current_pos;
    bool is_opened;

    FileHandle() : attr(FileAttribute::Normal), start_cluster(0), file_size(0), current_pos(0), is_opened(false) {
        filename[0] = '\0';
    }

    void close() { is_opened = false; }
};

class VirtualFileSystem {
private:
    static constexpr size_t MAX_OPEN_FILES = 16;
    FileHandle file_table[MAX_OPEN_FILES];

public:
    VirtualFileSystem();
    int CreateFile(const char* path, FileAttribute attr);
};

} // namespace RiftOS

#endif // VFS_HPP
