#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct WadHeader {
    char magic[4];
    uint32_t numDescriptors;
    uint32_t descriptorOffset;
};

struct WadDescriptor {
    uint32_t elementOffset;
    uint32_t elementLength;
    char name[8];
};

// 2. Define the Class Interface
// This matches the requirements in the PDF [cite: 68-96]
class Wad {
private:
    // You need to store the file data in memory!
    std::vector<uint8_t> fileData; 
    std::vector<WadDescriptor> descriptors; // The "Table of Contents"

    // Helper to find a descriptor index based on a path like "/F/F1/LOLWUT"
    int resolvePath(const std::string &path);

public:
    // Constructor is private, use the static loader
    Wad(const uint8_t *data, size_t size); 
    
    // Required Public API
    static Wad* loadWad(const std::string &path);
    std::string getMagic();
    bool isContent(const std::string &path);
    bool isDirectory(const std::string &path);
    int getSize(const std::string &path);
    int getContents(const std::string &path, char *buffer, int length, int offset = 0);
    int getDirectory(const std::string &path, std::vector<std::string> *directory);
    void createDirectory(const std::string &path);
    void createFile(const std::string &path);
    int writeToFile(const std::string &path, const char *buffer, int length, int offset = 0);
};