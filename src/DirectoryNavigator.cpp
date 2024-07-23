//
// Date       : 21/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "DirectoryNavigator.h"

namespace ReplayClipper {

    DirectoryNavigator::DirectoryNavigator(const fs::path& path) : m_Root(), m_Children() {
        m_Children.reserve(32);
        SetRoot(path);
    }

    bool DirectoryNavigator::NavigateToParent() noexcept {
        if (!m_Root.has_parent_path()) {
            return false;
        }
        return SetRoot(m_Root.parent_path());
    }

    bool DirectoryNavigator::SetRoot(const fs::path& root) noexcept {

        if (!fs::is_directory(root)) {
            return false;
        }

        m_Root = fs::absolute(root);
        m_Children.clear();

        using DirIter = fs::directory_iterator;
        for (const auto& entry : DirIter{m_Root}) {
            m_Children.push_back(fs::absolute(entry.path()));
        }

        return true;
    }

} // ReplayClipper