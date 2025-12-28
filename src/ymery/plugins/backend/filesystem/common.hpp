// Filesystem backend plugin - Browse and open files from filesystem
// Shared header for FilesystemManager class
#pragma once

#include "../../../types.hpp"
#include "../../../result.hpp"
#include <filesystem>
#include <map>
#include <string>

namespace ymery::plugins {

/**
 * FilesystemManager - manages filesystem browsing, implements TreeLike
 * Tree structure:
 *   /available - browse available locations
 *   /available/fs-root - root filesystem (/)
 *   /available/home - user home directory
 *   /available/mounts - mounted filesystems (Linux)
 *   /available/bookmarks - bookmarked locations (placeholder)
 *   /opened - opened files/devices
 */
class FilesystemManager : public TreeLike {
public:
    static Result<TreeLikePtr> create();

    Result<void> init() override;
    Result<std::vector<std::string>> get_children_names(const DataPath& path) override;
    Result<Dict> get_metadata(const DataPath& path) override;
    Result<std::vector<std::string>> get_metadata_keys(const DataPath& path) override;
    Result<Value> get(const DataPath& path) override;
    Result<void> set(const DataPath& path, const Value& value) override;
    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override;
    Result<std::string> as_tree(const DataPath& path, int depth) override;

private:
    // Virtual shortcuts mapping
    std::map<std::string, std::string> _virtual_shortcuts;

    // Map virtual path to real filesystem path
    std::string _map_virtual_to_real(const std::string& path_str);

    // Parse /proc/self/mounts to get mounted filesystems
    std::map<std::string, Dict> _parse_mounts();

    Result<std::vector<std::string>> _get_available_children(const DataPath& path);
    Result<std::vector<std::string>> _get_opened_children(const DataPath& path);
    Result<Dict> _get_available_metadata(const DataPath& path);
    Result<Dict> _get_opened_metadata(const DataPath& path);
};

} // namespace ymery::plugins
