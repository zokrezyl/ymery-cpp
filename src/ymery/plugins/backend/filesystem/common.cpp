// Filesystem backend plugin - Implementation
#include "common.hpp"
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace ymery::plugins {

Result<TreeLikePtr> FilesystemManager::create() {
    auto manager = std::make_shared<FilesystemManager>();
    if (auto res = manager->init(); !res) {
        return Err<TreeLikePtr>("FilesystemManager::create failed", res);
    }
    return manager;
}

Result<void> FilesystemManager::init() {
    // Initialize virtual shortcuts
    _virtual_shortcuts["/fs-root"] = "/";

    // Get home directory
    const char* home = std::getenv("HOME");
    if (home) {
        _virtual_shortcuts["/home"] = home;
    } else {
        _virtual_shortcuts["/home"] = "/";
    }

    return Ok();
}

Result<std::vector<std::string>> FilesystemManager::get_children_names(const DataPath& path) {
    std::string path_str = path.to_string();
    if (!path_str.empty() && path_str[0] != '/') {
        path_str = "/" + path_str;
    }

    // Root
    if (path_str == "/" || path_str.empty()) {
        return Ok(std::vector<std::string>{"available", "opened"});
    }

    // Branch handling
    const auto& parts = path.as_list();
    if (parts.empty()) {
        return Ok(std::vector<std::string>{});
    }

    if (parts[0] == "available") {
        return _get_available_children(path);
    } else if (parts[0] == "opened") {
        return _get_opened_children(path);
    }

    return Ok(std::vector<std::string>{});
}

Result<Dict> FilesystemManager::get_metadata(const DataPath& path) {
    std::string path_str = path.to_string();
    if (!path_str.empty() && path_str[0] != '/') {
        path_str = "/" + path_str;
    }

    // Root
    if (path_str == "/" || path_str.empty()) {
        return Ok(Dict{
            {"name", Value("filesystem")},
            {"label", Value("Filesystem")},
            {"type", Value("filesystem-manager")},
            {"category", Value("audio-device-manager")},
            {"description", Value("Browse and open files from filesystem")}
        });
    }

    const auto& parts = path.as_list();
    if (parts.empty()) {
        return Ok(Dict{});
    }

    if (parts[0] == "available") {
        return _get_available_metadata(path);
    } else if (parts[0] == "opened") {
        return _get_opened_metadata(path);
    }

    return Ok(Dict{});
}

Result<std::vector<std::string>> FilesystemManager::get_metadata_keys(const DataPath& path) {
    auto res = get_metadata(path);
    if (!res) return Err<std::vector<std::string>>("get_metadata_keys failed", res);

    std::vector<std::string> keys;
    for (const auto& [k, _] : *res) {
        keys.push_back(k);
    }
    return Ok(keys);
}

Result<Value> FilesystemManager::get(const DataPath& path) {
    auto parent = path.dirname();
    auto key = path.filename();

    auto meta_res = get_metadata(parent);
    if (!meta_res) return Err<Value>("get failed", meta_res);

    auto& meta = *meta_res;
    auto it = meta.find(key);
    if (it != meta.end()) {
        return Ok(it->second);
    }
    return Ok(Value{});
}

Result<void> FilesystemManager::set(const DataPath& path, const Value& value) {
    return Err<void>("FilesystemManager: set not implemented");
}

Result<void> FilesystemManager::add_child(const DataPath& path, const std::string& name, const Dict& data) {
    return Err<void>("FilesystemManager: add_child not implemented");
}

Result<std::string> FilesystemManager::as_tree(const DataPath& path, int depth) {
    return Ok(path.to_string());
}

std::string FilesystemManager::_map_virtual_to_real(const std::string& path_str) {
    // Check if it's a virtual shortcut root
    auto it = _virtual_shortcuts.find(path_str);
    if (it != _virtual_shortcuts.end()) {
        return it->second;
    }

    // Check if path starts with a virtual shortcut
    for (const auto& [virtual_path, real_path] : _virtual_shortcuts) {
        if (path_str.rfind(virtual_path, 0) == 0) {  // starts_with
            std::string suffix = path_str.substr(virtual_path.length());
            return real_path + suffix;
        }
    }

    return path_str;
}

std::map<std::string, Dict> FilesystemManager::_parse_mounts() {
    std::map<std::string, Dict> mounts;

    std::ifstream mounts_file("/proc/self/mounts");
    if (!mounts_file.is_open()) {
        return mounts;
    }

    std::string line;
    while (std::getline(mounts_file, line)) {
        std::istringstream iss(line);
        std::string device, mountpoint, fstype;
        if (!(iss >> device >> mountpoint >> fstype)) {
            continue;
        }

        // Filter out virtual/system filesystems
        static const std::set<std::string> virtual_fs = {
            "proc", "sysfs", "devpts", "devtmpfs", "tmpfs",
            "cgroup", "cgroup2", "pstore", "bpf", "configfs",
            "debugfs", "tracefs", "securityfs", "fusectl",
            "fuse.gvfsd-fuse", "fuse.portal"
        };
        if (virtual_fs.count(fstype) > 0) {
            continue;
        }

        // Filter out special device names
        if (device.rfind("proc", 0) == 0 || device.rfind("sysfs", 0) == 0 ||
            device.rfind("devpts", 0) == 0 || device.rfind("tmpfs", 0) == 0 ||
            device.rfind("cgroup", 0) == 0) {
            continue;
        }

        // Add valid mount points (except root)
        if (!mountpoint.empty() && mountpoint != "/") {
            mounts[mountpoint] = Dict{
                {"device", Value(device)},
                {"fstype", Value(fstype)}
            };
        }
    }

    return mounts;
}

Result<std::vector<std::string>> FilesystemManager::_get_available_children(const DataPath& path) {
    const auto& parts = path.as_list();

    // /available - return virtual shortcuts
    if (parts.size() == 1) {
        return Ok(std::vector<std::string>{"fs-root", "home", "mounts", "bookmarks"});
    }

    // Build subpath (after "available")
    std::string subpath = "/";
    for (size_t i = 1; i < parts.size(); ++i) {
        subpath += parts[i];
        if (i < parts.size() - 1) subpath += "/";
    }

    // /available/mounts - list mounted filesystems
    if (subpath == "/mounts") {
        auto mounts = _parse_mounts();
        std::vector<std::string> children;
        for (const auto& [mp, _] : mounts) {
            // Return just the mount path, strip leading /
            children.push_back(mp.substr(1));
        }
        return Ok(children);
    }

    // /available/bookmarks - placeholder
    if (subpath == "/bookmarks") {
        return Ok(std::vector<std::string>{});
    }

    // Check if under /mounts
    std::string fs_path;
    if (subpath.rfind("/mounts/", 0) == 0) {
        // Extract real path: /mounts/boot/efi -> /boot/efi
        fs_path = subpath.substr(7);  // Strip "/mounts"
    } else {
        // Map virtual path to real
        fs_path = _map_virtual_to_real(subpath);
    }

    // Check if path exists and is directory
    std::error_code ec;
    if (!fs::exists(fs_path, ec) || !fs::is_directory(fs_path, ec)) {
        return Ok(std::vector<std::string>{});
    }

    // List directory contents
    std::vector<std::string> children;
    try {
        for (const auto& entry : fs::directory_iterator(fs_path, ec)) {
            if (!ec) {
                children.push_back(entry.path().filename().string());
            }
        }
        std::sort(children.begin(), children.end());
    } catch (const fs::filesystem_error& e) {
        // Permission denied or other error
        spdlog::debug("FilesystemManager: cannot list {}: {}", fs_path, e.what());
    }

    return Ok(children);
}

Result<std::vector<std::string>> FilesystemManager::_get_opened_children(const DataPath& path) {
    // Placeholder - no opened devices yet
    return Ok(std::vector<std::string>{});
}

Result<Dict> FilesystemManager::_get_available_metadata(const DataPath& path) {
    const auto& parts = path.as_list();

    // /available
    if (parts.size() == 1) {
        return Ok(Dict{
            {"name", Value("available")},
            {"label", Value("Available")},
            {"type", Value("folder")},
            {"category", Value("folder")}
        });
    }

    // Build subpath (after "available")
    std::string subpath = "/";
    for (size_t i = 1; i < parts.size(); ++i) {
        subpath += parts[i];
        if (i < parts.size() - 1) subpath += "/";
    }

    // Virtual shortcuts
    if (subpath == "/fs-root") {
        return Ok(Dict{
            {"name", Value("fs-root")},
            {"label", Value("filesystem-root")},
            {"type", Value("shortcut")},
            {"category", Value("shortcut")},
            {"details", Value(Dict{{"fs-path", Value("/")}})}
        });
    }

    if (subpath == "/home") {
        return Ok(Dict{
            {"name", Value("home")},
            {"label", Value("home-dir")},
            {"type", Value("shortcut")},
            {"category", Value("shortcut")},
            {"details", Value(Dict{{"fs-path", Value(_virtual_shortcuts["/home"])}})}
        });
    }

    if (subpath == "/mounts") {
        return Ok(Dict{
            {"name", Value("mounts")},
            {"label", Value("mounts")},
            {"type", Value("folder")},
            {"category", Value("folder")},
            {"description", Value("Mounted filesystems")}
        });
    }

    if (subpath == "/bookmarks") {
        return Ok(Dict{
            {"name", Value("bookmarks")},
            {"label", Value("bookmarks")},
            {"type", Value("folder")},
            {"category", Value("folder")},
            {"description", Value("Bookmarked locations")}
        });
    }

    // Check if under /mounts
    std::string fs_path;
    if (subpath.rfind("/mounts/", 0) == 0) {
        fs_path = subpath.substr(7);  // Strip "/mounts"
    } else {
        fs_path = _map_virtual_to_real(subpath);
    }

    std::error_code ec;
    if (!fs::exists(fs_path, ec)) {
        std::string basename = fs::path(fs_path).filename().string();
        if (basename.empty()) basename = fs_path;
        return Ok(Dict{
            {"name", Value(basename)},
            {"label", Value(basename)},
            {"type", Value("error")},
            {"category", Value("error")},
            {"description", Value("Path does not exist")}
        });
    }

    std::string basename = fs::path(fs_path).filename().string();
    if (basename.empty()) basename = fs_path;

    if (fs::is_directory(fs_path, ec)) {
        return Ok(Dict{
            {"name", Value(basename)},
            {"label", Value(basename)},
            {"type", Value("folder")},
            {"category", Value("folder")},
            {"details", Value(Dict{{"fs-path", Value(fs_path)}})}
        });
    }

    // It's a file
    return Ok(Dict{
        {"name", Value(basename)},
        {"label", Value(basename)},
        {"type", Value("file")},
        {"category", Value("file")},
        {"details", Value(Dict{{"fs-path", Value(fs_path)}})}
    });
}

Result<Dict> FilesystemManager::_get_opened_metadata(const DataPath& path) {
    const auto& parts = path.as_list();

    // /opened
    if (parts.size() == 1) {
        return Ok(Dict{
            {"name", Value("opened")},
            {"label", Value("Opened")},
            {"type", Value("folder")},
            {"category", Value("folder")},
            {"description", Value("Opened files")}
        });
    }

    return Ok(Dict{});
}

} // namespace ymery::plugins
