//
// Date       : 21/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "DirectoryNavigator.h"

namespace ReplayClipper {

    DirectoryNavigator::DirectoryNavigator(const fs::path& path) : m_Root(), m_Children() {
        if (!fs::is_directory(path)) {
            return;
        }
        m_Children.reserve(32);
        SetRoot(path);

    }

    bool DirectoryNavigator::SetRoot(const fs::path& root) noexcept {

        if (!fs::is_directory(m_Root)) {
            return false;
        }

        m_Root = root;
        m_Children.clear();

        using DirIter = fs::directory_iterator;
        for (const auto& entry : DirIter{m_Root}) {
            m_Children.push_back(entry.path());
        }

        return true;
    }

} // ReplayClipper