#ifndef STORAGENODE_H
#define STORAGENODE_H

#include <string>
#include <netinet/in.h> // Linux/Mac Socket structures

class StorageNode {
public:
    // Constructor: Needs a Port (e.g., 9000) and a folder path (e.g., "./data")
    StorageNode(int port, std::string storage_dir);
    
    // Destructor: Clean up resources
    ~StorageNode();

    // Starts the main listening loop
    void start();

private:
    int server_fd;               // The server's socket file descriptor
    int port;                    // Port number
    std::string storage_dir;     // Where to save files
    struct sockaddr_in address; 

    // Handles an individual client connection
    void handleClient(int client_socket);

    // Protocol Handlers
    bool processUpload(int client_socket, uint32_t filename_len, uint64_t file_size);
    
    // Helper to read exactly N bytes (handling network fragmentation)
    bool readExact(int socket, void* buffer, size_t size);

    bool processDownload(int client_socket, uint32_t filename_len);
};

#endif