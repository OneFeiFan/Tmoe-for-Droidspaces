#include "virtualization.h"
#include "core/i18n.h"

namespace tmoe::domain {

VirtualizationManager::VirtualizationManager(const TmoeConfig &cfg) : cfg_(cfg) {
    // 对应 Bash: DOWNLOAD_PATH="${HOME}/sd/Download"
    const char* home = std::getenv("HOME");
    if (home && cfg_.is_wsl) {
        download_path_ = "/mnt/c/Users/Public/Documents";
    } else if (home) {
        download_path_ = std::string(home) + "/sd/Download";
    } else {
        download_path_ = "/tmp/tmoe_iso";
    }
}

// ═══════════════════════════════════════════════════════════════
//  下载路径
// ═══════════════════════════════════════════════════════════════

std::string VirtualizationManager::get_download_path() const {
    return download_path_;
}

void VirtualizationManager::ensure_download_path() const {
    fs::create_directories(download_path_);
}

// ═══════════════════════════════════════════════════════════════
//  Whiptail 辅助 — yes/no 对话框，返回 true=yes
// ═══════════════════════════════════════════════════════════════

bool VirtualizationManager::whiptail_yesno(const std::string& title,
                                            const std::string& yes_btn,
                                            const std::string& no_btn,
                                            const std::string& prompt,
                                            int height, int width) {
    std::string cmd = cfg_.tui_bin +
        " --title \"" + title + "\"" +
        " --yes-button \"" + yes_btn + "\"" +
        " --no-button \"" + no_btn + "\"" +
        " --yesno \"" + prompt + "\" " +
        std::to_string(height) + " " + std::to_string(width);
    auto r = Executor::passthrough(cmd);
    return r.exit_code == 0;
}

// ═══════════════════════════════════════════════════════════════
//  通用下载模型
// ═══════════════════════════════════════════════════════════════

bool VirtualizationManager::download_with_curl(const std::string& url,
                                                const std::string& output_path) {
    std::string cmd = "curl -L -o \"" + output_path + "\" \"" + url + "\" 2>&1";
    auto result = Executor::passthrough(cmd);
    return result.ok() && fs::exists(output_path);
}

bool VirtualizationManager::tmoe_iso_download_model(const std::string& iso_filename,
                                                     const std::string& iso_url,
                                                     const std::string& hint) {
    ensure_download_path();
    std::string iso_path = download_path_ + "/" + iso_filename;

    // 对应 Bash: 检测 iso 已下载 → 询问是否重下
    if (fs::exists(iso_path)) {
        Logger::info(_("virt.iso_already_downloaded") + ": " + iso_filename);
        if (!whiptail_yesno(
                _("virt.iso_detected_title"),
                _("virt.btn_keep_use"),       // yes → 保持已有的
                _("virt.btn_redownload"),     // no → 重新下载
                _("virt.iso_detected_prompt") + "\n" + iso_filename)) {
            // 用户选择重下
            Logger::info(_("virt.redownloading"));
        } else {
            Logger::ok(_("virt.iso_keep_existing") + ": " + iso_path);
            return true;
        }
    }

    if (!hint.empty()) {
        Logger::info(hint);
    }

    Logger::step(_("virt.downloading") + ": " + iso_filename);
    Logger::info(_("virt.iso_source") + ": " + iso_url);
    Logger::info(_("virt.iso_save_path") + ": " + iso_path);

    bool ok = download_with_curl(iso_url, iso_path);
    if (ok) {
        auto sz = fs::file_size(iso_path) / (1024 * 1024);
        Logger::ok(_("virt.iso_download_ok") + ": " + iso_path + " (" + std::to_string(sz) + " MB)");
    } else {
        Logger::error(_("virt.iso_download_failed"));
    }
    return ok;
}

// ═══════════════════════════════════════════════════════════════
//  主菜单 — 对应 Bash virt-menu (仅保留 ISO/Docker/Wine)
// ═══════════════════════════════════════════════════════════════

void VirtualizationManager::run_virt_menu() {
    while (true) {
        // 对应 Bash: install_container_and_virtual_machine (53-81行)
        std::string menu = cfg_.tui_bin +
            " --title \"" + _("virt.title") + "\""
            " --menu \"" + _("virt.menu_prompt") + "\" 0 0 0 "
            "\"1\" \"" + _("virt.download_iso") + "\" "     // 📀 iso/qcow2
            "\"2\" \"" + _("virt.docker") + "\" "            // 🐳 docker
            "\"3\" \"" + _("virt.wine") + "\" "              // 🍷 wine
            "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        if (choice == "1") {
            run_qcow2_or_iso_menu();
        } else if (choice == "2") {
            if (docker_cb_) docker_cb_();
        } else if (choice == "3") {
            run_wine_menu();
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  qcow2 / ISO 分支 (对应 Bash choose_qcow2_or_iso, iso.sh:3-9)
// ═══════════════════════════════════════════════════════════════

void VirtualizationManager::run_qcow2_or_iso_menu() {
    // 对应 Bash: choose_qcow2_or_iso → yes=qcow2, no=iso
    if (whiptail_yesno(
            _("virt.qcow2_iso_title"),
            "qcow2", "iso",
            _("virt.qcow2_iso_prompt"))) {
        run_qcow2_templates_menu();
    } else {
        run_iso_menu();
    }
}

// ═══════════════════════════════════════════════════════════════
//  qcow2 预构建模板 (对应 Bash tmoe_qemu_templates_repo, iso.sh:522-554)
// ═══════════════════════════════════════════════════════════════

void VirtualizationManager::run_qcow2_templates_menu() {
    while (true) {
        // 对应 Bash: tmoe_qemu_templates_repo — 5个预构建镜像
        std::string menu = cfg_.tui_bin +
            " --title \"" + _("virt.qcow2_templates_title") + "\""
            " --menu \"" + _("virt.qcow2_templates_prompt") + "\" 0 50 0 "
            "\"1\" \"Alpine-3.12_x64 (213M → 1.1G)\" "
            "\"2\" \"Arch_x64 (1G → 3G)\" "
            "\"3\" \"Debian-bullseye_x64 (766M → 3G)\" "
            "\"4\" \"Ubuntu-focal_x64 (1.4G → 5G)\" "
            "\"5\" \"Kali_x64 (xfce4, 3.3G → 11.6G)\" "
            "\"0\" \"" + _("menu.tui.back") + "\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        switch (std::stoi(choice)) {
        case 1:
            download_qcow2_template(
                "alpine_x64-tmoe_202011",
                "alpine_x64-tmoe_202011.qcow2.tar.xz",
                "alpine_x64-tmoe_202011.qcow2",
                "https://redirect.tmoe.me/down/share/Tmoe-linux/qemu/202011/"
                "alpine_x64-tmoe_202011.qcow2.tar.xz?download=1",
                "213.1MiB → 1.1GiB");
            break;
        case 2:
            download_qcow2_template(
                "arch_x64-tmoe-202011",
                "arch_x64-tmoe-202011.qcow2.tar.xz",
                "arch_x64-tmoe-202011.qcow2",
                "https://redirect.tmoe.me/down/share/Tmoe-linux/qemu/202011/"
                "arch_x64-tmoe-202011.qcow2.tar.xz?download=1",
                "995.2MiB → 2.7GiB");
            break;
        case 3:
            download_qcow2_template(
                "debian_x64-tmoe-202011",
                "debian-bullseye_amd64-20201117_tmoe.tar.xz",
                "debian-bullseye_amd64-20201117_tmoe.qcow2",
                "https://redirect.tmoe.me/down/share/Tmoe-linux/qemu/202011/"
                "debian-bullseye_amd64-20201117_tmoe.tar.xz?download=1",
                "766.6MiB → 3.5GiB");
            break;
        case 4:
            download_qcow2_template(
                "ubuntu_x64-tmoe-202011",
                "ubuntu-focal_amd64-tmoe_20201118.tar.xz",
                "ubuntu-focal_amd64-tmoe_20201118.qcow2",
                "https://redirect.tmoe.me/down/share/Tmoe-linux/qemu/202011/"
                "ubuntu-focal_amd64-tmoe_20201118.tar.xz?download=1",
                "1.4GiB → 5.14GiB");
            break;
        case 5:
            download_qcow2_template(
                "kali_x64-tmoe-202011",
                "kali_linux_x64_tmoe_20201117.tar.xz",
                "kali_linux_x64_tmoe_20201117.qcow2",
                "https://redirect.tmoe.me/down/share/Tmoe-linux/qemu/202011/"
                "kali_linux_x64_tmoe_20201117.tar.xz?download=1",
                "3.3GiB → 11.64GiB");
            break;
        default: break;
        }
        Logger::press_enter();
    }
}

void VirtualizationManager::download_qcow2_template(
        const std::string& name,
        const std::string& dl_filename,
        const std::string& qcow2_filename,
        const std::string& dl_url,
        const std::string& size_hint) {

    ensure_download_path();
    std::string dl_path = download_path_ + "/" + dl_filename;
    std::string qcow2_path = download_path_ + "/" + qcow2_filename;

    // 对应 Bash: check_arch_linux_qemu_qcow2_file
    Logger::info(_("virt.qcow2_size_hint") + ": " + size_hint);
    Logger::info(_("virt.root_passwd_empty"));  // note_of_empty_root_password

    // 检测是否已下载
    if (fs::exists(dl_path)) {
        Logger::info(_("virt.qcow2_already_downloaded") + ": " + dl_filename);
        if (whiptail_yesno(
                _("virt.qcow2_detected_title"),
                _("virt.btn_uncompress"),     // yes → 解压
                _("virt.btn_redownload"),     // no → 重下
                _("virt.qcow2_detected_prompt"))) {
            // 解压 (对应 Bash uncompress_alpine_and_docker_x64_img_file)
            Logger::warn(_("virt.qcow2_uncompress_warning"));
            if (!whiptail_yesno(_("virt.confirm_title"), "yes", "no", _("virt.confirm_continue")))
                return;
        } else {
            // 重新下载
            Logger::info(_("virt.redownloading"));
            if (!download_with_curl(dl_url, dl_path)) {
                Logger::error(_("virt.download_failed"));
                return;
            }
        }
    } else {
        // 首次下载
        if (!whiptail_yesno(_("virt.confirm_download_title"), "yes", "no",
                            _("virt.confirm_download_prompt") + "\n" + size_hint))
            return;
        Logger::info(_("virt.downloading") + ": " + dl_filename);
        if (!download_with_curl(dl_url, dl_path)) {
            Logger::error(_("virt.download_failed"));
            return;
        }
    }

    // 解压 (对应 Bash uncompress_alpine_and_docker_x64_img_file)
    Logger::step(_("virt.uncompressing") + ": " + dl_filename);
    Executor::passthrough("cd \"" + download_path_ + "\" && "
                          "tar -Jpxvf \"" + dl_filename + "\" 2>&1");

    if (fs::exists(qcow2_path)) {
        Logger::ok(_("virt.uncompress_done") + ": " + qcow2_path);
    } else {
        Logger::warn(_("virt.uncompress_maybe_failed"));
    }
}

// ═══════════════════════════════════════════════════════════════
//  ISO 下载菜单 (对应 Bash download_virtual_machine_iso_file, iso.sh:11-38)
// ═══════════════════════════════════════════════════════════════

void VirtualizationManager::run_iso_menu() {
    while (true) {
        // 对应 Bash: download_virtual_machine_iso_file — 6个ISO选项
        std::string menu = cfg_.tui_bin +
            " --title \"" + _("virt.iso_title") + "\""
            " --menu \"" + _("virt.iso_prompt") + "\" 0 50 0 "
            "\"1\" \"alpine (latest-stable)\" "
            "\"2\" \"Android x86_64 (latest)\" "
            "\"3\" \"debian-iso (weekly build, includes non-free)\" "
            "\"4\" \"ubuntu\" "
            "\"5\" \"windows 11\" "
            "\"6\" \"LMDE (Linux Mint Debian Edition)\" "
            "\"0\" \"" + _("menu.tui.back") + "\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        switch (std::stoi(choice)) {
        case 1: download_alpine_iso(); break;
        case 2: download_android_x86_iso(); break;
        case 3: download_debian_iso(); break;
        case 4: download_ubuntu_iso(); break;
        case 5: download_windows_iso(); break;
        case 6: download_lmde_iso(); break;
        default: break;
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  Alpine ISO (对应 Bash download_alpine_virtual_iso, iso.sh:128-159)
//   子选项: 架构(x64/arm64) + 版本(standard/extended/virt/xen)
// ═══════════════════════════════════════════════════════════════

void VirtualizationManager::download_alpine_iso() {
    // 对应 Bash: which_alpine_arch → yes=x64, no=arm64
    bool is_x64 = whiptail_yesno(
        _("virt.alpine_arch_title"),
        "x64", "arm64",
        _("virt.alpine_arch_prompt"));

    // 对应 Bash: WHICH_ALPINE_EDITION — 4个版本
    std::string edition_menu = cfg_.tui_bin +
        " --title \"" + _("virt.alpine_edition_title") + "\""
        " --menu \"" + _("virt.alpine_edition_prompt") + "\" 0 50 0 "
        "\"1\" \"standard (" + _("virt.alpine_standard") + ")\" "
        "\"2\" \"extended (" + _("virt.alpine_extended") + ")\" "
        "\"3\" \"virt (" + _("virt.alpine_virt") + ")\" "
        "\"4\" \"xen (" + _("virt.alpine_xen") + ")\" "
        "\"0\" \"" + _("menu.tui.back") + "\"";

    std::string ed_choice = Executor::tui_select(edition_menu);
    if (ed_choice == "0" || ed_choice.empty()) return;

    std::string arch = is_x64 ? "x86_64" : "aarch64";
    std::string edition;
    switch (std::stoi(ed_choice)) {
    case 1: edition = "standard"; break;
    case 2: edition = "extended"; break;
    case 3: edition = "virt"; break;
    case 4: edition = "xen"; break;
    default: return;
    }

    // 对应 Bash: download_the_latest_alpine_iso_file
    // ALPINE_ISO_REPO + latest-releases.yaml → 解析最新版本
    std::string iso_filename = "alpine-" + edition + "-" + arch + ".iso";
    std::string alpine_repo = "https://mirrors.bfsu.edu.cn/alpine/latest-stable/releases/" + arch + "/";
    std::string iso_url = alpine_repo + "alpine-" + edition + "-" + arch + ".iso";  // 简化为直接 URL

    // 也可以解析 yaml 获取精确版本号，但这里使用已知的命名规律
    Logger::info(_("virt.alpine_mirror") + ": " + alpine_repo);

    tmoe_iso_download_model(iso_filename, iso_url,
                            _("virt.root_passwd_empty"));
}

// ═══════════════════════════════════════════════════════════════
//  Android x86 ISO (对应 Bash download_android_x86_file, iso.sh:248-259)
// ═══════════════════════════════════════════════════════════════

void VirtualizationManager::download_android_x86_iso() {
    // 对应 Bash: 从 osdn 仓库抓取最新版本
    // REPO_URL='https://mirrors.bfsu.edu.cn/osdn/android-x86/'
    // 根据 ARCH_TYPE 决定是否过滤 x86_64

    std::string arch = cfg_.arch;
    if (arch != "i386" && arch != "amd64") arch = "amd64";

    Logger::info(_("virt.android_x86_fetching"));
    std::string iso_filename = "android-x86_" + arch + ".iso";

    // 使用直接链接 (抓取逻辑较复杂时使用简化 URL)
    std::string iso_url = "https://mirrors.bfsu.edu.cn/osdn/android-x86/69704/"
                          "android-x86_64-9.0-r2.iso";

    tmoe_iso_download_model(iso_filename, iso_url,
                            _("virt.android_x86_note"));
}

// ═══════════════════════════════════════════════════════════════
//  Debian ISO (对应 Bash download_debian_iso_file, iso.sh:260-369)
//   子选项: 架构(11种) → free/nonfree → 桌面环境(8种)
// ═══════════════════════════════════════════════════════════════

void VirtualizationManager::download_debian_iso() {
    // 对应 Bash: DEBIAN_ARCH 菜单 — 11个架构
    struct ArchEntry { std::string tag; std::string grep; bool free; bool is_nonfree_menu; };
    std::string arch_menu = cfg_.tui_bin +
        " --title \"" + _("virt.debian_arch_title") + "\""
        " --menu \"" + _("virt.debian_arch_prompt") + "\" 0 50 0 "
        "\"1\" \"x64 (non-free, unofficial)\" "
        "\"2\" \"x86 (non-free, unofficial)\" "
        "\"3\" \"x64 (free)\" "
        "\"4\" \"x86 (free)\" "
        "\"5\" \"arm64\" "
        "\"6\" \"armhf\" "
        "\"7\" \"mips\" "
        "\"8\" \"mipsel\" "
        "\"9\" \"mips64el\" "
        "\"10\" \"ppc64el\" "
        "\"11\" \"s390x\" "
        "\"0\" \"" + _("menu.tui.back") + "\"";

    std::string arch_choice = Executor::tui_select(arch_menu);
    if (arch_choice == "0" || arch_choice.empty()) return;

    int ac = std::stoi(arch_choice);
    std::string grep_arch;
    bool is_nonfree = false;
    bool is_free_menu = false;

    switch (ac) {
    case 1: grep_arch = "amd64"; is_nonfree = true; break;
    case 2: grep_arch = "i386";  is_nonfree = true; break;
    case 3: grep_arch = "amd64"; is_free_menu = true; break;
    case 4: grep_arch = "i386";  is_free_menu = true; break;
    case 5: grep_arch = "arm64"; break;
    case 6: grep_arch = "armhf"; break;
    case 7: grep_arch = "mips"; break;
    case 8: grep_arch = "mipsel"; break;
    case 9: grep_arch = "mips64el"; break;
    case 10: grep_arch = "ppc64el"; break;
    case 11: grep_arch = "s390x"; break;
    default: return;
    }

    // 非 x86/amd64 架构 → 直接下载 weekly builds netinst
    if (ac >= 5) {
        std::string iso_url =
            "https://mirrors.ustc.edu.cn/debian-cdimage/weekly-builds/"
            + grep_arch + "/iso-cd/debian-testing-" + grep_arch + "-netinst.iso";
        std::string iso_filename = "debian-testing-" + grep_arch + "-netinst.iso";
        tmoe_iso_download_model(iso_filename, iso_url);
        return;
    }

    // x86/amd64 → 选择桌面环境 (对应 Bash DEBIAN_LIVE)
    std::string de_menu = cfg_.tui_bin +
        " --title \"" + _("virt.debian_de_title") + "\""
        " --menu \"" + _("virt.debian_de_prompt") + "\" 0 0 0 "
        "\"1\" \"cinnamon\" "
        "\"2\" \"gnome\" "
        "\"3\" \"kde plasma\" "
        "\"4\" \"lxde\" "
        "\"5\" \"lxqt\" "
        "\"6\" \"mate\" "
        "\"7\" \"standard (" + _("virt.debian_standard") + ")\" "
        "\"8\" \"xfce\" "
        "\"0\" \"" + _("menu.tui.back") + "\"";

    std::string de_choice = Executor::tui_select(de_menu);
    if (de_choice == "0" || de_choice.empty()) return;

    static const char* de_names[] = {
        "cinnamon", "gnome", "kde", "lxde", "lxqt", "mate", "standard", "xfce"
    };
    int de_idx = std::stoi(de_choice) - 1;
    if (de_idx < 0 || de_idx >= 8) return;
    std::string de = de_names[de_idx];

    std::string iso_url, iso_filename;
    if (is_nonfree) {
        // 对应 Bash: download_debian_nonfree_live_iso
        iso_url = "https://mirrors.ustc.edu.cn/debian-cdimage/unofficial/non-free/"
                  "cd-including-firmware/weekly-live-builds/" + grep_arch +
                  "/iso-hybrid/debian-live-testing-" + grep_arch + "-" + de + "%2Bnonfree.iso";
        iso_filename = "debian-live-testing-" + grep_arch + "-" + de + "-nonfree.iso";
    } else {
        // 对应 Bash: download_debian_free_live_iso
        iso_url = "https://mirrors.ustc.edu.cn/debian-cdimage/weekly-live-builds/"
                  + grep_arch + "/iso-hybrid/debian-live-testing-" + grep_arch + "-" + de + ".iso";
        iso_filename = "debian-live-testing-" + grep_arch + "-" + de + ".iso";
    }

    tmoe_iso_download_model(iso_filename, iso_url,
                            _("virt.root_passwd_empty"));
}

// ═══════════════════════════════════════════════════════════════
//  Ubuntu ISO (对应 Bash download_ubuntu_iso_file, iso.sh:161-246)
//   子选项: 版本号 + 发行版(ubuntu/kubuntu/xubuntu/lubuntu/mate/server)
// ═══════════════════════════════════════════════════════════════

void VirtualizationManager::download_ubuntu_iso() {
    // 对应 Bash: 选择版本 — yes=20.04, no=自定义
    bool is_2004 = whiptail_yesno(
        _("virt.ubuntu_version_title"),
        "20.04", _("virt.btn_custom"),
        _("virt.ubuntu_version_prompt"));

    std::string ubuntu_ver = "20.04";
    if (!is_2004) {
        // 自定义版本 — 对应 Bash TARGET inputbox
        auto result = Executor::passthrough(
            cfg_.tui_bin +
            " --inputbox \"" + _("virt.ubuntu_custom_version_prompt") + "\" 0 50 "
            "\"20.04\" --title \"" + _("virt.ubuntu_version_title") + "\"");
        if (result.exit_code != 0 || result.stdout_data.empty()) {
            ubuntu_ver = "20.04";
        } else {
            ubuntu_ver = result.stdout_data;
            // trim
            while (!ubuntu_ver.empty() && (ubuntu_ver.back() == '\n' || ubuntu_ver.back() == '\r'))
                ubuntu_ver.pop_back();
        }
    }

    // 对应 Bash: UBUNTU_EDITION — 6个发行版
    std::string edition_menu = cfg_.tui_bin +
        " --title \"" + _("virt.ubuntu_edition_title") + "\""
        " --menu \"" + _("virt.ubuntu_edition_prompt") + "\" 0 50 0 "
        "\"1\" \"ubuntu-server (" + _("virt.ubuntu_auto_arch") + ")\" "
        "\"2\" \"ubuntu (gnome)\" "
        "\"3\" \"xubuntu (xfce)\" "
        "\"4\" \"kubuntu (kde plasma)\" "
        "\"5\" \"lubuntu (lxqt)\" "
        "\"6\" \"ubuntu-mate\" "
        "\"0\" \"" + _("menu.tui.back") + "\"";

    std::string ed_choice = Executor::tui_select(edition_menu);
    if (ed_choice == "0" || ed_choice.empty()) return;

    // 对应 Bash: 架构判断
    std::string arch = cfg_.arch;
    if (arch.empty() || arch == "unknown") arch = "amd64";

    std::string iso_url, iso_filename;

    switch (std::stoi(ed_choice)) {
    case 1:
        // ubuntu-server — 自动识别架构
        if (arch == "amd64")
            iso_url = "https://mirrors.bfsu.edu.cn/ubuntu-cdimage/ubuntu-legacy-server/"
                      "releases/" + ubuntu_ver + "/release/"
                      "ubuntu-" + ubuntu_ver + "-legacy-server-amd64.iso";
        else if (arch == "i386")
            iso_url = "https://mirrors.huaweicloud.com/ubuntu-releases/16.04.6/"
                      "ubuntu-16.04.6-server-i386.iso";
        else
            iso_url = "https://mirrors.bfsu.edu.cn/ubuntu-cdimage/ubuntu/releases/"
                      + ubuntu_ver + "/release/"
                      "ubuntu-" + ubuntu_ver + "-live-server-" + arch + ".iso";
        break;
    case 2:
        // ubuntu-gnome → 华为镜像
        if (arch == "i386")
            iso_url = "https://mirrors.huaweicloud.com/ubuntu-releases/16.04.6/"
                      "ubuntu-16.04.6-desktop-i386.iso";
        else
            iso_url = "https://mirrors.huaweicloud.com/ubuntu-releases/"
                      + ubuntu_ver + "/ubuntu-" + ubuntu_ver + "-desktop-amd64.iso";
        break;
    case 3: case 4: case 5: case 6: {
        // xubuntu/kubuntu/lubuntu/mate → TUNA 镜像
        static const char* distros[] = {"", "", "xubuntu", "kubuntu", "lubuntu", "ubuntu-mate"};
        std::string distro = distros[std::stoi(ed_choice)];
        if (arch == "i386")
            iso_url = "https://mirrors.bfsu.edu.cn/ubuntu-cdimage/"
                      + distro + "/releases/18.04.4/release/"
                      + distro + "-18.04.4-desktop-i386.iso";
        else
            iso_url = "https://mirrors.bfsu.edu.cn/ubuntu-cdimage/"
                      + distro + "/releases/" + ubuntu_ver + "/release/"
                      + distro + "-" + ubuntu_ver + "-desktop-amd64.iso";
        break;
    }
    default: return;
    }

    iso_filename = iso_url.substr(iso_url.rfind('/') + 1);
    tmoe_iso_download_model(iso_filename, iso_url);
}

// ═══════════════════════════════════════════════════════════════
//  Windows ISO (对应 Bash download_windows_10_iso, iso.sh:79-103)
//   子选项: win11_21996_x64 / win10_20h2_arm64 / other
// ═══════════════════════════════════════════════════════════════

void VirtualizationManager::download_windows_iso() {
    // 对应 Bash: download_windows_10_iso — 3个选项
    std::string win_menu = cfg_.tui_bin +
        " --title \"" + _("virt.windows_title") + "\""
        " --menu \"" + _("virt.windows_prompt") + "\" 12 55 4 "
        "\"1\" \"win11_21996_x64\" "
        "\"2\" \"win10_20h2_arm64 (uup)\" "
        "\"3\" \"other\" "
        "\"0\" \"" + _("menu.tui.back") + "\"";

    std::string choice = Executor::tui_select(win_menu);
    if (choice == "0" || choice.empty()) return;

    if (choice == "1") {
        // 对应 Bash: download_win11_x64_iso
        tmoe_iso_download_model(
            "win11_x64.iso",
            "https://packages.tmoe.me/iso/"
            "21996.1.210529-1541.co_release_CLIENT_CONSUMER_x64FRE_en-us.iso");
    } else if (choice == "2") {
        // 对应 Bash: download_win10_arm64_iso
        Logger::info(_("virt.windows_uup_note"));
        tmoe_iso_download_model(
            "win10-19042_arm64.iso",
            "https://m.tmoe.me/win10_arm64-latest-iso");
    } else {
        // other
        Logger::info("https://www.microsoft.com/zh-cn/software-download/windows10ISO");
        Logger::info("https://uupdump.ml");
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  LMDE ISO (对应 Bash download_linux_mint_debian_edition_iso, iso.sh:105-118)
//   子选项: 64bit / 32bit
// ═══════════════════════════════════════════════════════════════

void VirtualizationManager::download_lmde_iso() {
    // 对应 Bash: 架构选择 → yes=x86_64, no=x86_32
    bool is_64bit = whiptail_yesno(
        _("virt.lmde_arch_title"),
        "x86_64", "x86_32",
        _("virt.lmde_arch_prompt"));

    std::string grep_arch = is_64bit ? "64bit" : "32bit";
    std::string iso_url;

    // 对应 Bash: 从华为镜像抓取最新 LMDE ISO
    // ISO_REPO='https://mirrors.huaweicloud.com/linuxmint-cd/debian/'
    Logger::info(_("virt.lmde_fetching"));
    std::string iso_filename = "lmde-" + std::string(is_64bit ? "64bit" : "32bit") + ".iso";

    // 使用已知的稳定 URL
    iso_url = std::string("https://mirrors.huaweicloud.com/linuxmint-cd/debian/")
              + (is_64bit ? "lmde-6-cinnamon-64bit.iso" : "lmde-6-cinnamon-32bit.iso");

    tmoe_iso_download_model(iso_filename, iso_url);
}

// ═══════════════════════════════════════════════════════════════
//  Wine (保持不变 — 已有完整实现)
// ═══════════════════════════════════════════════════════════════

std::vector<std::pair<std::string, std::string>>
VirtualizationManager::wine_branches() {
    return {
        {"devel",   "Wine 开发版 — 最新特性"},
        {"staging", "Wine Staging — 实验补丁"},
        {"stable",  "Wine 稳定版"},
    };
}

bool VirtualizationManager::install_wine(std::string_view branch) {
    Logger::step(_f("virt.installing_wine", std::string(branch)));

    if (is_debian() || is_ubuntu()) {
        Executor::passthrough("dpkg --add-architecture i386 2>/dev/null");
        Executor::passthrough(cfg_.update_command);

        auto result = Executor::passthrough(cfg_.install_command +
                                            " wine wine32 wine64 2>/dev/null");

        if (!result.ok()) {
            Executor::passthrough("mkdir -p /etc/apt/keyrings");
            Executor::passthrough("wget -qO- https://dl.winehq.org/wine-builds/winehq.key | "
                "gpg --dearmor -o /etc/apt/keyrings/winehq-archive-keyring.gpg");
            Executor::passthrough(cfg_.update_command);

            result = Executor::passthrough(cfg_.install_command + " --install-recommends "
                                           "winehq-" + std::string(branch) + " wine-" +
                                           std::string(branch) + " wine-" +
                                           std::string(branch) + "-amd64 wine-" +
                                           std::string(branch) + "-i386");
        }

        if (result.ok()) {
            Logger::ok(_f("virt.wine_installed", std::string(branch)));
            Logger::info(_("virt.wine_winecfg_hint"));
            return true;
        }
    }

    if (is_arch()) {
        auto result = Executor::passthrough("pacman -S --noconfirm wine");
        if (result.ok()) {
            Logger::ok(_("virt.wine_installed_simple"));
            return true;
        }
    }

    auto result = Executor::passthrough(cfg_.install_command + " wine 2>/dev/null");
    if (result.ok()) {
        Logger::ok("Wine 已安装");
        return true;
    }

    Logger::error(_("virt.wine_install_failed"));
    return false;
}

bool VirtualizationManager::install_winetricks() {
    Logger::step(_("virt.installing_winetricks"));
    auto result = Executor::passthrough(cfg_.install_command + " winetricks");
    if (result.ok()) {
        Logger::ok(_("virt.winetricks_installed"));
    } else {
        Logger::warn(_("virt.winetricks_fallback"));
        Executor::passthrough("wget -O /usr/local/bin/winetricks "
            "https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks 2>/dev/null");
        Executor::shell("chmod +x /usr/local/bin/winetricks");
    }
    return true;
}

bool VirtualizationManager::install_dxvk() {
    Logger::step(_("virt.installing_dxvk"));
    Logger::info(_("virt.dxvk_hint"));
    return Executor::passthrough("winetricks dxvk 2>/dev/null").ok();
}

bool VirtualizationManager::install_playonlinux() {
    Logger::step(_("virt.installing_pol"));
    return Executor::passthrough(cfg_.install_command + " playonlinux").ok();
}

void VirtualizationManager::run_wine_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin +
            " --title \"" + _("virt.wine_title") + "\""
            " --menu \"" + _("virt.wine_prompt") + "\" 0 0 0 "
            "\"1\" \"" + _("virt.wine_devel") + "\" "
            "\"2\" \"" + _("virt.wine_staging") + "\" "
            "\"3\" \"" + _("virt.wine_stable") + "\" "
            "\"4\" \"" + _("virt.wine_winetricks") + "\" "
            "\"5\" \"" + _("virt.wine_dxvk") + "\" "
            "\"6\" \"" + _("virt.wine_playonlinux") + "\" "
            "\"0\" \"" + _("menu.tui.back") + "\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        if (choice == "1") install_wine("devel");
        else if (choice == "2") install_wine("staging");
        else if (choice == "3") install_wine("stable");
        else if (choice == "4") install_winetricks();
        else if (choice == "5") install_dxvk();
        else if (choice == "6") install_playonlinux();

        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  辅助
// ═══════════════════════════════════════════════════════════════

std::string VirtualizationManager::os_release() const {
    auto result = Executor::shell("cat /etc/os-release 2>/dev/null | grep '^ID=' | cut -d= -f2");
    std::string rel = result.ok() ? result.stdout_data : "";
    rel.erase(std::remove(rel.begin(), rel.end(), '\n'), rel.end());
    rel.erase(std::remove(rel.begin(), rel.end(), '\"'), rel.end());
    return rel;
}

bool VirtualizationManager::is_debian() const { return os_release() == "debian"; }
bool VirtualizationManager::is_ubuntu() const { return os_release() == "ubuntu"; }
bool VirtualizationManager::is_arch() const { return os_release() == "arch"; }

} // namespace tmoe::domain
