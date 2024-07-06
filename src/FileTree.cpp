//
// Date       : 02/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "FileTree.h"

#include <exception>

namespace ReplayClipper {

    FileTree::FileTree() : m_Root() {
    }

    void FileTree::InitRoot(const fs::path& root) {

        if (!fs::exists(root) || !fs::is_directory(root)) {
            return;
        }

        // Reset
        m_Root.Path = root;
        m_Root.Children = {};
        RecurseFileTree(m_Root);

    }

    void FileTree::RecurseFileTree(Node& front) {
        fs::directory_iterator iter{front.Path};
        for (const fs::directory_entry& entry : iter) {
            Node& node = front.Children.emplace_back();
            node.Path = entry.path();

            if (entry.is_directory()) {
                node.Children.reserve(128);
                RecurseFileTree(node);
                node.Children.shrink_to_fit();
            }

        }
    }

} // ReplayClipper