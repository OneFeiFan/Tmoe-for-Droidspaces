#include "domain/system/config_manager.h"
#include "core/command_builder.hpp"
#include "core/i18n.h"
#include "domain/system/package_manager.h"
#include <algorithm>
#include <fstream>
#include <sstream>

#include "core/system_helper.h"

namespace tmoe::domain {

// ═══════════════════════════════════════════════════════════════════
// 构造
// ═══════════════════════════════════════════════════════════════════

ConfigManager::ConfigManager(const TmoeConfig& cfg) : cfg_(cfg) {}

std::string ConfigManager::config_dir() const {
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
    return home + "/.config/tmoe-linux";
}

std::string ConfigManager::read_config_file(const std::string& path) const {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return "";
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

bool ConfigManager::write_config_file(const std::string& path, const std::string& content) const {
    return SystemHelper::write_file(path, content);
}

// ═══════════════════════════════════════════════════════════════════
// DNS 注册表
// ═══════════════════════════════════════════════════════════════════

const std::vector<ConfigManager::DnsEntry>& ConfigManager::dns_registry() {
    static const std::vector<DnsEntry> registry = {
        {"cloudflare", "dns.cloudflare", "1.1.1.1", "1.0.0.1"},
        {"google",     "dns.google",     "8.8.8.8", "8.8.4.4"},
        {"ali",        "dns.ali",        "223.5.5.5", "223.6.6.6"},
        {"dnspod",     "dns.dnspod",     "114.114.114.114", "114.114.115.115"},
        {"baidu",      "dns.baidu",      "180.76.76.76", ""},
        {"tuna",       "dns.tuna",       "101.6.6.6", "166.111.8.28"},
    };
    return registry;
}

bool ConfigManager::configure_dns() {
    const auto& dns_list = dns_registry();

    std::string menu_cmd = cfg_.tui_bin + " --title \"" + _("dns.title") + "\""
                           " --menu \"" + _("dns.menu_prompt") + "\" 0 0 0 ";
    for (size_t i = 0; i < dns_list.size(); ++i) {
        menu_cmd += "\"" + std::to_string(i + 1) + "\" \""
                 + _(dns_list[i].name_key) + " (" + dns_list[i].primary + ")\" ";
    }
    menu_cmd += "\"0\" \"" + _("menu.tui.back") + "\"";

    auto choice = Executor::tui_select(menu_cmd);
    if (choice == "0" || choice.empty()) return false;

    int idx = std::stoi(choice) - 1;
    if (idx >= 0 && idx < static_cast<int>(dns_list.size())) {
        return apply_dns(dns_list[idx].id);
    }
    return false;
}

bool ConfigManager::apply_dns(const std::string& provider_id) {
    const auto& dns_list = dns_registry();
    const DnsEntry* entry = nullptr;
    for (const auto& d : dns_list) {
        if (d.id == provider_id) { entry = &d; break; }
    }
    if (!entry) return false;

    Logger::step(_("dns.applying"));

    std::string resolv;
    resolv += "nameserver " + entry->primary + "\n";
    if (!entry->secondary.empty()) {
        resolv += "nameserver " + entry->secondary + "\n";
    }

    // 备份当前 resolv.conf
    if (fs::exists("/etc/resolv.conf")) {
        Executor::shell(CommandBuilder("sudo").add_arg("cp").add_flag("-f").add_arg("/etc/resolv.conf").add_arg("/etc/resolv.conf.tmoe.bak").add_raw("2>/dev/null").build_string());
    }

    bool ok = write_config_file("/etc/resolv.conf", resolv);
    if (ok) {
        Logger::ok(_f("dns.applied", entry->primary));
        // 持久化到 tmoe 配置
        write_config_file(config_dir() + "/dns.conf", provider_id);
    } else {
        Logger::error(_("dns.failed"));
    }
    return ok;
}

// ═══════════════════════════════════════════════════════════════════
// 时区注册表
// ═══════════════════════════════════════════════════════════════════

const std::vector<std::pair<std::string, std::vector<ConfigManager::TzEntry>>>& ConfigManager::tz_registry() {
    static const std::vector<std::pair<std::string, std::vector<TzEntry>>> registry = {
        {"tz.region.asia", {
            {"Asia/Shanghai",   "tz.shanghai"},
            {"Asia/Tokyo",      "tz.tokyo"},
            {"Asia/Seoul",      "tz.seoul"},
            {"Asia/Singapore",  "tz.singapore"},
            {"Asia/Hong_Kong",  "tz.hongkong"},
            {"Asia/Taipei",     "tz.taipei"},
            {"Asia/Kolkata",    "tz.kolkata"},
            {"Asia/Dubai",      "tz.dubai"},
            {"Asia/Bangkok",    "tz.bangkok"},
            {"Asia/Jakarta",    "tz.jakarta"},
        }},
        {"tz.region.europe", {
            {"Europe/London",   "tz.london"},
            {"Europe/Paris",    "tz.paris"},
            {"Europe/Berlin",   "tz.berlin"},
            {"Europe/Moscow",   "tz.moscow"},
            {"Europe/Madrid",   "tz.madrid"},
            {"Europe/Rome",     "tz.rome"},
            {"Europe/Amsterdam","tz.amsterdam"},
        }},
        {"tz.region.america", {
            {"America/New_York",    "tz.newyork"},
            {"America/Chicago",     "tz.chicago"},
            {"America/Denver",      "tz.denver"},
            {"America/Los_Angeles", "tz.losangeles"},
            {"America/Sao_Paulo",   "tz.saopaulo"},
            {"America/Toronto",     "tz.toronto"},
            {"America/Mexico_City", "tz.mexicocity"},
        }},
        {"tz.region.pacific", {
            {"Pacific/Auckland", "tz.auckland"},
            {"Pacific/Fiji",     "tz.fiji"},
            {"Pacific/Honolulu", "tz.honolulu"},
        }},
        {"tz.region.africa", {
            {"Africa/Cairo",      "tz.cairo"},
            {"Africa/Johannesburg","tz.johannesburg"},
            {"Africa/Lagos",      "tz.lagos"},
        }},
        {"tz.region.etc", {
            {"UTC", "tz.utc"},
        }},
    };
    return registry;
}

bool ConfigManager::configure_timezone() {
    std::string current = detect_current_timezone();
    Logger::info(_("tz.current") + ": " + current);

    const auto& regions = tz_registry();

    // 第一层: 选择区域
    std::string region_menu = cfg_.tui_bin + " --title \"" + _("tz.title") + "\""
                              " --menu \"" + _("tz.select_region") + "\" 0 0 0 ";
    for (size_t i = 0; i < regions.size(); ++i) {
        region_menu += "\"" + std::to_string(i + 1) + "\" \"" + _(regions[i].first) + "\" ";
    }
    region_menu += "\"0\" \"" + _("menu.tui.back") + "\"";

    auto region_choice = Executor::tui_select(region_menu);
    if (region_choice == "0" || region_choice.empty()) return false;

    int ri = std::stoi(region_choice) - 1;
    if (ri < 0 || ri >= static_cast<int>(regions.size())) return false;

    // 第二层: 选择具体城市
    std::string city_menu = cfg_.tui_bin + " --title \"" + _(regions[ri].first) + "\""
                            " --menu \"" + _("tz.select_city") + "\" 0 0 0 ";
    for (size_t j = 0; j < regions[ri].second.size(); ++j) {
        city_menu += "\"" + std::to_string(j + 1) + "\" \""
                  + _(regions[ri].second[j].name_key) + " (" + regions[ri].second[j].zone + ")\" ";
    }
    city_menu += "\"0\" \"" + _("menu.tui.back") + "\"";

    auto city_choice = Executor::tui_select(city_menu);
    if (city_choice == "0" || city_choice.empty()) return false;

    int ci = std::stoi(city_choice) - 1;
    if (ci >= 0 && ci < static_cast<int>(regions[ri].second.size())) {
        return apply_timezone(regions[ri].second[ci].zone);
    }
    return false;
}

bool ConfigManager::apply_timezone(const std::string& tz) {
    Logger::step(_f("tz.applying", tz));

    bool ok = false;
    if (Executor::has("timedatectl")) {
        ok = Executor::shell(CommandBuilder("sudo").add_arg("timedatectl").add_arg("set-timezone").add_arg(tz).build_string()).ok();
    }
    if (!ok) {
        ok = Executor::shell(CommandBuilder("sudo").add_arg("ln").add_flag("-sf").add_arg("/usr/share/zoneinfo/" + tz).add_arg("/etc/localtime").add_raw("2>/dev/null").build_string()).ok();
        if (ok) {
            write_config_file("/etc/timezone", tz);
        }
    }

    if (ok) {
        Logger::ok(_f("tz.applied", tz));
        write_config_file(config_dir() + "/timezone.txt", tz);
    } else {
        Logger::error(_("tz.failed"));
    }
    return ok;
}

std::string ConfigManager::detect_current_timezone() const {
    // 方法1: timedatectl
    auto r = Executor::shell(CommandBuilder("timedatectl").add_arg("show").add_arg("--property=Timezone").add_arg("--value").add_raw("2>/dev/null").build_string());
    if (r.ok() && !r.stdout_data.empty()) {
        std::string tz = r.stdout_data;
        tz.erase(std::remove(tz.begin(), tz.end(), '\n'), tz.end());
        tz.erase(std::remove(tz.begin(), tz.end(), '\r'), tz.end());
        if (!tz.empty()) return tz;
    }

    // 方法2: 读取 /etc/timezone
    if (fs::exists("/etc/timezone")) {
        auto content = read_config_file("/etc/timezone");
        if (!content.empty()) {
            content.erase(std::remove(content.begin(), content.end(), '\n'), content.end());
            return content;
        }
    }

    // 方法3: 读取 /etc/localtime 软链接
    r = Executor::shell(CommandBuilder("readlink").add_flag("-f").add_arg("/etc/localtime").add_raw("2>/dev/null").build_string());
    if (r.ok() && !r.stdout_data.empty()) {
        std::string path = r.stdout_data;
        path.erase(std::remove(path.begin(), path.end(), '\n'), path.end());
        size_t pos = path.find("zoneinfo/");
        if (pos != std::string::npos) {
            return path.substr(pos + 9);
        }
    }

    // 方法4: Android getprop
    r = Executor::shell(CommandBuilder("getprop").add_arg("persist.sys.timezone").add_raw("2>/dev/null").build_string());
    if (r.ok() && !r.stdout_data.empty()) {
        std::string tz = r.stdout_data;
        tz.erase(std::remove(tz.begin(), tz.end(), '\n'), tz.end());
        if (!tz.empty()) return tz;
    }

    return "UTC";
}

// ═══════════════════════════════════════════════════════════════════
// Locale 注册表
// ═══════════════════════════════════════════════════════════════════

const std::vector<std::pair<std::string, std::vector<std::string>>>& ConfigManager::locale_registry() {
    static const std::vector<std::pair<std::string, std::vector<std::string>>> registry = {
        {"locale.region.asia", {
            "zh_CN.UTF-8", "zh_TW.UTF-8", "zh_HK.UTF-8",
            "ja_JP.UTF-8", "ko_KR.UTF-8",
            "th_TH.UTF-8", "vi_VN.UTF-8",
            "hi_IN.UTF-8", "bn_IN.UTF-8",
        }},
        {"locale.region.europe", {
            "en_GB.UTF-8", "en_IE.UTF-8",
            "de_DE.UTF-8", "de_AT.UTF-8", "de_CH.UTF-8",
            "fr_FR.UTF-8", "fr_BE.UTF-8", "fr_CH.UTF-8",
            "es_ES.UTF-8", "es_MX.UTF-8",
            "it_IT.UTF-8", "pt_PT.UTF-8", "pt_BR.UTF-8",
            "ru_RU.UTF-8", "nl_NL.UTF-8", "sv_SE.UTF-8",
            "pl_PL.UTF-8", "cs_CZ.UTF-8", "hu_HU.UTF-8",
            "fi_FI.UTF-8", "da_DK.UTF-8", "nb_NO.UTF-8",
            "el_GR.UTF-8", "tr_TR.UTF-8", "uk_UA.UTF-8",
        }},
        {"locale.region.america", {
            "en_US.UTF-8", "en_CA.UTF-8",
        }},
        {"locale.region.africa", {
            "ar_EG.UTF-8", "ar_SA.UTF-8",
            "af_ZA.UTF-8",
        }},
        {"locale.region.other", {
            "C.UTF-8",
        }},
    };
    return registry;
}

bool ConfigManager::configure_locale() {
    Environment env(cfg_);
    const auto& regions = locale_registry();

    // 检测当前 locale
    std::string current_locale = "en_US.UTF-8";
    const char* env_lang = std::getenv("LANG");
    if (env_lang) current_locale = env_lang;
    Logger::info(_("locale.current") + ": " + current_locale);

    // 第一层: 选择区域
    std::string region_menu = cfg_.tui_bin + " --title \"" + _("locale.title") + "\""
                              " --menu \"" + _("locale.select_region") + "\" 0 0 0 ";
    for (size_t i = 0; i < regions.size(); ++i) {
        region_menu += "\"" + std::to_string(i + 1) + "\" \"" + _(regions[i].first) + "\" ";
    }
    region_menu += "\"0\" \"" + _("menu.tui.back") + "\"";

    auto region_choice = Executor::tui_select(region_menu);
    if (region_choice == "0" || region_choice.empty()) return false;

    int ri = std::stoi(region_choice) - 1;
    if (ri < 0 || ri >= static_cast<int>(regions.size())) return false;

    // 第二层: 选择具体 locale
    std::string locale_menu = cfg_.tui_bin + " --title \"" + _(regions[ri].first) + "\""
                              " --menu \"" + _("locale.select_prompt") + "\" 0 0 0 ";
    for (size_t j = 0; j < regions[ri].second.size(); ++j) {
        locale_menu += "\"" + std::to_string(j + 1) + "\" \""
                    + regions[ri].second[j] + "\" ";
    }
    locale_menu += "\"0\" \"" + _("menu.tui.back") + "\"";

    auto locale_choice = Executor::tui_select(locale_menu);
    if (locale_choice == "0" || locale_choice.empty()) return false;

    int li = std::stoi(locale_choice) - 1;
    if (li >= 0 && li < static_cast<int>(regions[ri].second.size())) {
        std::string loc = regions[ri].second[li];
        Logger::step(_f("locale.applying", loc));

        // 写入 tmoe 配置
        write_config_file(config_dir() + "/locale.txt", loc);

        // 调用 Environment 进行系统级设置
        bool ok = env.set_locale(loc);
        if (ok) ok = env.apply_system_locale(loc);
        if (ok) {
            Logger::ok(_f("locale.applied", loc));
        } else {
            Logger::error(_("locale.failed"));
        }
        return ok;
    }
    return false;
}

std::vector<std::pair<std::string, std::string>> ConfigManager::list_supported_locales() const {
    std::vector<std::pair<std::string, std::string>> result;

    // 尝试从 /usr/share/i18n/SUPPORTED 读取
    if (fs::exists("/usr/share/i18n/SUPPORTED")) {
        auto content = read_config_file("/usr/share/i18n/SUPPORTED");
        std::stringstream ss(content);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty() || line[0] == '#') continue;
            size_t space = line.find(' ');
            if (space != std::string::npos) {
                result.emplace_back(line.substr(0, space), line.substr(space + 1));
            }
        }
    }

    // 回退：使用内置注册表
    if (result.empty()) {
        const auto& regions = locale_registry();
        for (const auto& [region_name, locales] : regions) {
            for (const auto& loc : locales) {
                result.emplace_back(loc, loc);
            }
        }
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════════
// Fortune / Hitokoto
// ═══════════════════════════════════════════════════════════════════

bool ConfigManager::configure_fortune() {
    std::string menu_cmd = cfg_.tui_bin + " --title \"" + _("fortune.title") + "\""
                           " --menu \"" + _("fortune.menu_prompt") + "\" 0 0 0 "
                           "\"1\" \"" + _("fortune.install") + "\" "
                           "\"2\" \"" + _("fortune.hitokoto_enable") + "\" "
                           "\"3\" \"" + _("fortune.hitokoto_disable") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";

    auto choice = Executor::tui_select(menu_cmd);
    if (choice == "0" || choice.empty()) return false;

    if (choice == "1") {
        return install_fortune();
    } else if (choice == "2") {
        return toggle_hitokoto();
    } else if (choice == "3") {
        write_config_file(config_dir() + "/hitokoto.conf", "TMOE_CONTAINER_HITOKOTO=false");
        Logger::ok(_("fortune.hitokoto_disabled"));
        return true;
    }
    return false;
}

bool ConfigManager::install_fortune() {
    Logger::step(_("fortune.installing"));

    // 检测当前 locale 语言
    std::string lang = cfg_.locale.substr(0, 2);
    auto family = infer_family_from_config(cfg_.linux_distro);

    bool ok = false;
    switch (family) {
        case DistroFamily::Debian:
            ok = PackageManager::install({"fortune-mod", "fortunes"}, family);
            if (ok && (lang == "zh" || lang == "ja" || lang == "ko")) {
                PackageManager::install("fortunes-" + lang, family);
            }
            break;
        case DistroFamily::Arch:
            ok = PackageManager::install("fortune-mod", family);
            break;
        case DistroFamily::Alpine:
            ok = PackageManager::install("fortune", family);
            break;
        case DistroFamily::Gentoo:
            ok = PackageManager::install("games-misc/fortune-mod", family);
            break;
        default:
            ok = PackageManager::install("fortune-mod", family);
            break;
    }

    if (ok) {
        Logger::ok(_("fortune.install_ok"));
        // 写入配置
        write_config_file(config_dir() + "/fortune.conf", "TMOE_CONTAINER_FORTUNE=true");
    } else {
        Logger::warn(_("fortune.install_failed"));
    }
    return ok;
}

bool ConfigManager::toggle_hitokoto() {
    Logger::step(_("fortune.hitokoto_enabling"));

    // 确保 curl 可用
    if (!Executor::has("curl") && !Executor::has("wget")) {
        Logger::warn(_("fortune.no_curl"));
        return false;
    }

    write_config_file(config_dir() + "/hitokoto.conf", "TMOE_CONTAINER_HITOKOTO=true");
    Logger::ok(_("fortune.hitokoto_enabled"));
    return true;
}

// ═══════════════════════════════════════════════════════════════════
// 共享目录
// ═══════════════════════════════════════════════════════════════════

bool ConfigManager::configure_shared_dirs() {
    const std::string root_type = cfg_.is_root ? "rootful" : "rootless";
    const std::string base = config_dir() + "/" + root_type + "/";

    // 读取当前状态
    auto read_flag = [&](const std::string& name) -> bool {
        auto content = read_config_file(base + name);
        return content.find("true") != std::string::npos;
    };

    bool sd_on = read_flag("mount_sd.conf");
    bool home_on = read_flag("mount_termux.conf");
    bool storage_on = read_flag("mount_storage.conf");
    bool tf_on = read_flag("mount_tf.conf");

    auto make_item = [](const std::string& label, bool on) -> std::string {
        return "\"" + label + "\" \"" + _(label) + (on ? " [ON]" : " [OFF]") + "\" ";
    };

    std::string menu_cmd = cfg_.tui_bin + " --title \"" + _("sharedir.title") + "\""
                           " --checklist \"" + _("sharedir.menu_prompt") + "\" 0 0 0 "
        + make_item("sharedir.sd", sd_on)
        + make_item("sharedir.termux_home", home_on)
        + make_item("sharedir.storage", storage_on)
        + make_item("sharedir.tf_card", tf_on);

    auto result = Executor::shell(menu_cmd + " 2>&1");
    std::string output = result.stdout_data;

    // whiptail --checklist returns quoted, space-separated tags
    bool sd_new    = output.find("sharedir.sd")          != std::string::npos;
    bool home_new  = output.find("sharedir.termux_home") != std::string::npos;
    bool st_new    = output.find("sharedir.storage")     != std::string::npos;
    bool tf_new    = output.find("sharedir.tf_card")     != std::string::npos;

    if (sd_on != sd_new)       write_mount_conf("mount_sd.conf",       sd_new);
    if (home_on != home_new)   write_mount_conf("mount_termux.conf",   home_new);
    if (storage_on != st_new)  write_mount_conf("mount_storage.conf",  st_new);
    if (tf_on != tf_new)       write_mount_conf("mount_tf.conf",       tf_new);

    if (sd_on != sd_new || home_on != home_new || storage_on != st_new || tf_on != tf_new) {
        Logger::ok(_("sharedir.saved"));
    }
    Logger::press_enter();
    return true;
}

bool ConfigManager::write_mount_conf(const std::string& conf_name, bool enabled) {
    const std::string root_type = cfg_.is_root ? "rootful" : "rootless";
    std::string path = config_dir() + "/" + root_type + "/" + conf_name;
    return write_config_file(path, enabled ? "ENABLED=true" : "ENABLED=false");
}

// ═══════════════════════════════════════════════════════════════════
// 密码管理
// ═══════════════════════════════════════════════════════════════════

bool ConfigManager::change_root_password() {
    Logger::step(_("passwd.title"));

    // 使用 whiptail passwordbox 获取新密码
    std::string pw1_cmd = cfg_.tui_bin + " --title \"" + _("passwd.title") + "\""
                           " --passwordbox \"" + _("passwd.enter") + "\" 0 0";
    std::string pw1 = Executor::tui_select(pw1_cmd);

    if (pw1.empty()) {
        Logger::info(_("passwd.cancelled"));
        return false;
    }

    std::string pw2_cmd = cfg_.tui_bin + " --title \"" + _("passwd.title") + "\""
                           " --passwordbox \"" + _("passwd.confirm") + "\" 0 0";
    std::string pw2 = Executor::tui_select(pw2_cmd);

    if (pw1 != pw2) {
        Logger::error(_("passwd.mismatch"));
        return false;
    }

    // 通过 chpasswd 修改 (需要 root)
    std::string chpasswd_input = "root:" + pw1;
    auto result = Executor::shell(CommandBuilder("echo").add_arg(chpasswd_input).add_raw("| sudo chpasswd 2>/dev/null").build_string());

    if (result.ok()) {
        Logger::ok(_("passwd.changed"));
        return true;
    } else {
        // fallback: passwd --stdin
        result = Executor::shell(CommandBuilder("echo").add_arg(pw1).add_raw("| sudo passwd --stdin root 2>/dev/null").build_string());
        if (result.ok()) {
            Logger::ok(_("passwd.changed"));
            return true;
        }
        Logger::error(_("passwd.failed"));
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════
// 主机名
// ═══════════════════════════════════════════════════════════════════

bool ConfigManager::configure_hostname() {
    std::string current = current_hostname();
    Logger::info(_("hostname.current") + ": " + current);

    std::string input_cmd = cfg_.tui_bin + " --title \"" + _("hostname.title") + "\""
                            " --inputbox \"" + _("hostname.prompt") + "\" 0 0 \"" + current + "\"";
    std::string new_hostname = Executor::tui_select(input_cmd);

    if (new_hostname.empty() || new_hostname == current) {
        return false;
    }

    bool ok = true;
    if (Executor::has("hostnamectl")) {
        ok = Executor::shell(CommandBuilder("sudo").add_arg("hostnamectl").add_arg("set-hostname").add_arg(new_hostname).build_string()).ok();
    } else {
        ok = write_config_file("/etc/hostname", new_hostname + "\n");
        if (ok) {
            Executor::shell(CommandBuilder("sudo").add_arg("hostname").add_arg(new_hostname).add_raw("2>/dev/null || true").build_string());
        }
    }

    if (ok) {
        Logger::ok(_f("hostname.set", new_hostname));
    } else {
        Logger::error(_("hostname.failed"));
    }
    return ok;
}

std::string ConfigManager::current_hostname() const {
    auto r = Executor::shell(CommandBuilder("hostname").add_raw("2>/dev/null").build_string());
    if (r.ok() && !r.stdout_data.empty()) {
        std::string h = r.stdout_data;
        h.erase(std::remove(h.begin(), h.end(), '\n'), h.end());
        return h;
    }

    if (fs::exists("/etc/hostname")) {
        auto h = read_config_file("/etc/hostname");
        h.erase(std::remove(h.begin(), h.end(), '\n'), h.end());
        if (!h.empty()) return h;
    }

    return "localhost";
}

    } // namespace tmoe::domain
