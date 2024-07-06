//
// Date       : 02/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#ifndef REPLAYCLIPPER_FILETREE_H
#define REPLAYCLIPPER_FILETREE_H

#include <filesystem>
#include <vector>
#include <memory>
#include <functional>

namespace ReplayClipper {

    namespace fs = std::filesystem;

    class FileTree {

      public:
        struct Node {
            fs::path Path;
            std::vector<Node> Children;
        };

      private:
        Node m_Root;

      public:
        FileTree();
        ~FileTree() = default;

      public:
        void InitRoot(const fs::path& root);

      public:
        inline const Node& Root() const noexcept {
            return m_Root;
        }

      private:
        void RecurseFileTree(const Node& node, const std::function<void(const Node&)>& fn) const noexcept;
        void RecurseFileTree(Node& front);
    };

} // ReplayClipper

#endif
