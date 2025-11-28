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
    
    read(fd, wad->header.magic, 4);
    wad->header.magic[4] = '\0';
    read(fd, &wad->header.numDescriptors, sizeof(uint32_t));
    read(fd, &wad->header.descriptorOffset, sizeof(uint32_t));
    
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

    vector<pair<Node*, string>> dirStack;
    dirStack.push_back(make_pair(root, ""));
    
    for (size_t i = 0; i < descriptors.size(); i++) {
        string name(descriptors[i].name);
        Node* currentDir = dirStack.back().first;
        
        // Check for namespace end marker
        if (name.length() > 4 && name.substr(name.length() - 4) == "_END") {
            string nsName = name.substr(0, name.length() - 4);
            if (!dirStack.empty() && dirStack.back().second == nsName) {
                dirStack.pop_back();
                continue;
            }
        }
        
        // Check for map marker (E#M#)
        if (name.length() == 4 && name[0] == 'E' && name[2] == 'M' &&
            isdigit(name[1]) && isdigit(name[3])) {
            Node* mapDir = new Node(name, true, i);
            mapDir->parent = currentDir;
            currentDir->children.push_back(mapDir);
            
            for (int j = 1; j <= 10 && i + j < descriptors.size(); j++) {
                Node* child = new Node(string(descriptors[i + j].name), false, i + j);
                child->parent = mapDir;
                mapDir->children.push_back(child);
            }
            i += 10;
        }
        // Check for namespace start marker
        else if (name.length() > 6 && name.substr(name.length() - 6) == "_START") {
            string nsName = name.substr(0, name.length() - 6);
            Node* nsDir = new Node(nsName, true, i);
            nsDir->parent = currentDir;
            currentDir->children.push_back(nsDir);
            
            dirStack.push_back(make_pair(nsDir, nsName));
        }
        // Regular file: add to current directory
        else {
            Node* child = new Node(name, false, i);
            child->parent = currentDir;
            currentDir->children.push_back(child);
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
    
    if (dirName.length() > 2) {
        return;
    }
    
    if (parent != root && parent->isDirectory) {
        // Check if parent is a map marker (E#M# format)
        string parentName = parent->name;
        if (parentName.length() == 4 && parentName[0] == 'E' && 
            isdigit(parentName[1]) && parentName[2] == 'M' && isdigit(parentName[3])) {
            return;
        }
    }
    
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
    memset(startDesc.name, 0, 9);
    strncpy(startDesc.name, (dirName + "_START").c_str(), 8);
    
    // Create END marker
    Descriptor endDesc;
    endDesc.offset = 0;
    endDesc.length = 0;
    memset(endDesc.name, 0, 9);
    strncpy(endDesc.name, (dirName + "_END").c_str(), 8);
    
    descriptors.insert(descriptors.begin() + insertIdx, startDesc);
    descriptors.insert(descriptors.begin() + insertIdx + 1, endDesc);
    
    header.numDescriptors += 2;
    
    saveToFile();
    
    delete root;
    buildTree();
}

void Wad::createFile(const string &path) {
    Node* parent = getParentNode(path);
    if (parent == nullptr) return;
    
    string fileName = getFileName(path);
    
    if (parent != root && parent->isDirectory) {
        string parentName = parent->name;
        if (parentName.length() == 4 && parentName[0] == 'E' && 
            isdigit(parentName[1]) && parentName[2] == 'M' && isdigit(parentName[3])) {
            return;
        }
    }
    
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
    
    int fd = open(wadPath.c_str(), O_RDWR);
    if (fd < 0) return -1;

    // The file is empty
    if (desc.length == 0) {
        desc.offset = header.descriptorOffset;
        desc.length = length;

        lseek(fd, desc.offset, SEEK_SET);
        write(fd, buffer, length);

        header.descriptorOffset += length;
    }
    // The file has data
    else {
        if (offset + length > (int)desc.length) {
             int oldSize = desc.length;
             int newSize = offset + length;
             int oldOffset = desc.offset;

             desc.offset = header.descriptorOffset;
             desc.length = newSize;
             header.descriptorOffset += newSize;

             lseek(fd, desc.offset, SEEK_SET);
             write(fd, fileData.data() + oldOffset, oldSize);

             lseek(fd, desc.offset + offset, SEEK_SET);
             write(fd, buffer, length);
        } 
        else {
            lseek(fd, desc.offset + offset, SEEK_SET);
            write(fd, buffer, length);
        }
    }
    
    close(fd);

    saveToFile(); 
    loadFileData();

    return length;
}

void Wad::saveToFile() {
    int fd = open(wadPath.c_str(), O_RDWR);
    if (fd < 0) return;
    

    lseek(fd, 0, SEEK_SET);
    write(fd, header.magic, 4);
    write(fd, &header.numDescriptors, 4);
    write(fd, &header.descriptorOffset, 4);
    
    lseek(fd, header.descriptorOffset, SEEK_SET);
    
    for (const auto& desc : descriptors) {
        write(fd, &desc.offset, 4);
        write(fd, &desc.length, 4);
        write(fd, desc.name, 8);
    }
    
    close(fd);
}