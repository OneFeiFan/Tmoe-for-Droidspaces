#include "domain/docker.h"
#include "core/i18n.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace tmoe::domain {

// ── 发行版注册表（17 种，含 tag1/tag2/容器名） ──
struct DistroSpec {
    const char* image;
    const char* label_key;   // i18n key
    const char* tag1;
    const char* tag2;
    const char* container;
};

static const DistroSpec DISTRO_SPECS[] = {
    {"alpine",        "docker.distros_alpine",     "latest", "edge",    ""},
    {"debian",        "docker.distros_debian",     "unstable","stable", ""},
    {"ubuntu",        "docker.distros_ubuntu",     "latest", "devel",   ""},
    {"kalilinux/kali-rolling","docker.distros_kali", "latest","latest", "kali"},
    {"archlinux",     "docker.distros_arch",       "latest", "",        "arch"},
    {"fedora",        "docker.distros_fedora",     "latest", "rawhide", ""},
    {"centos",        "docker.distros_centos",     "latest", "7",       "cent"},
    {"opensuse/tumbleweed","docker.distros_opensuse","latest","latest","suse"},
    {"gentoo/stage3-amd64","docker.distros_gentoo", "latest", "",       "gentoo"},
    {"clearlinux",    "docker.distros_clearlinux", "latest", "base",    "clear"},
    {"voidlinux/voidlinux","docker.distros_void",  "latest", "latest",  "void"},
    {"oraclelinux",   "docker.distros_oracle",     "latest", "7",       "oracle"},
    {"amazonlinux",   "docker.distros_amazon",     "latest", "with-sources","amazon"},
    {"crux",          "docker.distros_crux",       "latest", "3.4",     ""},
    {"openwrtorg/rootfs","docker.distros_openwrt", "latest", "",        "openwrt"},
    {"alt",           "docker.distros_alt",        "latest", "sisyphus",""},
    {"photon",        "docker.distros_photon",     "latest", "2.0",     ""},
};

// 旧版简易发行版列表（tui_pull_distro_image 使用）
static const std::vector<std::pair<std::string, std::string>> DISTRO_IMAGES = {
    {"alpine",        "Alpine Linux"},
    {"debian",        "Debian Sid"},
    {"ubuntu",        "Ubuntu"},
    {"kali-rolling",  "Kali Linux"},
    {"archlinux",     "Arch Linux"},
    {"fedora",        "Fedora"},
    {"centos",        "CentOS"},
    {"opensuse/tumbleweed", "openSUSE Tumbleweed"},
    {"gentoo/stage3", "Gentoo"},
    {"clearlinux",    "Clear Linux"},
    {"voidlinux/void-linux-x86_64-musl", "Void Linux"},
    {"oraclelinux",   "Oracle Linux"},
    {"amazonlinux",   "Amazon Linux"},
    {"crux",          "CRUX"},
    {"openwrt/rootfs","OpenWrt"},
    {"alt",           "ALT Linux"},
    {"photon",        "VMware Photon OS"},
};

DockerManager::DockerManager(const TmoeConfig& cfg) : cfg_(cfg),
    current_systemd_(false) {}

// ═══════════════════════════════════════════════════════════════
//  TUI 主菜单
// ═══════════════════════════════════════════════════════════════

void DockerManager::run_docker_menu() {
    while (true) {
        std::string available = is_docker_available() ?
            _("docker.status_installed") : _("docker.status_not_installed");

        std::string menu = cfg_.tui_bin +
            " --title \"" + _("docker.title") + " — " + available + "\""
            " --menu \"" + _("docker.menu_prompt") + "\" 0 0 0 "
            "\"1\" \"" + _("docker.cross_arch") + "\" "
            "\"2\" \"" + _("docker.systemd_docker") + "\" "
            "\"3\" \"" + _("docker.pull_distro_title") + "\" "
            "\"4\" \"" + _("docker.install_portainer") + "\" "
            "\"5\" \"" + _("docker.export") + "\" "
            "\"6\" \"" + _("docker.import") + "\" "
            "\"7\" \"" + _("docker.mirror_source") + "\" "
            "\"8\" \"" + _("docker.install") + "\" "
            "\"9\" \"" + _("docker.add_user_group") + "\" "
            "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        if (choice == "1") {
            run_docker_across_architectures(false);
        } else if (choice == "2") {
            run_docker_across_architectures(true);
        } else if (choice == "3") {
            proot_warning_check();
            if (!check_docker_installation()) break;
            choose_gnu_linux_docker_images(false);
        } else if (choice == "4") {
            if (!check_docker_installation()) break;
            std::string port_cmd = cfg_.tui_bin +
                " --title \"" + _("docker.portainer_port_title") + "\""
                " --inputbox \"" + _("docker.portainer_port_input") + "\" 0 0 \"39080\"";
            auto result = Executor::passthrough("echo \"$(" + port_cmd + " 2>&1 1>/dev/tty)\"");
            std::string port_str = result.stdout_data;
            port_str.erase(std::remove(port_str.begin(), port_str.end(), '\n'), port_str.end());
            int port = port_str.empty() ? 39080 : std::stoi(port_str);
            install_portainer(port);
        } else if (choice == "5") {
            if (!check_docker_installation()) break;
            export_docker_image_menu();
        } else if (choice == "6") {
            if (!check_docker_installation()) break;
            import_docker_image_menu();
        } else if (choice == "7") {
            docker_mirror_source();
        } else if (choice == "8") {
            proot_warning_check();
            install_docker_ce_or_io();
        } else if (choice == "9") {
            add_user_to_docker_group();
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  Docker 安装
// ═══════════════════════════════════════════════════════════════

bool DockerManager::install_docker_ce() {
    Logger::step(_("docker.installing_ce"));

    if (is_debian_family()) {
        if (!debian_add_docker_gpg()) {
            Logger::error(_("docker.repo_add_failed"));
            return false;
        }
        return Executor::passthrough("apt install -y docker-ce docker-ce-cli containerd.io").ok();
    }

    if (is_redhat_family()) {
        Executor::passthrough("dnf config-manager --add-repo https://mirrors.bfsu.edu.cn/docker-ce/linux/fedora/docker-ce.repo 2>/dev/null");
        return Executor::passthrough("dnf install -y docker-ce docker-ce-cli containerd.io").ok();
    }

    if (is_arch_family()) {
        return Executor::passthrough("pacman -S --noconfirm docker").ok();
    }

    if (is_alpine()) {
        Executor::passthrough("apk add docker docker-cli-compose");
        Executor::passthrough("rc-update add docker boot");
        return Executor::passthrough("service docker start").ok();
    }

    Logger::warn(_("docker.unsupported_distro"));
    return false;
}

bool DockerManager::install_docker_io() {
    Logger::step(_("docker.installing_io"));
    if (is_debian_family()) {
        Executor::passthrough("apt install -y docker.io docker");
        return true;
    }
    Logger::warn(_("docker.io_debian_only"));
    return install_docker_ce();
}

bool DockerManager::install_docker_ce_or_io() {
    std::string menu = cfg_.tui_bin +
        " --title \"DOCKER\" --yes-button 'docker-ce' --no-button 'docker.io'"
        " --yesno \"" + _("docker.proot_warning") + "\\n\\n" +
        _("docker.version_prompt") + "\" 0 50";
    auto result = Executor::passthrough(menu);
    bool install_ce = result.exit_code == 0;

    bool ok;
    if (install_ce) ok = install_docker_ce();
    else ok = install_docker_io();

    if (ok) Executor::passthrough("docker version 2>/dev/null");
    return ok;
}

bool DockerManager::install_portainer(int port) {
    Logger::step(_("docker.installing_portainer"));

    if (!is_docker_available()) {
        Logger::error(_("docker.install_docker_first"));
        return false;
    }

    ensure_docker_running();
    Executor::passthrough("docker stop portainer 2>/dev/null");
    Executor::passthrough("docker pull portainer/portainer-ce:latest");
    Executor::passthrough("docker rm portainer 2>/dev/null");

    std::ostringstream cmd;
    cmd << "docker run -d --name portainer --restart=always "
        << "-p " << port << ":9000 "
        << "-v /var/run/docker.sock:/var/run/docker.sock "
        << "-v portainer_data:/data portainer/portainer-ce:latest";
    auto res = Executor::passthrough(cmd.str());
    if (res.ok()) {
        Logger::ok(_("docker.portainer_started"));
        Logger::info(_f("docker.portainer_url", std::to_string(port)));
    }
    return res.ok();
}

// ═══════════════════════════════════════════════════════════════
//  镜像管理
// ═══════════════════════════════════════════════════════════════

bool DockerManager::pull_image(std::string_view image, std::string_view tag) {
    Logger::step(_f("docker.pulling_image", std::string(image), std::string(tag)));
    std::string full = std::string(image) + ":" + std::string(tag);
    return Executor::passthrough("docker pull " + full).ok();
}

std::vector<DockerImageInfo> DockerManager::list_images() const {
    std::vector<DockerImageInfo> result;
    auto exec = Executor::shell("docker images --format \"{{.Repository}} {{.Tag}}\" 2>/dev/null");
    if (!exec.ok()) return result;

    std::istringstream iss(exec.stdout_data);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        std::istringstream lss(line);
        std::string name, tag;
        lss >> name >> tag;
        if (!name.empty()) {
            if (tag.empty()) tag = "latest";
            result.push_back({name, tag});
        }
    }
    return result;
}

bool DockerManager::tui_pull_distro_image() {
    std::string menu = cfg_.tui_bin +
        " --title \"" + _("docker.pull_distro_title") + "\""
        " --menu \"" + _("docker.pull_distro_prompt") + "\" 0 0 0 ";

    for (size_t i = 0; i < DISTRO_IMAGES.size(); ++i) {
        menu += "\"" + std::to_string(i + 1) + "\" \""
             + DISTRO_IMAGES[i].second + "\" ";
    }
    menu += "\"0\" \"" + _("menu.tui.back") + "\"";

    std::string choice = Executor::tui_select(menu);
    if (choice == "0" || choice.empty()) return false;

    int idx = std::stoi(choice) - 1;
    if (idx < 0 || idx >= static_cast<int>(DISTRO_IMAGES.size())) return false;

    std::string image_name = DISTRO_IMAGES[idx].first;
    std::string tag = tui_input_tag();
    return pull_image(image_name, tag);
}

// ═══════════════════════════════════════════════════════════════
//  发行版选择 + 容器管理子菜单（完整 Bash 复刻）
// ═══════════════════════════════════════════════════════════════

void DockerManager::choose_gnu_linux_docker_images(bool systemd_mode) {
    if (!check_docker_installation()) return;

    current_systemd_ = systemd_mode;

    // 构建 17 种发行版菜单（+ systemd 子菜单中的 centos/fedora）
    std::string menu = cfg_.tui_bin +
        " --title \"" + _("docker.distros_title") + "\""
        " --menu \"" + _("docker.distros_prompt") + "\" 0 50 0 ";
    menu += "\"0\" \"" + _("menu.tui.back_upper") + "\" ";

    for (int i = 0; i < 17; ++i) {
        menu += "\"" + std::to_string(i + 1) + "\" \""
             + _(DISTRO_SPECS[i].label_key) + "\" ";
    }

    std::string choice = Executor::tui_select(menu);
    if (choice == "0" || choice.empty()) return;

    int idx = std::stoi(choice) - 1;
    if (idx < 0 || idx >= 17) return;

    const auto& spec = DISTRO_SPECS[idx];
    current_docker_name_ = spec.image;
    current_docker_tag1_ = spec.tag1;
    current_docker_tag2_ = std::string(spec.tag2);
    current_container_name_ = std::string(spec.container);
    current_docker_name2_ = "";
    current_mgt_menu_type_ = 1;

    // 特殊路由：Kali/Arch/Gentoo/OpenWrt 根据架构选镜像
    std::string true_arch = detect_true_arch();

    if (idx == 3) { // Kali
        if (qemu_arch_ == "x86_64" || (qemu_arch_.empty() && true_arch == "amd64")) {
            current_docker_name_ = "kalilinux/kali-rolling";
            current_docker_name2_ = "kalilinux/kali";
        } else if (qemu_arch_ == "arm" || true_arch == "armhf") {
            current_docker_name_ = "rbartoli/kali-linux-arm";
            current_docker_name2_ = "williamlegourd/kali-gui";
            current_mgt_menu_type_ = 2;
        } else {
            current_docker_name_ = "donaldrich/kali-linux";
            current_docker_name2_ = "heywoodlh/kali-linux";
            current_mgt_menu_type_ = 2;
        }
        current_container_name_ = "kali";
    } else if (idx == 4) { // Arch
        if (qemu_arch_ == "x86_64" || (qemu_arch_.empty() && true_arch == "amd64")) {
            current_docker_name_ = "archlinux";
            current_mgt_menu_type_ = 3;
        } else {
            current_docker_name_ = "lopsided/archlinux";
            current_docker_name2_ = "agners/archlinuxarm";
            current_mgt_menu_type_ = 2;
        }
        current_container_name_ = "arch";
    } else if (idx == 8) { // Gentoo
        current_container_name_ = "gentoo";
        current_mgt_menu_type_ = 2;
        if (qemu_arch_ == "x86_64" || (qemu_arch_.empty() && true_arch == "amd64")) {
            current_docker_name_ = "gentoo/stage3-amd64";
            current_docker_name2_ = "gentoo/stage3-amd64-hardened-nomultilib";
        } else if (qemu_arch_ == "i386" || true_arch == "i386") {
            current_docker_name_ = "gentoo/stage3-x86";
            current_docker_name2_ = "gentoo/stage3-x86-hardened";
        } else {
            current_docker_name_ = "paralin/gentoo-stage3-armv7a";
            current_docker_name2_ = "applehq/gentoo-stage4";
        }
    } else if (idx == 14) { // OpenWrt
        current_container_name_ = "openwrt";
        current_mgt_menu_type_ = 2;
        if (qemu_arch_ == "x86_64" || (qemu_arch_.empty() && true_arch == "amd64")) {
            current_docker_name_ = "openwrtorg/rootfs";
            current_docker_name2_ = "katta/openwrt-rootfs";
        } else {
            current_docker_name_ = "buddyfly/openwrt-aarch64";
            current_docker_name2_ = "unifreq/openwrt-aarch64";
        }
    } else if (idx == 7) { // openSUSE
        current_docker_name2_ = "opensuse/leap";
        current_mgt_menu_type_ = 2;
        current_container_name_ = "suse";
    } else if (idx == 10) { // Void
        current_docker_name2_ = "voidlinux/voidlinux-musl";
        current_mgt_menu_type_ = 2;
        current_container_name_ = "void";
    }

    // 设默认容器名
    if (current_container_name_.empty()) {
        current_container_name_ = current_docker_name_;
        auto slash = current_container_name_.find('/');
        if (slash != std::string::npos) {
            current_container_name_ = current_container_name_.substr(slash + 1);
        }
    }

    // systemd 模式下容器名加后缀
    if (systemd_mode) {
        current_container_name_ += "-systemd";
    }

    // 跨架构时前缀处理
    if (!qemu_arch_.empty() && !new_arch_prefix_.empty()) {
        if (current_mgt_menu_type_ == 1 || current_mgt_menu_type_ == 3) {
            current_docker_name_ = new_arch_prefix_ + "/" + current_docker_name_;
            current_container_name_ += "_" + container_ext_name_;
        } else {
            current_container_name_ += "_" + container_ext_name_;
        }
    }

    // 进入子菜单
    switch (current_mgt_menu_type_) {
        case 1:
            // 如果 tag2 为空，降级到 menu_03
            if (current_docker_tag2_.empty()) {
                docker_management_menu_03(current_docker_name_, current_container_name_,
                                           current_docker_tag1_, systemd_mode, qemu_arch_);
            } else {
                docker_management_menu_01(current_docker_name_, current_container_name_,
                                           current_docker_tag1_, current_docker_tag2_,
                                           systemd_mode, qemu_arch_);
            }
            break;
        case 2:
            docker_management_menu_02(current_docker_name_, current_docker_name2_,
                                       current_container_name_, current_docker_tag1_,
                                       systemd_mode, qemu_arch_);
            break;
        case 3:
            docker_management_menu_03(current_docker_name_, current_container_name_,
                                       current_docker_tag1_, systemd_mode, qemu_arch_);
            break;
    }

    Logger::press_enter();
    choose_gnu_linux_docker_images(systemd_mode);
}

// ── 容器管理子菜单 ──

void DockerManager::docker_management_menu_01(const std::string& docker_name,
                                                const std::string& container_name,
                                                const std::string& tag1,
                                                const std::string& tag2,
                                                bool systemd,
                                                const std::string& qemu_arch) {
    while (true) {
        std::string menu = cfg_.tui_bin +
            " --title \"" + docker_name + " CONTAINER\""
            " --menu \"" + _("docker.mgt_menu_prompt") + "\" 0 0 0 "
            "\"1\" \"" + _f("docker.mgt_tag1", tag1) + "\" "
            "\"2\" \"" + _f("docker.mgt_tag2", tag2) + "\" "
            "\"3\" \"" + _("docker.mgt_custom_tag") + "\" "
            "\"4\" \"" + _f("docker.mgt_attach", container_name) + "\" "
            "\"5\" \"" + _("docker.mgt_readme") + "\" "
            "\"6\" \"" + _("docker.mgt_reset") + "\" "
            "\"7\" \"" + _("docker.mgt_delete") + "\" "
            "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        if (choice == "1") {
            run_special_tag_docker_container(docker_name, tag1, container_name, qemu_arch, systemd);
        } else if (choice == "2") {
            run_special_tag_docker_container(docker_name, tag2, container_name, qemu_arch, systemd);
        } else if (choice == "3") {
            std::string custom_tag = custom_docker_container_tag(docker_name, container_name);
            if (!custom_tag.empty()) {
                run_special_tag_docker_container(docker_name, custom_tag, container_name, qemu_arch, systemd);
            }
        } else if (choice == "4") {
            docker_attach_container(container_name, docker_name, tag1, systemd);
        } else if (choice == "5") {
            tmoe_docker_readme(container_name);
        } else if (choice == "6") {
            // 询问确认
            Logger::info(_f("docker.reset_prompt", container_name, docker_name, tag1));
            std::string confirm = cfg_.tui_bin +
                " --title \"" + _("docker.mgt_reset") + "\""
                " --yesno \"" + _f("docker.reset_prompt", container_name, docker_name, tag1) + "\" 0 50";
            if (Executor::passthrough(confirm).exit_code == 0) {
                reset_docker_container(docker_name, tag1, container_name, systemd);
            }
        } else if (choice == "7") {
            // 删除确认
            std::string del_menu = cfg_.tui_bin +
                " --title \"" + _("docker.delete_title") + "\""
                " --yes-button '" + _("docker.delete_container_only") + "'"
                " --no-button '" + _("docker.delete_with_image") + "'"
                " --yesno \"" + _("docker.delete_prompt") + "\" 0 50";
            auto del_choice = Executor::passthrough(del_menu);
            if (del_choice.exit_code == 0) {
                remove_container(container_name);
            } else {
                delete_container_and_image(docker_name, tag1, tag2, container_name);
            }
        }

        if (choice != "5") Logger::press_enter();
    }
}

void DockerManager::docker_management_menu_02(const std::string& docker_name,
                                                const std::string& docker_name2,
                                                const std::string& container_name,
                                                const std::string& tag1,
                                                bool systemd,
                                                const std::string& qemu_arch) {
    while (true) {
        std::string menu = cfg_.tui_bin +
            " --title \"" + docker_name + " CONTAINER\""
            " --menu \"" + _("docker.mgt_menu_prompt") + "\" 0 0 0 "
            "\"1\" \"" + docker_name + "\" "
            "\"2\" \"" + docker_name2 + "\" "
            "\"3\" \"" + _("docker.mgt_custom_tag") + "\" "
            "\"4\" \"" + _f("docker.mgt_attach", container_name) + "\" "
            "\"5\" \"" + _("docker.mgt_readme") + "\" "
            "\"6\" \"" + _("docker.mgt_reset") + "\" "
            "\"7\" \"" + _("docker.mgt_delete") + "\" "
            "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        if (choice == "1") {
            run_special_tag_docker_container(docker_name, tag1, container_name, qemu_arch, systemd);
        } else if (choice == "2") {
            run_special_tag_docker_container(docker_name2, tag1, container_name, qemu_arch, systemd);
        } else if (choice == "3") {
            std::string ct = custom_docker_container_tag(docker_name, container_name);
            if (!ct.empty()) {
                run_special_tag_docker_container(docker_name, ct, container_name, qemu_arch, systemd);
            }
        } else if (choice == "4") {
            docker_attach_container(container_name, docker_name, tag1, systemd);
        } else if (choice == "5") {
            tmoe_docker_readme(container_name);
        } else if (choice == "6") {
            std::string confirm = cfg_.tui_bin +
                " --title \"" + _("docker.mgt_reset") + "\""
                " --yesno \"" + _f("docker.reset_prompt", container_name, docker_name, tag1) + "\" 0 50";
            if (Executor::passthrough(confirm).exit_code == 0) {
                reset_docker_container(docker_name, tag1, container_name, systemd);
            }
        } else if (choice == "7") {
            std::string del_menu = cfg_.tui_bin +
                " --title \"" + _("docker.delete_title") + "\""
                " --yes-button '" + _("docker.delete_container_only") + "'"
                " --no-button '" + _("docker.delete_with_image") + "'"
                " --yesno \"" + _("docker.delete_prompt") + "\" 0 50";
            auto dc = Executor::passthrough(del_menu);
            if (dc.exit_code == 0) {
                remove_container(container_name);
            } else {
                delete_container_and_image(docker_name, tag1, "", container_name);
                // 也尝试删 docker_name2 的镜像
                Executor::passthrough("docker rmi " + docker_name2 + ":" + tag1 + " 2>/dev/null");
                Executor::passthrough("docker rmi " + docker_name2 + " 2>/dev/null");
            }
        }

        if (choice != "5") Logger::press_enter();
    }
}

void DockerManager::docker_management_menu_03(const std::string& docker_name,
                                                const std::string& container_name,
                                                const std::string& tag1,
                                                bool systemd,
                                                const std::string& qemu_arch) {
    while (true) {
        std::string menu = cfg_.tui_bin +
            " --title \"" + docker_name + " CONTAINER\""
            " --menu \"" + _("docker.mgt_menu_prompt") + "\" 0 0 0 "
            "\"1\" \"" + _f("docker.mgt_tag1", tag1) + "\" "
            "\"2\" \"" + _("docker.mgt_custom_tag") + "\" "
            "\"3\" \"" + _f("docker.mgt_attach", container_name) + "\" "
            "\"4\" \"" + _("docker.mgt_readme") + "\" "
            "\"5\" \"" + _("docker.mgt_reset") + "\" "
            "\"6\" \"" + _("docker.mgt_delete") + "\" "
            "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        if (choice == "1") {
            run_special_tag_docker_container(docker_name, tag1, container_name, qemu_arch, systemd);
        } else if (choice == "2") {
            std::string ct = custom_docker_container_tag(docker_name, container_name);
            if (!ct.empty()) {
                run_special_tag_docker_container(docker_name, ct, container_name, qemu_arch, systemd);
            }
        } else if (choice == "3") {
            docker_attach_container(container_name, docker_name, tag1, systemd);
        } else if (choice == "4") {
            tmoe_docker_readme(container_name);
        } else if (choice == "5") {
            std::string confirm = cfg_.tui_bin +
                " --yesno \"" + _f("docker.reset_prompt", container_name, docker_name, tag1) + "\" 0 50";
            if (Executor::passthrough(confirm).exit_code == 0) {
                reset_docker_container(docker_name, tag1, container_name, systemd);
            }
        } else if (choice == "6") {
            std::string del_menu = cfg_.tui_bin +
                " --title \"" + _("docker.delete_title") + "\""
                " --yes-button '" + _("docker.delete_container_only") + "'"
                " --no-button '" + _("docker.delete_with_image") + "'"
                " --yesno \"" + _("docker.delete_prompt") + "\" 0 50";
            auto dc = Executor::passthrough(del_menu);
            if (dc.exit_code == 0) {
                remove_container(container_name);
            } else {
                delete_container_and_image(docker_name, tag1, "", container_name);
            }
        }

        if (choice != "4") Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  容器运行（含跨架构 + systemd）
// ═══════════════════════════════════════════════════════════════

bool DockerManager::run_special_tag_docker_container(const std::string& docker_name,
                                                       const std::string& docker_tag,
                                                       const std::string& container_name,
                                                       const std::string& qemu_arch,
                                                       bool systemd) {
    docker_init();
    ensure_docker_running();

    std::ostringstream cmd;
    cmd << "docker run -itd --name \"" << container_name << "\"";

    if (systemd) {
        cmd << " --privileged=true";
        cmd << " --env CONTAINER_SYSTEMD=true";
    }

    cmd << " --env LANG=" << cfg_.locale;
    cmd << " --env TMOE_CHROOT=true";
    cmd << " --env TMOE_DOCKER=true";
    cmd << " --env TMOE_PROOT=false";
    cmd << " --restart on-failure";

    // 跨架构 qemu 挂载
    std::string qemu_path = "/usr/bin";
    if (!qemu_arch.empty()) {
        std::string qemu_static = qemu_path + "/qemu-" + qemu_arch + "-static";
        if (fs::exists(qemu_static)) {
            cmd << " -v " << qemu_static << ":" << qemu_path << "/qemu-" << qemu_arch << "-static";
        }
    }

    // 挂载目录
    fs::path mount_dir = "/media/docker";
    if (fs::exists(mount_dir)) {
        cmd << " -v " << mount_dir.string() << ":" << mount_dir.string();
    }

    cmd << " \"" << docker_name << "\"";
    if (!docker_tag.empty() && docker_tag != "latest") {
        cmd << ":" << docker_tag;
    }

    if (systemd) {
        cmd << " /sbin/init";
    }

    Logger::info("docker run -itd --name " + container_name + " ...");
    auto result = Executor::passthrough(cmd.str());

    if (result.ok()) {
        Logger::ok(_f("docker.container_started", container_name));
        if (fs::exists(mount_dir)) {
            Logger::info(_f("docker.mount_volume_hint", mount_dir.string()));
        }
        Logger::info(_f("docker.exec_hint", container_name));

        // 询问是否启动并配置
        Logger::info(_("docker.start_configure_prompt"));
        std::string confirm = cfg_.tui_bin +
            " --title \"" + container_name + "\""
            " --yesno \"" + _("docker.start_configure_prompt") + "\" 0 50";
        if (Executor::passthrough(confirm).exit_code == 0) {
            ensure_docker_running();
            Executor::shell("docker start " + container_name);

            // 执行配置脚本
            std::string config_file = mount_dir.string() + "/.tmoe-linux-docker.sh";
            if (fs::exists(config_file)) {
                Executor::passthrough("docker exec -it " + container_name + " /bin/sh " + config_file);
            } else {
                Executor::passthrough("docker exec -it " + container_name + " /bin/bash 2>/dev/null || docker attach " + container_name);
            }
        }
    }
    return result.ok();
}

bool DockerManager::run_container(std::string_view image,
                                   std::string_view name,
                                   std::string_view mount_path,
                                   int host_port, int container_port) {
    Logger::step(_f("docker.running_container", std::string(name)));

    std::ostringstream cmd;
    cmd << "docker run -itd --name \"" << name << "\""
        << " --privileged=true"
        << " --env LANG=" << cfg_.locale
        << " --restart on-failure";

    std::string mount = mount_path.empty() ?
        cfg_.container_root.string() : std::string(mount_path);
    if (!mount.empty() && fs::exists(mount)) {
        cmd << " -v \"" << mount << "\":\"" << mount << "\"";
    }

    if (host_port > 0 && container_port > 0) {
        cmd << " -p " << host_port << ":" << container_port;
    }

    cmd << " \"" << image << "\"";

    auto result = Executor::passthrough(cmd.str());
    if (result.ok()) {
        Logger::ok(_f("docker.container_started", std::string(name)));
    }
    return result.ok();
}

bool DockerManager::run_cross_arch_container(std::string_view image,
                                               std::string_view name,
                                               std::string_view arch) {
    Logger::step(_f("docker.cross_arch_run", std::string(arch)));
    install_qemu_user_static();

    // 使用 run_special_tag_docker_container 处理 qemu 挂载
    auto colon = std::string(image).find(':');
    std::string img_name = std::string(image);
    std::string img_tag = "latest";
    if (colon != std::string::npos) {
        img_tag = img_name.substr(colon + 1);
        img_name = img_name.substr(0, colon);
    }

    return run_special_tag_docker_container(img_name, img_tag, std::string(name),
                                             std::string(arch), false);
}

// ═══════════════════════════════════════════════════════════════
//  容器管理
// ═══════════════════════════════════════════════════════════════

std::vector<std::string> DockerManager::list_containers(bool all) const {
    std::vector<std::string> result;
    std::string flag = all ? "-a" : "";
    auto exec = Executor::shell("docker ps " + flag + " --format \"{{.Names}} [{{.Image}}] {{.Status}}\" 2>/dev/null");
    if (!exec.ok()) return result;

    std::istringstream iss(exec.stdout_data);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty()) result.push_back(line);
    }
    return result;
}

bool DockerManager::remove_container(std::string_view name) {
    Logger::step(_f("docker.removing_container", std::string(name)));
    ensure_docker_running();
    Executor::passthrough("docker stop \"" + std::string(name) + "\" 2>/dev/null");
    return Executor::passthrough("docker rm \"" + std::string(name) + "\"").ok();
}

bool DockerManager::delete_container_and_image(const std::string& docker_name,
                                                 const std::string& docker_tag,
                                                 const std::string& docker_tag2,
                                                 const std::string& container_name) {
    Logger::step(_f("docker.deleting_container", container_name.empty() ? docker_name : container_name));
    ensure_docker_running();

    if (!container_name.empty()) {
        Executor::passthrough("docker stop " + container_name + " 2>/dev/null");
        Executor::passthrough("docker rm " + container_name + " 2>/dev/null");
    }

    // 删除镜像
    Logger::info(_f("docker.delete_image_rmi", docker_name, docker_tag));
    Executor::passthrough("docker rmi " + docker_name + ":" + docker_tag + " 2>/dev/null");
    if (!docker_tag2.empty() && docker_tag2 != docker_tag) {
        Executor::passthrough("docker rmi " + docker_name + ":" + docker_tag2 + " 2>/dev/null");
    }
    Executor::passthrough("docker rmi " + docker_name + " 2>/dev/null");

    return true;
}

bool DockerManager::docker_attach_container(const std::string& container_name,
                                              const std::string& docker_name,
                                              const std::string& docker_tag,
                                              bool systemd) {
    Logger::step(_("docker.attach_hint"));
    ensure_docker_running();

    // 检查容器是否存在
    auto check = Executor::shell("docker ps -a --format \"{{.Names}}\" 2>/dev/null | grep -q \"^" + container_name + "$\"");
    if (!check.ok()) {
        Logger::warn(_f("docker.attach_not_found", container_name));
        Logger::info(_f("docker.attach_pull_prompt", docker_name));
        std::string confirm = cfg_.tui_bin +
            " --yesno \"" + _f("docker.attach_pull_prompt", docker_name) + "\" 0 50";
        if (Executor::passthrough(confirm).exit_code == 0) {
            run_special_tag_docker_container(docker_name, docker_tag, container_name,
                                              qemu_arch_, systemd);
            return true;
        }
        return false;
    }

    Executor::passthrough("docker start " + container_name);
    // 先尝试 bash，失败则 attach
    auto r = Executor::passthrough("docker exec -it " + container_name + " /bin/bash 2>/dev/null");
    if (!r.ok()) {
        Executor::passthrough("docker attach " + container_name);
    }
    return true;
}

bool DockerManager::reset_docker_container(const std::string& docker_name,
                                             const std::string& docker_tag,
                                             const std::string& container_name,
                                             bool systemd) {
    Logger::step(_f("docker.resetting_container", container_name));
    ensure_docker_running();

    // 删除容器和镜像
    delete_container_and_image(docker_name, docker_tag, "", container_name);

    // 重新拉取
    Logger::info(_f("docker.pulling_image", docker_name, docker_tag));
    Executor::passthrough("docker pull " + docker_name + ":" + docker_tag);

    // 重新运行
    return run_special_tag_docker_container(docker_name, docker_tag, container_name,
                                             qemu_arch_, systemd);
}

// ═══════════════════════════════════════════════════════════════
//  导入/导出
// ═══════════════════════════════════════════════════════════════

bool DockerManager::export_container(std::string_view name,
                                       std::string_view output_path) {
    Logger::step(_f("docker.exporting_container", std::string(name)));

    fs::path out(output_path);
    if (!fs::exists(out.parent_path())) {
        fs::create_directories(out.parent_path());
    }

    std::string cmd = "docker export \"" + std::string(name) +
                      "\" > \"" + std::string(output_path) + "\"";
    auto result = Executor::passthrough(cmd);
    if (result.ok()) {
        auto size_mb = fs::file_size(output_path) / (1024 * 1024);
        Logger::ok(_f("docker.export_ok", std::string(output_path), std::to_string(size_mb)));
    }
    return result.ok();
}

void DockerManager::export_docker_image_menu() {
    ensure_docker_running();

    // 获取容器列表 (名称 镜像)
    std::vector<std::string> names, images;
    auto exec = Executor::shell("docker ps -a --format \"{{.Names}}\" 2>/dev/null");
    if (exec.ok()) {
        std::istringstream iss(exec.stdout_data);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) names.push_back(line);
        }
    }
    auto exec2 = Executor::shell("docker ps -a --format \"{{.Image}}\" 2>/dev/null");
    if (exec2.ok()) {
        std::istringstream iss(exec2.stdout_data);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) images.push_back(line);
        }
    }

    if (names.empty()) {
        Logger::info(_("docker.containers_list_title") + ": (empty)");
        return;
    }

    // TUI 选择容器
    std::string menu = cfg_.tui_bin +
        " --title \"" + _("docker.export_container_title") + "\""
        " --menu \"" + _("docker.export_container_prompt") + "\" 0 0 0 ";
    menu += "\"0\" \"" + _("menu.tui.back_upper") + "\" ";

    for (size_t i = 0; i < names.size() && i < images.size(); ++i) {
        menu += "\"" + names[i] + "\" \""
             + names[i] + " [" + images[i] + "]\" ";
    }

    auto result = Executor::passthrough("echo \"$(" + menu + " 2>&1 1>/dev/tty)\"");
    std::string choice = result.stdout_data;
    choice.erase(std::remove(choice.begin(), choice.end(), '\n'), choice.end());
    if (choice == "0" || choice.empty()) return;

    // 选择保存路径
    std::string start_dir;
    std::string home_dir = std::getenv("HOME") ? std::getenv("HOME") : "";
    for (const auto& d : {home_dir + "/sd/Download/backup/rootfs",
                           std::string{"/media/sd/Download/backup/rootfs"},
                           home_dir + "/sd",
                           home_dir}) {
        if (fs::is_directory(d)) { start_dir = d; break; }
    }
    if (start_dir.empty()) start_dir = "/tmp";

    std::string path_cmd = cfg_.tui_bin +
        " --title \"FILE PATH\""
        " --inputbox \"" + start_dir + "\" 0 50 \"" + start_dir + "\"";
    result = Executor::passthrough("echo \"$(" + path_cmd + " 2>&1 1>/dev/tty)\"");
    std::string target = result.stdout_data;
    target.erase(std::remove(target.begin(), target.end(), '\n'), target.end());
    if (target.empty()) target = start_dir;

    std::string bak_file = target + "/" + choice + "_bak-" +
        Executor::shell("date +%Y-%m-%d_%H-%M").stdout_data;
    bak_file.erase(std::remove(bak_file.begin(), bak_file.end(), '\n'), bak_file.end());
    bak_file += ".tar";

    export_container(choice, bak_file);

    // 修复权限
    if (std::getenv("HOME") && std::string(std::getenv("HOME")) != "/root") {
        std::string user = Executor::shell("whoami").stdout_data;
        user.erase(std::remove(user.begin(), user.end(), '\n'), user.end());
        user.erase(std::remove(user.begin(), user.end(), '\r'), user.end());
        Executor::shell("chown " + user + ":" + user + " " + bak_file + " 2>/dev/null");
        Executor::shell("chmod a+rw " + bak_file + " 2>/dev/null");
    }
}

bool DockerManager::import_image(std::string_view tar_path,
                                   std::string_view image_name,
                                   std::string_view tag) {
    Logger::step(_f("docker.importing_image", std::string(image_name), std::string(tag)));

    // 检测压缩格式
    std::string tar_path_str(tar_path);
    std::string extract_cmd;
    if (tar_path_str.size() > 7 && tar_path_str.substr(tar_path_str.size() - 7) == ".tar.xz") {
        Logger::info(_("docker.import_tar_detected") + " (xz)");
        extract_cmd = "xz -d -k " + tar_path_str;
        tar_path_str = tar_path_str.substr(0, tar_path_str.size() - 3);
    } else if (tar_path_str.size() > 7 && tar_path_str.substr(tar_path_str.size() - 7) == ".tar.gz") {
        Logger::info(_("docker.import_tar_detected") + " (gz)");
        extract_cmd = "gzip -d -k " + tar_path_str;
        tar_path_str = tar_path_str.substr(0, tar_path_str.size() - 3);
    } else if (tar_path_str.size() > 7 && tar_path_str.substr(tar_path_str.size() - 7) == ".ar.zst") {
        Logger::info(_("docker.import_tar_detected") + " (zst)");
        extract_cmd = "zstd -dv " + tar_path_str;
        tar_path_str = tar_path_str.substr(0, tar_path_str.size() - 4);
    }

    if (!extract_cmd.empty()) {
        Executor::passthrough(extract_cmd);
    }

    std::string full_name = std::string(image_name) + ":" + std::string(tag);
    std::string cmd = "docker import - \"" + full_name +
                      "\" < \"" + tar_path_str + "\"";
    auto result = Executor::passthrough(cmd);
    if (result.ok()) {
        Logger::ok(_f("docker.import_ok", full_name));
    }
    return result.ok();
}

void DockerManager::import_docker_image_menu() {
    ensure_docker_running();

    // 找起始目录
    std::string start_dir;
    std::string home_dir = std::getenv("HOME") ? std::getenv("HOME") : "";
    for (const auto& d : {home_dir + "/sd/Download/backup/rootfs",
                           std::string{"/media/sd/Download/backup/rootfs"},
                           home_dir + "/sd",
                           home_dir}) {
        if (fs::is_directory(d)) { start_dir = d; break; }
    }
    if (start_dir.empty()) start_dir = "/tmp";

    std::string path_cmd = cfg_.tui_bin +
        " --title \"" + _("docker.tar_path_title") + "\""
        " --inputbox \"" + _("docker.tar_path_input") + "\" 0 50";
    auto result = Executor::passthrough("echo \"$(" + path_cmd + " 2>&1 1>/dev/tty)\"");
    std::string tarpath = result.stdout_data;
    tarpath.erase(std::remove(tarpath.begin(), tarpath.end(), '\n'), tarpath.end());

    if (tarpath.empty() || !fs::exists(tarpath)) {
        Logger::warn(_("docker.tag_hint"));
        return;
    }

    std::string img_cmd = cfg_.tui_bin +
        " --title \"" + _("docker.import_image_title") + "\""
        " --inputbox \"" + _("docker.import_image_input") + "\" 0 50 \"myimage:latest\"";
    result = Executor::passthrough("echo \"$(" + img_cmd + " 2>&1 1>/dev/tty)\"");
    std::string img = result.stdout_data;
    img.erase(std::remove(img.begin(), img.end(), '\n'), img.end());
    if (img.empty()) return;

    auto colon = img.find(':');
    if (colon != std::string::npos) {
        import_image(tarpath, img.substr(0, colon), img.substr(colon + 1));
    } else {
        import_image(tarpath, img);
    }

    // 显示镜像列表
    Executor::passthrough("docker images");

    // 询问是否运行容器
    std::string name_cmd = cfg_.tui_bin +
        " --title \"" + _("docker.container_name_title") + "\""
        " --inputbox \"" + _("docker.container_name_input") + "\" 0 50 \"" +
        (colon != std::string::npos ? img.substr(0, colon) : img) + "\"";
    result = Executor::passthrough("echo \"$(" + name_cmd + " 2>&1 1>/dev/tty)\"");
    std::string cname = result.stdout_data;
    cname.erase(std::remove(cname.begin(), cname.end(), '\n'), cname.end());
    if (cname.empty()) return;

    std::string img_full = (colon != std::string::npos) ? img : img + ":latest";
    std::string confirm = cfg_.tui_bin +
        " --yesno \"docker run -itd --name " + cname + " --env LANG=" + cfg_.locale +
        " --restart on-failure " + img_full + " sh\" 0 50";
    if (Executor::passthrough(confirm).exit_code == 0) {
        run_container(img_full, cname);
    }
}

// ═══════════════════════════════════════════════════════════════
//  Docker 配置
// ═══════════════════════════════════════════════════════════════

bool DockerManager::configure_mirror() {
    Logger::step(_("docker.configuring_mirror"));

    std::string menu = cfg_.tui_bin +
        " --title \"" + _("docker.mirror_title") + "\""
        " --menu \"" + _("docker.mirror_prompt") + "\" 0 0 0 "
        "\"1\" \"" + _("docker.mirror_ustc") + "\" "
        "\"2\" \"" + _("docker.mirror_163") + "\" "
        "\"3\" \"" + _("docker.mirror_aliyun") + "\" "
        "\"4\" \"" + _("docker.mirror_tencent") + "\" "
        "\"0\" \"" + _("menu.tui.back") + "\"";

    std::string choice = Executor::tui_select(menu);
    if (choice == "0" || choice.empty()) return false;

    std::string mirror_url;
    if (choice == "1") mirror_url = "https://docker.mirrors.ustc.edu.cn";
    else if (choice == "2") mirror_url = "https://hub-mirror.c.163.com";
    else if (choice == "3") mirror_url = "https://registry.cn-hangzhou.aliyuncs.com";
    else if (choice == "4") mirror_url = "https://mirror.ccs.tencentyun.com";

    fs::path daemon_dir = "/etc/docker";
    if (!fs::exists(daemon_dir)) fs::create_directories(daemon_dir);

    fs::path daemon_json = daemon_dir / "daemon.json";
    std::string content = "{\n  \"registry-mirrors\": [\"" + mirror_url + "\"]\n}\n";
    std::ofstream ofs(daemon_json);
    if (!ofs) {
        Logger::error(_f("docker.write_daemon_failed", daemon_json.string()));
        return false;
    }
    ofs << content;
    ofs.close();

    Logger::ok(_f("docker.mirror_configured", mirror_url));
    Logger::info(_("docker.restart_docker_hint"));
    return true;
}

void DockerManager::docker_mirror_source() {
    while (true) {
        std::string menu = cfg_.tui_bin +
            " --title \"" + _("docker.mirror_title_sub") + "\""
            " --menu \"" + _("docker.mirror_prompt") + "\" 0 0 0 "
            "\"1\" \"" + _("docker.mirror_163_label") + "\" "
            "\"2\" \"" + _("docker.mirror_edit_daemon") + "\" "
            "\"3\" \"" + _("docker.mirror_edit_source") + "\" "
            "\"4\" \"" + _("docker.mirror_remove_list") + "\" "
            "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        if (choice == "1") {
            configure_mirror();
        } else if (choice == "2") {
            Executor::passthrough("nano /etc/docker/daemon.json 2>/dev/null || vi /etc/docker/daemon.json");
        } else if (choice == "3") {
            if (is_debian_family()) {
                std::string editor = "nano";
                auto e = Executor::shell("which nano 2>/dev/null");
                if (!e.ok()) editor = "vi";
                Executor::passthrough(editor + " /etc/apt/sources.list.d/docker-ce.list 2>/dev/null");
            } else {
                Logger::info(_("docker.unsupported_distro"));
            }
        } else if (choice == "4") {
            Logger::warn(_("docker.mirror_remove_confirm"));
            Logger::info("rm -fv /etc/apt/sources.list.d/docker-ce.list "
                          "/etc/apt/sources.list.d/docker.list "
                          "/usr/share/keyrings/docker-ce-archive-keyring.gpg");
            std::string confirm = cfg_.tui_bin +
                " --yesno \"" + _("docker.mirror_remove_confirm") + "\" 0 50";
            if (Executor::passthrough(confirm).exit_code == 0) {
                Executor::shell("rm -fv /etc/apt/sources.list.d/docker-ce.list "
                                "/etc/apt/sources.list.d/docker.list "
                                "/usr/share/keyrings/docker-ce-archive-keyring.gpg 2>/dev/null");
            }
        }
        Logger::press_enter();
    }
}

bool DockerManager::add_user_to_docker_group() {
    Logger::step(_("docker.adding_user_group"));

    std::string cur_user = "root";
    auto result = Executor::shell("whoami");
    if (result.ok()) {
        cur_user = result.stdout_data;
        cur_user.erase(std::remove(cur_user.begin(), cur_user.end(), '\n'), cur_user.end());
        cur_user.erase(std::remove(cur_user.begin(), cur_user.end(), '\r'), cur_user.end());
    }

    if (cur_user == "root") {
        Logger::info(_("docker.root_no_group_needed"));
        return true;
    }

    Executor::shell("groupadd docker 2>/dev/null");
    bool ok = Executor::shell("gpasswd -a " + cur_user + " docker").ok();
    if (ok) {
        Logger::ok(_f("docker.user_added_to_group", cur_user));
        Logger::info(_("docker.relogin_hint"));
    }
    return ok;
}

bool DockerManager::install_qemu_user_static() {
    Logger::step(_("docker.installing_qemu"));

    // 通过包管理器安装
    if (is_debian_family() || is_redhat_family() || is_arch_family()) {
        std::string install_cmd = cfg_.install_command + " qemu-user-static";
        Executor::passthrough(install_cmd);
    }

    // 通过 Docker 注册 binfmt
    if (is_docker_available()) {
        auto result = Executor::passthrough(
            "docker run --rm --privileged multiarch/qemu-user-static:register --reset 2>/dev/null");
        if (result.ok()) {
            Logger::ok(_("docker.qemu_registered"));
            return true;
        }
    }

    Logger::warn(_("docker.qemu_register_failed"));
    return false;
}

void DockerManager::tmoe_qemu_user_static() {
    while (true) {
        std::string menu = cfg_.tui_bin +
            " --title \"" + _("docker.qemu_menu_title") + "\""
            " --menu \"qemu-user-static\" 0 50 0 "
            "\"1\" \"" + _("docker.qemu_chart") + "\" "
            "\"2\" \"" + _("docker.qemu_install_via_repo") + "\" "
            "\"3\" \"" + _("docker.qemu_install_pkg") + "\" "
            "\"4\" \"" + _("docker.qemu_remove") + "\" "
            "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        if (choice == "1") {
            Logger::info(_("docker.qemu_chart_text"));
        } else if (choice == "2") {
            install_qemu_user_static();
        } else if (choice == "3") {
            // 从 Debian 仓库下载最新版
            if (is_debian_family()) {
                Executor::passthrough("apt install -y qemu-user-static 2>/dev/null");
            } else {
                install_qemu_user_static();
            }
        } else if (choice == "4") {
            Logger::warn("rm -rv /usr/bin/qemu-*-static");
            Logger::warn(cfg_.remove_command + " qemu-user-static");
            std::string confirm = cfg_.tui_bin + " --yesno \"Remove qemu-user-static?\" 0 50";
            if (Executor::passthrough(confirm).exit_code == 0) {
                Executor::shell("rm -rvf /usr/bin/qemu-*-static 2>/dev/null");
                Executor::shell(cfg_.remove_command + " qemu-user-static 2>/dev/null");
            }
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  跨架构运行
// ═══════════════════════════════════════════════════════════════

std::string DockerManager::setup_cross_arch_env(int arch_choice) {
    std::string true_arch = detect_true_arch();

    switch (arch_choice) {
        case 1: // amd64
            new_arch_prefix_ = "amd64";
            container_ext_name_ = "x64";
            if (true_arch != "amd64") qemu_arch_ = "x86_64";
            else qemu_arch_ = "";
            break;
        case 2: // arm64
            new_arch_prefix_ = "arm64v8";
            container_ext_name_ = "arm64";
            if (true_arch != "arm64") qemu_arch_ = "aarch64";
            else qemu_arch_ = "";
            break;
        case 3: // i386
            new_arch_prefix_ = "i386";
            container_ext_name_ = "x86";
            if (true_arch != "i386") qemu_arch_ = "i386";
            else qemu_arch_ = "";
            break;
        case 4: // arm32
            new_arch_prefix_ = "arm32v7";
            container_ext_name_ = "arm";
            if (true_arch != "armhf") qemu_arch_ = "arm";
            else qemu_arch_ = "";
            break;
        case 5: // ppc64le
            new_arch_prefix_ = "ppc64le";
            container_ext_name_ = "ppc";
            if (true_arch != "ppc64el") qemu_arch_ = "ppc64le";
            else qemu_arch_ = "";
            break;
        case 6: // s390x
            new_arch_prefix_ = "s390x";
            container_ext_name_ = "s390";
            if (true_arch != "s390x") qemu_arch_ = "s390x";
            else qemu_arch_ = "";
            break;
        default:
            qemu_arch_ = "";
            new_arch_prefix_ = "";
            container_ext_name_ = "";
            break;
    }

    return qemu_arch_;
}

void DockerManager::run_docker_across_architectures(bool systemd_mode) {
    if (!check_docker_installation()) return;

    current_systemd_ = systemd_mode;
    qemu_arch_ = "";
    new_arch_prefix_ = "";
    container_ext_name_ = "";

    while (true) {
        std::string menu = cfg_.tui_bin +
            " --title \"" + _("docker.cross_arch_title") + "\""
            " --menu \"" + _("docker.cross_arch_prompt") + "\" 0 50 0 "
            "\"0\" \"" + _("menu.tui.back_upper") + "\" "
            "\"00\" \"" + _("docker.qemu_manage") + "\" "
            "\"1\" \"" + _("docker.arch_amd64") + "\" "
            "\"2\" \"" + _("docker.arch_arm64") + "\" "
            "\"3\" \"" + _("docker.arch_i386") + "\" "
            "\"4\" \"" + _("docker.arch_arm32") + "\" "
            "\"5\" \"" + _("docker.arch_ppc64le") + "\" "
            "\"6\" \"" + _("docker.arch_s390x") + "\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        if (choice == "00") {
            tmoe_qemu_user_static();
            Logger::press_enter();
            continue;
        }

        int arch = std::stoi(choice);
        setup_cross_arch_env(arch);

        // 检查 qemu-user-static 是否存在
        if (!qemu_arch_.empty() && !fs::exists("/usr/bin/qemu-x86_64-static")) {
            Logger::info(_("docker.cross_arch_qemu_hint"));
            install_qemu_user_static();
        }

        // systemd 模式下使用特殊列表
        if (systemd_mode) {
            // systemd 模式只支持 centos/fedora
            std::string s_menu = cfg_.tui_bin +
                " --title \"DOCKER IMAGES (systemd)\""
                " --menu \"" + _("docker.distros_prompt") + "\" 0 50 0 "
                "\"0\" \"" + _("menu.tui.back_upper") + "\" "
                "\"1\" \"fedora (redhat community)\" "
                "\"2\" \"centos (enterprise)\"";
            auto sc = Executor::tui_select(s_menu);
            if (sc == "0" || sc.empty()) continue;

            std::string d_name, d_tag1 = "latest", d_cname;
            if (sc == "1") {
                d_name = "fedora";
                d_tag1 = "latest";
                d_cname = "fedora-systemd";
            } else {
                d_name = "centos";
                d_tag1 = "latest";
                d_cname = "cent-systemd";
            }

            // 跨架构前缀
            if (!qemu_arch_.empty()) {
                d_name = new_arch_prefix_ + "/" + d_name;
                d_cname += "_" + container_ext_name_;
            }

            current_docker_tag1_ = d_tag1;
            current_container_name_ = d_cname;
            run_special_tag_docker_container(d_name, d_tag1, d_cname, qemu_arch_, true);
        } else {
            choose_gnu_linux_docker_images(false);
        }

        Logger::press_enter();
    }

    // 退出时重置
    qemu_arch_ = "";
    new_arch_prefix_ = "";
    container_ext_name_ = "";
}

// ═══════════════════════════════════════════════════════════════
//  信息输出
// ═══════════════════════════════════════════════════════════════

void DockerManager::tmoe_docker_readme(const std::string& container_name) const {
    Logger::info(_f("docker.readme_content", container_name));
}

std::string DockerManager::custom_docker_container_tag(const std::string& docker_name,
                                                          const std::string& container_name) {
    std::string docker_url;
    if (docker_name.find('/') != std::string::npos) {
        docker_url = "https://hub.docker.com/r/" + docker_name + "/tags";
    } else {
        docker_url = "https://hub.docker.com/_/" + docker_name + "?tab=tags";
    }

    std::string cmd = cfg_.tui_bin +
        " --title \"" + _("docker.custom_tag_title") + "\""
        " --inputbox \"" + _f("docker.custom_tag_prompt", docker_url) + "\" 0 50";
    auto result = Executor::passthrough("echo \"$(" + cmd + " 2>&1 1>/dev/tty)\"");
    std::string tag = result.stdout_data;
    tag.erase(std::remove(tag.begin(), tag.end(), '\n'), tag.end());

    if (tag.empty()) {
        Logger::warn(_("docker.tag_hint"));
        return "";
    }
    return tag;
}

bool DockerManager::is_docker_available() const {
    return Executor::shell("docker info 2>/dev/null 1>/dev/null").ok();
}

// ═══════════════════════════════════════════════════════════════
//  内部辅助
// ═══════════════════════════════════════════════════════════════

void DockerManager::ensure_docker_running() {
    auto pgrep = Executor::shell("pgrep docker 2>/dev/null");
    if (!pgrep.ok()) {
        Logger::step(_("docker.start_service"));
        Executor::shell("service docker start 2>/dev/null || systemctl start docker 2>/dev/null");
    }
}

bool DockerManager::check_docker_installation() {
    if (!is_docker_available()) {
        Logger::warn(_("docker.docker_not_installed"));
        std::string confirm = cfg_.tui_bin +
            " --yesno \"" + _("docker.docker_not_installed") + "\" 0 50";
        if (Executor::passthrough(confirm).exit_code == 0) {
            return install_docker_ce_or_io();
        }
        return false;
    }
    return true;
}

void DockerManager::docker_init() {
    fs::path mount_dir = "/media/docker";
    if (!fs::exists(mount_dir)) {
        fs::create_directories(mount_dir);
        // 修改权限
        std::string user = Executor::shell("whoami").stdout_data;
        user.erase(std::remove(user.begin(), user.end(), '\n'), user.end());
        user.erase(std::remove(user.begin(), user.end(), '\r'), user.end());
        if (!user.empty()) {
            Executor::shell("chown -R " + user + ":" + user + " " + mount_dir.string() + " 2>/dev/null");
        }
    }

    // 复制配置脚本到挂载目录
    std::string config_file = mount_dir.string() + "/.tmoe-linux-docker.sh";
    if (!fs::exists(config_file)) {
        // 创建基本配置脚本
        std::ofstream ofs(config_file);
        if (ofs) {
            ofs << "#!/bin/sh\n"
                << "# tmoe-linux docker container configuration script\n"
                << "echo 'Container configured via tmoe-linux C++'\n";
            ofs.close();
        }
    }
}

void DockerManager::proot_warning_check() const {
    auto result = Executor::shell("cat /proc/1/cmdline 2>/dev/null | grep -q proot");
    if (result.ok()) {
        Logger::warn(_("docker.proot_warning"));
        Logger::press_enter();
    }
}

std::string DockerManager::detect_linux_release() const {
    auto result = Executor::shell("cat /etc/os-release 2>/dev/null | grep '^ID=' | cut -d= -f2");
    std::string release = result.ok() ? result.stdout_data : "";
    release.erase(std::remove(release.begin(), release.end(), '\n'), release.end());
    release.erase(std::remove(release.begin(), release.end(), '\"'), release.end());
    return release;
}

std::string DockerManager::detect_distro_code() const {
    auto result = Executor::shell("cat /etc/os-release 2>/dev/null | grep '^VERSION_CODENAME=' | cut -d= -f2");
    std::string code = result.ok() ? result.stdout_data : "";
    code.erase(std::remove(code.begin(), code.end(), '\n'), code.end());
    code.erase(std::remove(code.begin(), code.end(), '\"'), code.end());
    return code;
}

std::string DockerManager::os_pretty_name() const {
    return cfg_.os_pretty_name;
}

std::string DockerManager::detect_true_arch() const {
    auto result = Executor::shell("uname -m");
    if (result.ok()) {
        std::string arch = result.stdout_data;
        arch.erase(std::remove(arch.begin(), arch.end(), '\n'), arch.end());
        arch.erase(std::remove(arch.begin(), arch.end(), '\r'), arch.end());

        if (arch == "x86_64" || arch == "amd64") return "amd64";
        if (arch == "aarch64" || arch == "arm64" || arch == "armv8l") return "arm64";
        if (arch == "armv7l" || arch == "armhf") return "armhf";
        if (arch == "i686" || arch == "i386") return "i386";
        if (arch == "ppc64le") return "ppc64el";
        if (arch == "s390x") return "s390x";
        return arch;
    }
    return "amd64";
}

bool DockerManager::is_debian_family() const {
    std::string rel = detect_linux_release();
    return rel == "debian" || rel == "ubuntu" || rel == "kali" ||
           rel == "linuxmint" || rel == "deepin" || rel == "uos";
}

bool DockerManager::is_redhat_family() const {
    std::string rel = detect_linux_release();
    return rel == "fedora" || rel == "centos" || rel == "rhel" ||
           rel == "rocky" || rel == "almalinux";
}

bool DockerManager::is_arch_family() const {
    std::string rel = detect_linux_release();
    return rel == "arch" || rel == "manjaro" || rel == "endeavouros";
}

bool DockerManager::is_alpine() const {
    return detect_linux_release() == "alpine";
}

bool DockerManager::add_docker_debian_repo() {
    Logger::step(_("docker.adding_debian_repo"));

    std::string release = detect_linux_release();
    if (release.empty()) release = "debian";
    std::string code = detect_distro_code();
    if (code.empty()) {
        if (release == "debian") code = "bookworm";
        else if (release == "ubuntu") code = "jammy";
        else code = "stable";
    }

    Executor::passthrough(cfg_.install_command + " ca-certificates curl gnupg lsb-release");
    std::string mirror = "https://mirrors.bfsu.edu.cn/docker-ce/linux/" + release;
    std::string gpg_cmd = "curl -fsSL " + mirror + "/gpg 2>/dev/null | "
                          "gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg 2>/dev/null";
    Executor::passthrough(gpg_cmd);

    std::string repo_line = "deb [arch=$(dpkg --print-architecture) "
                            "signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] "
                            + mirror + " " + code + " stable";

    fs::create_directories("/etc/apt/sources.list.d");
    std::ofstream ofs("/etc/apt/sources.list.d/docker-ce.list");
    if (!ofs) return false;
    ofs << repo_line << "\n";
    ofs.close();

    Executor::passthrough(cfg_.update_command);
    Logger::ok(_("docker.debian_repo_added"));
    return true;
}

bool DockerManager::debian_add_docker_gpg() {
    Logger::step(_("docker.adding_debian_repo"));

    std::string release = detect_linux_release();
    if (release.empty()) release = "debian";

    std::string docker_release = (release == "ubuntu") ? "ubuntu" : "debian";

    // 获取可用的版本代号列表
    std::string code = detect_distro_code();
    if (code.empty()) {
        if (release == "debian") code = "bookworm";
        else if (release == "ubuntu") code = "jammy";
        else code = "stable";
    }

    Executor::passthrough(cfg_.install_command + " ca-certificates curl gnupg lsb-release");

    // GPG 密钥
    Executor::passthrough("curl -fsSL https://mirrors.bfsu.edu.cn/docker-ce/linux/"
                           + docker_release + "/gpg 2>/dev/null | "
                           "gpg --dearmor > /tmp/docker-ce.gpg 2>/dev/null");
    Executor::passthrough("install -o root -g root -m 644 /tmp/docker-ce.gpg "
                           "/usr/share/keyrings/docker-ce-archive-keyring.gpg");

    // 注释已有源
    Executor::shell("sed -i 's@^[^#]*deb @#&@g' /etc/apt/sources.list.d/docker-ce.list 2>/dev/null");

    // TUI 选择源
    std::string src_menu = cfg_.tui_bin +
        " --title \"" + _("docker.debian_repo_title") + "\""
        " --yes-button '" + _("docker.debian_repo_bfsu") + "'"
        " --no-button '" + _("docker.debian_repo_docker") + "'"
        " --yesno \"" + _("docker.debian_repo_prompt") + "\" 0 50";
    auto src_choice = Executor::passthrough(src_menu);

    fs::create_directories("/etc/apt/sources.list.d");
    std::ofstream ofs("/etc/apt/sources.list.d/docker-ce.list", std::ios::app);
    if (!ofs) return false;

    if (src_choice.exit_code == 0) {
        ofs << "deb [signed-by=/usr/share/keyrings/docker-ce-archive-keyring.gpg] "
            << "https://mirrors.bfsu.edu.cn/docker-ce/linux/" << docker_release
            << " " << code << " stable\n";
    } else {
        ofs << "deb [signed-by=/usr/share/keyrings/docker-ce-archive-keyring.gpg] "
            << "https://download.docker.com/linux/" << docker_release
            << " " << code << " stable\n";
    }
    ofs.close();

    Executor::passthrough(cfg_.update_command);
    Logger::ok(_("docker.debian_repo_added"));
    return true;
}

std::string DockerManager::tui_input_tag() {
    std::string cmd = cfg_.tui_bin +
        " --title \"" + _("docker.tag_title") + "\""
        " --inputbox \"" + _("docker.tag_input") + "\" 0 0 \"latest\"";
    auto result = Executor::passthrough("echo \"$(" + cmd + " 2>&1 1>/dev/tty)\"");
    std::string tag = result.stdout_data;
    tag.erase(std::remove(tag.begin(), tag.end(), '\n'), tag.end());
    return tag.empty() ? "latest" : tag;
}

} // namespace tmoe::domain
