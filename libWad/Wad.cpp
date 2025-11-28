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
    if (path.empty()) return nullptr;

    if (path == "/") {
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
    if (path.empty()) return root;
    // Trim trailing slashes
    size_t end = path.size();
    while (end > 0 && path[end - 1] == '/') {
        --end;
    }
    if (end == 0) return root;

    size_t lastSlash = path.rfind('/', end - 1);
    if (lastSlash == string::npos) {
        return root;
    }
    if (lastSlash == 0) {
        return root;
    }
    return findNode(path.substr(0, lastSlash));
}

string Wad::getFileName(const string& path) {
    if (path.empty()) return "";
    // Skip trailing slash(es)
    size_t end = path.size();
    while (end > 0 && path[end - 1] == '/') {
        --end;
    }
    if (end == 0) return "";

    size_t lastSlash = path.rfind('/', end - 1);
    if (lastSlash == string::npos) {
        return path.substr(0, end);
    }
    return path.substr(lastSlash + 1, end - (lastSlash + 1));
}

void Wad::createDirectory(const string &path) {
    Node* parent = getParentNode(path);
    if (parent == nullptr) return;

    string dirName = getFileName(path);

    if (dirName.empty()) {
        return;
    }

    if (dirName.length() > 2) {
        return;
    }

    if (parent != root && parent->isDirectory) {
        string parentName = parent->name;
        if (parentName.length() == 4 && parentName[0] == 'E' && 
            isdigit(parentName[1]) && parentName[2] == 'M' && isdigit(parentName[3])) {
            return;
        }
    }

    int insertIdx = descriptors.size();
    
    if (parent != root) {
        // Start searching AFTER the parent's descriptor
        int startIdx = parent->descriptorIndex;
        int depth = 0;
        
        // Scan forward to find the MATCHING _END tag
        for (size_t i = startIdx + 1; i < descriptors.size(); i++) {
            string name(descriptors[i].name);
            
            // If we hit a nested namespace start, go deeper
            if (name.length() > 6 && name.substr(name.length() - 6) == "_START") {
                depth++;
            }
            // If we hit a namespace end
            else if (name.length() > 4 && name.substr(name.length() - 4) == "_END") {
                if (depth == 0) {
                    insertIdx = i;
                    break;
                }
                depth--;
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

    // Insert into vector
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

    if (fileName.empty()) {
        return;
    }

    if (fileName.length() > 6 && fileName.substr(fileName.length() - 6) == "_START") {
        return;
    }
    if (fileName.length() > 4 && fileName.substr(fileName.length() - 4) == "_END") {
        return;
    }

    if (fileName.length() > 8) {
        return;
    }

    if (fileName.length() == 4 && fileName[0] == 'E' && fileName[2] == 'M' &&
        isdigit(fileName[1]) && isdigit(fileName[3])) {
        return;
    }

    if (parent != root && parent->isDirectory) {
        string parentName = parent->name;
        if (parentName.length() == 4 && parentName[0] == 'E' && 
            isdigit(parentName[1]) && parentName[2] == 'M' && isdigit(parentName[3])) {
            return;
        }
    }

    int insertIdx = descriptors.size();
    
    if (parent != root) {
        int startIdx = parent->descriptorIndex;
        int depth = 0;
        
        for (size_t i = startIdx + 1; i < descriptors.size(); i++) {
            string name(descriptors[i].name);
            
            if (name.length() > 6 && name.substr(name.length() - 6) == "_START") {
                depth++;
            }
            else if (name.length() > 4 && name.substr(name.length() - 4) == "_END") {
                if (depth == 0) {
                    insertIdx = i;
                    break;
                }
                depth--;
            }
        }
    }

    Descriptor fileDesc;
    fileDesc.offset = 0;
    fileDesc.length = 0;
    memset(fileDesc.name, 0, 9);
    strncpy(fileDesc.name, fileName.c_str(), 8);
    
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

    // Do not write to existing files
    if (desc.length != 0) {
        close(fd);
        return 0;
    }

    // The file is empty (new file)
    if (desc.length == 0) {
        desc.offset = header.descriptorOffset;
        desc.length = length;

        lseek(fd, desc.offset, SEEK_SET);
        write(fd, buffer, length);

        header.descriptorOffset += length;
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
