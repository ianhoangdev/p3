#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <vector>
#include "../libWad/Wad.h"

using namespace std;

static Wad* wad = nullptr;

static int wadfs_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    
    if (wad->isDirectory(path)) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        return 0;
    }
    
    if (wad->isContent(path)) {
        stbuf->st_mode = S_IFREG | 0777;
        stbuf->st_nlink = 1;
        stbuf->st_size = wad->getSize(path);
        return 0;
    }
    
    return -ENOENT;
}

static int wadfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;
    
    if (!wad->isDirectory(path)) {
        return -ENOENT;
    }
    
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    
    vector<string> entries;
    wad->getDirectory(path, &entries);
    
    for (const auto& entry : entries) {
        filler(buf, entry.c_str(), NULL, 0);
    }
    
    return 0;
}

static int wadfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    (void) fi;
    
    if (!wad->isContent(path)) {
        return -ENOENT;
    }
    
    int bytesRead = wad->getContents(path, buf, size, offset);
    
    if (bytesRead < 0) {
        return -ENOENT;
    }
    
    return bytesRead;
}

static int wadfs_mkdir(const char *path, mode_t mode) {
    (void) mode;
    
    wad->createDirectory(path);
    return 0;
}

static int wadfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    (void) mode;
    (void) rdev;
    
    wad->createFile(path);
    return 0;
}

static int wadfs_write(const char *path, const char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    
    if (!wad->isContent(path)) {
        return -ENOENT;
    }
    
    int bytesWritten = wad->writeToFile(path, buf, size, offset);
    
    if (bytesWritten < 0) {
        return -ENOENT;
    }
    
    return bytesWritten;
}

static struct fuse_operations wadfs_oper;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        return 1;
    }
    
    // Find the WAD file argument (not starting with -)
    string wadFile;
    int wadArgIndex = -1;
    for (int i = 1; i < argc - 1; i++) {
        if (argv[i][0] != '-') {
            wadFile = argv[i];
            wadArgIndex = i;
            break;
        }
    }
    
    if (wadFile.empty()) {
        return 1;
    }
    
    wad = Wad::loadWad(wadFile);
    if (wad == nullptr) {
        return 1;
    }
    
    // Remove WAD file argument from argv for FUSE
    for (int i = wadArgIndex; i < argc - 1; i++) {
        argv[i] = argv[i + 1];
    }
    argc--;
    
    memset(&wadfs_oper, 0, sizeof(wadfs_oper));
    wadfs_oper.getattr = wadfs_getattr;
    wadfs_oper.readdir = wadfs_readdir;
    wadfs_oper.read = wadfs_read;
    wadfs_oper.mkdir = wadfs_mkdir;
    wadfs_oper.mknod = wadfs_mknod;
    wadfs_oper.write = wadfs_write;
    
    int ret = fuse_main(argc, argv, &wadfs_oper, NULL);
    
    delete wad;
    
    return ret;
}