#ifndef WAD_H
#define WAD_H

#include <string>
#include <vector>
#include <cstdint>

using namespace std;

class Wad {
private:
    struct Descriptor {
        uint32_t offset;
        uint32_t length;
        char name[9];
    };

    struct Header {
        char magic[5];
        uint32_t numDescriptors;
        uint32_t descriptorOffset;
    };

    struct Node {
        string name;
        bool isDirectory;
        int descriptorIndex;
        vector<Node*> children;
        Node* parent;
        
        Node(const string& n, bool isDir, int descIdx = -1) 
            : name(n), isDirectory(isDir), descriptorIndex(descIdx), parent(nullptr) {}
        
        ~Node() {
            for (auto child : children) {
                delete child;
            }
        }
    };

    Header header;
    vector<Descriptor> descriptors;
    vector<char> fileData;
    string wadPath;
    Node* root;

    void buildTree();
    Node* findNode(const string& path);
    void saveToFile();
    void loadFileData();
    Node* getParentNode(const string& path);
    string getFileName(const string& path);

public:
    Wad();
    ~Wad();
    
    static Wad* loadWad(const string &path);
    string getMagic();
    bool isContent(const string &path);
    bool isDirectory(const string &path);
    int getSize(const string &path);
    int getContents(const string &path, char *buffer, int length, int offset = 0);
    int getDirectory(const string &path, vector<string> *directory);
    void createDirectory(const string &path);
    void createFile(const string &path);
    int writeToFile(const string &path, const char *buffer, int length, int offset = 0);
};

#endif