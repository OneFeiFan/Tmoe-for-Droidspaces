//
// Created by WorkBuddy on 2026/5/26.
//
#include "mirror_registry.h"
#include "mirror_data.h"
#include <algorithm>

namespace tmoe::domain {

MirrorRegistry& MirrorRegistry::instance() {
    static MirrorRegistry inst;
    return inst;
}

MirrorRegistry::MirrorRegistry() {
    try {
        data_ = nlohmann::json::parse(gen::MIRROR_REGISTRY_JSON);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error(std::string("Failed to parse mirror_registry.json: ") + e.what());
    }
}

std::vector<MirrorEntry> MirrorRegistry::by_category(const std::string& category) const {
    std::vector<MirrorEntry> result;
    for (const auto& el : data_["mirrors"]) {
        if (el.value("category", "") == category) {
            MirrorEntry entry;
            entry.id       = el.value("id", "");
            entry.name     = el.value("name", "");
            entry.url      = el.value("url", "");
            entry.category = el.value("category", "");
            entry.region   = el.value("region", "");
            entry.note     = el.value("note", "");
            result.push_back(std::move(entry));
        }
    }
    return result;
}

std::vector<MirrorEntry> MirrorRegistry::by_region(const std::string& region) const {
    std::vector<MirrorEntry> result;
    for (const auto& el : data_["mirrors"]) {
        if (el.value("region", "") == region) {
            MirrorEntry entry;
            entry.id       = el.value("id", "");
            entry.name     = el.value("name", "");
            entry.url      = el.value("url", "");
            entry.category = el.value("category", "");
            entry.region   = el.value("region", "");
            entry.note     = el.value("note", "");
            result.push_back(std::move(entry));
        }
    }
    return result;
}

std::vector<MirrorEntry> MirrorRegistry::all() const {
    std::vector<MirrorEntry> result;
    for (const auto& el : data_["mirrors"]) {
        MirrorEntry entry;
        entry.id       = el.value("id", "");
        entry.name     = el.value("name", "");
        entry.url      = el.value("url", "");
        entry.category = el.value("category", "");
        entry.region   = el.value("region", "");
        entry.note     = el.value("note", "");
        result.push_back(std::move(entry));
    }
    return result;
}

std::optional<MirrorEntry> MirrorRegistry::find(const std::string& id) const {
    for (const auto& el : data_["mirrors"]) {
        if (el.value("id", "") == id) {
            MirrorEntry entry;
            entry.id       = el.value("id", "");
            entry.name     = el.value("name", "");
            entry.url      = el.value("url", "");
            entry.category = el.value("category", "");
            entry.region   = el.value("region", "");
            entry.note     = el.value("note", "");
            return entry;
        }
    }
    return std::nullopt;
}

std::string MirrorRegistry::default_mirror() const {
    return data_.value("default_mirror", "tuna");
}

std::optional<MirrorCompatInfo> MirrorRegistry::compat_for(const std::string& distro) const {
    if (!data_["compat"].contains(distro)) {
        return std::nullopt;
    }

    const auto& c = data_["compat"][distro];
    MirrorCompatInfo info;
    info.source_file      = c.value("source_file", "");
    info.backup_path      = c.value("backup_path", "");
    info.source_conf      = c.value("source_conf", "");
    info.backup_conf      = c.value("backup_conf", "");
    info.security_official = c.value("security_official", "");
    info.arm_repo         = c.value("arm_repo", "");
    info.x86_repo         = c.value("x86_repo", "");
    info.arm_path         = c.value("arm_path", "");
    info.rolling_branch    = c.value("rolling_branch", "");
    info.source_dir       = c.value("source_dir", "");

    if (c.contains("repos")) {
        info.repos = c["repos"].get<std::vector<std::string>>();
    }
    return info;
}

std::string MirrorRegistry::build_source_url(const MirrorEntry& mirror,
                                              const std::string& distro,
                                              const std::string& codename) {
    // 产出各发行版标准源地址，供 sources.list 等文件使用
    if (distro == "debian") {
        return "http://" + mirror.url + "/debian/ " + codename + " main contrib non-free";
    }
    if (distro == "ubuntu") {
        return "http://" + mirror.url + "/ubuntu/ " + codename + " main restricted universe multiverse";
    }
    if (distro == "kali") {
        return "http://" + mirror.url + "/kali/ " + codename + " main contrib non-free";
    }
    return "";
}

} // namespace tmoe::domain
