//
// Date       : 21/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#ifndef REPLAYCLIPPER_DIRECTORYNAVIGATOR_H
#define REPLAYCLIPPER_DIRECTORYNAVIGATOR_H

#include <filesystem>

namespace ReplayClipper {

    namespace fs = std::filesystem;

    class DirectoryNavigator {

      private:
        fs::path m_Root;
        std::vector<fs::path> m_Children;

      public:
        DirectoryNavigator(const fs::path& path = fs::current_path());
        ~DirectoryNavigator() = default;

      public:
        bool SetRoot(const fs::path& root) noexcept;

      public:
        inline fs::path GetParent() const noexcept {
            if (m_Root.has_parent_path()) {
                return m_Root.parent_path();
            }
            return {};
        }
        inline const std::vector<fs::path>& GetChildren() const noexcept {
            return m_Children;
        }
        size_t ChildrenCount() const noexcept {
            return m_Children.size();
        }

    };

} // ReplayClipper

#endif
