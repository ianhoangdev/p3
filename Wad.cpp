#include "Wad.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

Wad::Wad() : root(nullptr) {}

Wad::~Wad() {
    if (root) {
        delete root;
    }
}

Wad* Wad::loadWad(const string &path) {
    Wad* wad = new Wad();
    wad->wadPath = path;
    
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        delete wad;
        return nullptr;
    }
    
    // Read header
    read(fd, wad->header.magic, 4);
    wad->header.magic[4] = '\0';
    read(fd, &wad->header.numDescriptors, sizeof(uint32_t));
    read(fd, &wad->header.descriptorOffset, sizeof(uint32_t));
    
    // Read descriptors
    lseek(fd, wad->header.descriptorOffset, SEEK_SET);
    wad->descriptors.resize(wad->header.numDescriptors);
    
    for (uint32_t i = 0; i < wad->header.numDescriptors; i++) {
        read(fd, &wad->descriptors[i].offset, sizeof(uint32_t));
        read(fd, &wad->descriptors[i].length, sizeof(uint32_t));
        read(fd, wad->descriptors[i].name, 8);
        wad->descriptors[i].name[8] = '\0';
    }
    
    close(fd);
    
    wad->loadFileData();
    wad->buildTree();
    
    return wad;
}

void Wad::loadFileData() {
    int fd = open(wadPath.c_str(), O_RDONLY);
    if (fd < 0) return;
    
    off_t fileSize = lseek(fd, 0, SEEK_END);
    fileData.resize(fileSize);
    lseek(fd, 0, SEEK_SET);
    read(fd, fileData.data(), fileSize);
    close(fd);
}

void Wad::buildTree() {
    root = new Node("", true);
    
    for (size_t i = 0; i < descriptors.size(); i++) {
        string name(descriptors[i].name);
        
        // Check for map marker (E#M#)
        if (name.length() == 4 && name[0] == 'E' && name[2] == 'M' &&
            isdigit(name[1]) && isdigit(name[3])) {
            Node* mapDir = new Node(name, true, i);
            mapDir->parent = root;
            root->children.push_back(mapDir);
            
            // Add next 10 elements to this map
            for (int j = 1; j <= 10 && i + j < descriptors.size(); j++) {
                Node* child = new Node(string(descriptors[i + j].name), false, i + j);
                child->parent = mapDir;
                mapDir->children.push_back(child);
            }
            i += 10;
        }
        // Check for namespace start
        else if (name.length() > 6 && name.substr(name.length() - 6) == "_START") {
            string nsName = name.substr(0, name.length() - 6);
            Node* nsDir = new Node(nsName, true, i);
            nsDir->parent = root;
            root->children.push_back(nsDir);
            
            // Find matching _END
            i++;
            while (i < descriptors.size()) {
                string currentName(descriptors[i].name);
                
                if (currentName == nsName + "_END") {
                    break;
                }
                
                // Check for nested namespace
                if (currentName.length() > 6 && currentName.substr(currentName.length() - 6) == "_START") {
                    string nestedNsName = currentName.substr(0, currentName.length() - 6);
                    Node* nestedDir = new Node(nestedNsName, true, i);
                    nestedDir->parent = nsDir;
                    nsDir->children.push_back(nestedDir);
                    
                    i++;
                    while (i < descriptors.size()) {
                        string nestedCurrentName(descriptors[i].name);
                        if (nestedCurrentName == nestedNsName + "_END") {
                            break;
                        }
                        
                        // Check for map marker inside nested namespace
                        if (nestedCurrentName.length() == 4 && nestedCurrentName[0] == 'E' && 
                            nestedCurrentName[2] == 'M' && isdigit(nestedCurrentName[1]) && 
                            isdigit(nestedCurrentName[3])) {
                            Node* mapDir = new Node(nestedCurrentName, true, i);
                            mapDir->parent = nestedDir;
                            nestedDir->children.push_back(mapDir);
                            
                            for (int j = 1; j <= 10 && i + j < descriptors.size(); j++) {
                                Node* child = new Node(string(descriptors[i + j].name), false, i + j);
                                child->parent = mapDir;
                                mapDir->children.push_back(child);
                            }
                            i += 10;
                        } else {
                            Node* child = new Node(nestedCurrentName, false, i);
                            child->parent = nestedDir;
                            nestedDir->children.push_back(child);
                        }
                        i++;
                    }
                }
                // Check for map marker
                else if (currentName.length() == 4 && currentName[0] == 'E' && 
                         currentName[2] == 'M' && isdigit(currentName[1]) && isdigit(currentName[3])) {
                    Node* mapDir = new Node(currentName, true, i);
                    mapDir->parent = nsDir;
                    nsDir->children.push_back(mapDir);
                    
                    for (int j = 1; j <= 10 && i + j < descriptors.size(); j++) {
                        Node* child = new Node(string(descriptors[i + j].name), false, i + j);
                        child->parent = mapDir;
                        mapDir->children.push_back(child);
                    }
                    i += 10;
                } else {
                    Node* child = new Node(currentName, false, i);
                    child->parent = nsDir;
                    nsDir->children.push_back(child);
                }
                i++;
            }
        }
    }
}

Wad::Node* Wad::findNode(const string& path) {
    if (path == "/" || path.empty()) {
        return root;
    }
    
    stringstream ss(path);
    string token;
    Node* current = root;
    
    while (getline(ss, token, '/')) {
        if (token.empty()) continue;
        
        bool found = false;
        for (auto child : current->children) {
            if (child->name == token) {
                current = child;
                found = true;
                break;
            }
        }
        
        if (!found) {
            return nullptr;
        }
    }
    
    return current;
}

string Wad::getMagic() {
    return string(header.magic);
}

bool Wad::isContent(const string &path) {
    Node* node = findNode(path);
    return node != nullptr && !node->isDirectory;
}

bool Wad::isDirectory(const string &path) {
    Node* node = findNode(path);
    return node != nullptr && node->isDirectory;
}

int Wad::getSize(const string &path) {
    Node* node = findNode(path);
    if (node == nullptr || node->isDirectory) {
        return -1;
    }
    return descriptors[node->descriptorIndex].length;
}

int Wad::getContents(const string &path, char *buffer, int length, int offset) {
    Node* node = findNode(path);
    if (node == nullptr || node->isDirectory) {
        return -1;
    }
    
    Descriptor& desc = descriptors[node->descriptorIndex];
    int available = desc.length - offset;
    if (available <= 0) {
        return 0;
    }
    
    int bytesToCopy = (length < available) ? length : available;
    memcpy(buffer, fileData.data() + desc.offset + offset, bytesToCopy);
    
    return bytesToCopy;
}

int Wad::getDirectory(const string &path, vector<string> *directory) {
    Node* node = findNode(path);
    if (node == nullptr || !node->isDirectory) {
        return -1;
    }
    
    for (auto child : node->children) {
        directory->push_back(child->name);
    }
    
    return node->children.size();
}

Wad::Node* Wad::getParentNode(const string& path) {
    size_t lastSlash = path.find_last_of('/');
    if (lastSlash == string::npos || lastSlash == 0) {
        return root;
    }
    return findNode(path.substr(0, lastSlash));
}

string Wad::getFileName(const string& path) {
    size_t lastSlash = path.find_last_of('/');
    if (lastSlash == string::npos) {
        return path;
    }
    return path.substr(lastSlash + 1);
}

void Wad::createDirectory(const string &path) {
    Node* parent = getParentNode(path);
    if (parent == nullptr) return;
    
    string dirName = getFileName(path);
    
    // Find insertion point (before parent's _END marker)
    int insertIdx = descriptors.size();
    
    if (parent != root) {
        // Find parent's _END marker
        for (size_t i = 0; i < descriptors.size(); i++) {
            string name(descriptors[i].name);
            if (name == parent->name + "_END") {
                insertIdx = i;
                break;
            }
        }
    }
    
    // Create START marker
    Descriptor startDesc;
    startDesc.offset = 0;
    startDesc.length = 0;
    strncpy(startDesc.name, (dirName + "_START").c_str(), 8);
    startDesc.name[8] = '\0';
    
    // Create END marker
    Descriptor endDesc;
    endDesc.offset = 0;
    endDesc.length = 0;
    strncpy(endDesc.name, (dirName + "_END").c_str(), 8);
    endDesc.name[8] = '\0';
    
    descriptors.insert(descriptors.begin() + insertIdx, startDesc);
    descriptors.insert(descriptors.begin() + insertIdx + 1, endDesc);
    
    header.numDescriptors += 2;
    
    // Update tree
    Node* newDir = new Node(dirName, true, insertIdx);
    newDir->parent = parent;
    parent->children.push_back(newDir);
    
    saveToFile();
    
    // Rebuild tree
    delete root;
    buildTree();
}

void Wad::createFile(const string &path) {
    Node* parent = getParentNode(path);
    if (parent == nullptr) return;
    
    string fileName = getFileName(path);
    
    int insertIdx = descriptors.size();
    
    if (parent != root) {
        for (size_t i = 0; i < descriptors.size(); i++) {
            string name(descriptors[i].name);
            if (name == parent->name + "_END") {
                insertIdx = i;
                break;
            }
        }
    }
    
    Descriptor fileDesc;
    fileDesc.offset = 0;
    fileDesc.length = 0;
    strncpy(fileDesc.name, fileName.c_str(), 8);
    fileDesc.name[8] = '\0';
    
    descriptors.insert(descriptors.begin() + insertIdx, fileDesc);
    header.numDescriptors++;
    
    Node* newFile = new Node(fileName, false, insertIdx);
    newFile->parent = parent;
    parent->children.push_back(newFile);
    
    saveToFile();
    
    delete root;
    buildTree();
}

int Wad::writeToFile(const string &path, const char *buffer, int length, int offset) {
    Node* node = findNode(path);
    if (node == nullptr || node->isDirectory) {
        return -1;
    }
    
    Descriptor& desc = descriptors[node->descriptorIndex];
    
    // Calculate new size
    int newSize = offset + length;
    if (newSize > (int)desc.length) {
        // Need to expand file
        desc.offset = fileData.size();
        desc.length = newSize;
        fileData.resize(fileData.size() + newSize);
    }
    
    memcpy(fileData.data() + desc.offset + offset, buffer, length);
    
    saveToFile();
    
    return length;
}

void Wad::saveToFile() {
    int fd = open(wadPath.c_str(), O_WRONLY | O_TRUNC);
    if (fd < 0) return;
    
    // Update descriptor offset
    header.descriptorOffset = fileData.size();
    
    // Write header
    write(fd, header.magic, 4);
    write(fd, &header.numDescriptors, sizeof(uint32_t));
    write(fd, &header.descriptorOffset, sizeof(uint32_t));
    
    // Write file data
    write(fd, fileData.data(), fileData.size());
    
    // Write descriptors
    for (const auto& desc : descriptors) {
        write(fd, &desc.offset, sizeof(uint32_t));
        write(fd, &desc.length, sizeof(uint32_t));
        write(fd, desc.name, 8);
    }
    
    close(fd);
}