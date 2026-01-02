#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

// Fixed Header Size: 1 + 1 + 4 + 8 = 14 Bytes
const int HEADER_SIZE = 14;

// Magic Byte to verify protocol (ASCII 'V' for VaultMesh)
const uint8_t PROTOCOL_MAGIC = 0x56; 

// Commands
enum class CommandType : uint8_t {
    UPLOAD   = 0x01,
    DOWNLOAD = 0x02
};

// The Structure of our Binary Header
// Note: We don't use a struct directly for network transmission 
// due to struct padding/alignment issues across different OS/Compilers.
// We will manually serialize/deserialize these bytes.
struct FileHeader {
    uint8_t magic;         // 1 Byte
    uint8_t command;       // 1 Byte
    uint32_t filename_len; // 4 Bytes (Network Byte Order)
    uint64_t file_size;    // 8 Bytes (Network Byte Order)
};

#endif
