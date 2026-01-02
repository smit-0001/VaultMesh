#include "StorageNode.h"
#include <iostream>
#include <filesystem>

int main() {
    // Configuration (Hardcoded for Phase 1)
    int PORT = 9000;
    std::string DATA_DIR = "./data";

    // Ensure data directory exists
    try {
        if (!std::filesystem::exists(DATA_DIR)) {
            std::filesystem::create_directory(DATA_DIR);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating data directory: " << e.what() << std::endl;
        return 1;
    }

    // Start Server
    try {
        StorageNode node(PORT, DATA_DIR);
        node.start();
    } catch (const std::exception& e) {
        std::cerr << "Server crashed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}