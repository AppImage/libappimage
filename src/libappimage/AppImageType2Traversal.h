#pragma once
extern "C" {
#include <squashfuse.h>
#include <squashfs_fs.h>
}

#include <memory>
#include "AppImageTraversal.h"

namespace AppImage {

    class AppImageType2Traversal : public AppImageTraversal {
        std::string path;
        bool completed = false;
        struct sqfs fs;
        sqfs_traverse trv;
        sqfs_inode_id root_inode;

    public:
        AppImageType2Traversal(std::string path);

        ~AppImageType2Traversal() override;

        void next() override;

        bool isCompleted() override;

        std::string getEntryName() override;

        void extract(const std::string& target) override;

    private:
        void extractDir(const std::string& target);
        void extractFile(sqfs_inode inode, const std::string& target);
        void extractSymlink(sqfs_inode inode, const std::string& target);

        static sqfs_err sqfs_stat(sqfs* fs, sqfs_inode* inode, struct stat* st);
    };
}
