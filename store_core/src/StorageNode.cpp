#include "StorageNode.h"
#include "common/Protocol.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <cstring>      // for memset
#include <unistd.h>     // for close, read, write
#include <arpa/inet.h>  // for inet_ntoa
#include <filesystem>   // C++17 filesystem

namespace fs = std::filesystem;

StorageNode::StorageNode(int port, std::string storage_dir) 
    : port(port), storage_dir(storage_dir) {
    
    // 1. Create Socket (IPv4, TCP, Default Protocol)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 2. Set Socket Options (Reuse address/port to avoid "Address already in use" errors)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // 3. Bind to Port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on 0.0.0.0 (All interfaces)
    address.sin_port = htons(port);       // Host to Network Short

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // 4. Start Listening (Queue up to 10 connections)
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
}

StorageNode::~StorageNode() {
    close(server_fd);
}

void StorageNode::start() {
    std::cout << "[StorageNode] Server started on port " << port << std::endl;
    std::cout << "[StorageNode] Saving files to: " << storage_dir << std::endl;

    int addrlen = sizeof(address);

    while (true) {
        // 5. Accept Connection (Blocking Call)
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        std::cout << "[StorageNode] New connection from " << inet_ntoa(address.sin_addr) << std::endl;

        // 6. Handle in a separate thread (Detach so we don't block the main loop)
        std::thread([this, new_socket]() {
            this->handleClient(new_socket);
        }).detach();
    }
}

uint64_t ntoh64(uint64_t input) {
    uint64_t rval;
    uint8_t *data = (uint8_t *)&rval;
    uint8_t *orig = (uint8_t *)&input;

    data[0] = orig[7];
    data[1] = orig[6];
    data[2] = orig[5];
    data[3] = orig[4];
    data[4] = orig[3];
    data[5] = orig[2];
    data[6] = orig[1];
    data[7] = orig[0];

    return rval;
}

void StorageNode::handleClient(int client_socket) {
    uint8_t header_buf[HEADER_SIZE];

    // 1. Read the Header
    if (!readExact(client_socket, header_buf, HEADER_SIZE)) {
        std::cerr << "Failed to read header or connection closed." << std::endl;
        close(client_socket);
        return;
    }

    // 2. Parse Header
    uint8_t magic = header_buf[0];
    uint8_t command = header_buf[1];
    
    if (magic != PROTOCOL_MAGIC) {
        std::cerr << "Invalid Magic Byte. Dropping connection." << std::endl;
        close(client_socket);
        return;
    }

    uint32_t name_len_net;
    uint64_t file_size_net;

    // Copy raw bytes
    std::memcpy(&name_len_net, &header_buf[2], 4);
    std::memcpy(&file_size_net, &header_buf[6], 8);
    
    // CONVERT TO HOST BYTE ORDER
    uint32_t name_len = ntohl(name_len_net); 
    
    // DETECT SYSTEM ENDIANNESS FOR 64-BIT
    // If we are on Little Endian (Intel/AMD), we must swap.
    // If we are on Big Endian, we keep it as is.
    uint32_t num = 1;
    if (*(char *)&num == 1) { 
        // We are on Little Endian, so we MUST swap the network bytes
        file_size_net = ntoh64(file_size_net); 
    }
    uint64_t file_size = file_size_net;

    // 3. Route Command
    if (command == static_cast<uint8_t>(CommandType::UPLOAD)) {
        processUpload(client_socket, name_len, file_size);
    } 
    else if (command == static_cast<uint8_t>(CommandType::DOWNLOAD)) {
        processDownload(client_socket, name_len);
    }
    else {
        std::cerr << "Unknown command: " << (int)command << std::endl;
    }

    close(client_socket);
}

bool StorageNode::processUpload(int client_socket, uint32_t filename_len, uint64_t file_size) {
    // 1. Read Filename
    std::vector<char> name_buf(filename_len);
    if (!readExact(client_socket, name_buf.data(), filename_len)) {
        return false;
    }
    std::string filename(name_buf.begin(), name_buf.end());
    
    // Security: Basic path traversal prevention (MVP)
    if (filename.find("..") != std::string::npos) {
        std::cerr << "Security Warning: Path traversal attempt detected!" << std::endl;
        return false;
    }

    std::string full_path = storage_dir + "/" + filename;
    std::cout << "Receiving: " << filename << " (" << file_size << " bytes)" << std::endl;

    // 2. Open File
    std::ofstream outfile(full_path, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Could not open file for writing: " << full_path << std::endl;
        return false;
    }

    // 3. Read Data Chunks
    const size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    uint64_t remaining = file_size;

    while (remaining > 0) {
        size_t to_read = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
        ssize_t bytes_read = read(client_socket, buffer, to_read);

        if (bytes_read <= 0) {
            std::cerr << "Error reading file data." << std::endl;
            return false;
        }

        outfile.write(buffer, bytes_read);
        remaining -= bytes_read;
    }

    outfile.close();
    std::cout << "File saved successfully!" << std::endl;
    return true;
}

// Helper: loops until exactly 'size' bytes are read
bool StorageNode::readExact(int socket, void* buffer, size_t size) {
    size_t total_read = 0;
    char* ptr = (char*)buffer;

    while (total_read < size) {
        ssize_t bytes_read = read(socket, ptr + total_read, size - total_read);
        if (bytes_read <= 0) return false; // Error or EOF
        total_read += bytes_read;
    }
    return true;
}

// Add this new function
bool StorageNode::processDownload(int client_socket, uint32_t filename_len) {
    // 1. Read Filename from Request
    std::vector<char> name_buf(filename_len);
    if (!readExact(client_socket, name_buf.data(), filename_len)) {
        return false;
    }
    std::string filename(name_buf.begin(), name_buf.end());
    
    // Security check
    if (filename.find("..") != std::string::npos) {
        return false;
    }

    std::string full_path = storage_dir + "/" + filename;
    
    // 2. Check if file exists
    if (!fs::exists(full_path)) {
        std::cerr << "File not found: " << full_path << std::endl;
        // In a real protocol, we should send an ERROR header back.
        // For Phase 1, we just close connection (Python will timeout).
        return false;
    }

    uint64_t file_size = fs::file_size(full_path);
    std::cout << "Sending: " << filename << " (" << file_size << " bytes)" << std::endl;

    // 3. Send Header Back to Python
    // We reuse the same header format: [Magic][DOWNLOAD][NameLen][FileSize]
    uint8_t header[HEADER_SIZE];
    header[0] = PROTOCOL_MAGIC;
    header[1] = static_cast<uint8_t>(CommandType::DOWNLOAD);
    
    // Convert Host to Network Byte Order (Big Endian)
    uint32_t net_name_len = htonl(filename_len); 
    
    // Manual 64-bit swap for file size if on Little Endian
    uint64_t net_file_size = file_size;
    uint32_t num = 1;
    if (*(char *)&num == 1) { 
        net_file_size = ntoh64(file_size); // Reuse the helper we wrote earlier
    }

    std::memcpy(&header[2], &net_name_len, 4);
    std::memcpy(&header[6], &net_file_size, 8);

    if (write(client_socket, header, HEADER_SIZE) < 0) return false;

    // 4. Send Content
    std::ifstream infile(full_path, std::ios::binary);
    char buffer[4096];
    
    while (infile.read(buffer, sizeof(buffer)) || infile.gcount() > 0) {
        if (write(client_socket, buffer, infile.gcount()) < 0) {
            return false;
        }
    }

    std::cout << "File sent successfully." << std::endl;
    return true;
}