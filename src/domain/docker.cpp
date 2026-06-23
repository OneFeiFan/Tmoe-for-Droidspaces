#include "domain/docker.h"
#include "core/i18n.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace tmoe::domain {
    // ── 发行版注册表（17 种，含 tag1/tag2/容器名） ──
    struct DistroSpec {
        const char *image;
        const char *label_key; // i18n key
        const char *tag1;
        const char *tag2;
        const char *container;
    };

    static const DistroSpec DISTRO_SPECS[] = {
        {"alpine", "docker.distros_alpine", "latest", "edge", ""},
        {"debian", "docker.distros_debian", "unstable", "stable", ""},
        {"ubuntu", "docker.distros_ubuntu", "latest", "devel", ""},
        {"kalilinux/kali-rolling", "docker.distros_kali", "latest", "latest", "kali"},
        {"archlinux", "docker.distros_arch", "latest", "", "arch"},
        {"fedora", "docker.distros_fedora", "latest", "rawhide", ""},
        {"centos", "docker.distros_centos", "latest", "7", "cent"},
        {"opensuse/tumbleweed", "docker.distros_opensuse", "latest", "latest", "suse"},
        {"gentoo/stage3-amd64", "docker.distros_gentoo", "latest", "", "gentoo"},
        {"clearlinux", "docker.distros_clearlinux", "latest", "base", "clear"},
        {"voidlinux/voidlinux", "docker.distros_void", "latest", "latest", "void"},
        {"oraclelinux", "docker.distros_oracle", "latest", "7", "oracle"},
        {"amazonlinux", "docker.distros_amazon", "latest", "with-sources", "amazon"},
        {"crux", "docker.distros_crux", "latest", "3.4", ""},
        {"openwrtorg/rootfs", "docker.distros_openwrt", "latest", "", "openwrt"},
        {"alt", "docker.distros_alt", "latest", "sisyphus", ""},
        {"photon", "docker.distros_photon", "latest", "2.0", ""},
    };

    // 旧版简易发行版列表（tui_pull_distro_image 使用）
    static const std::vector<std::pair<std::string, std::string> > DISTRO_IMAGES = {
        {"alpine", "Alpine Linux"},
        {"debian", "Debian Sid"},
        {"ubuntu", "Ubuntu"},
        {"kali-rolling", "Kali Linux"},
        {"archlinux", "Arch Linux"},
        {"fedora", "Fedora"},
        {"centos", "CentOS"},
        {"opensuse/tumbleweed", "openSUSE Tumbleweed"},
        {"gentoo/stage3", "Gentoo"},
        {"clearlinux", "Clear Linux"},
        {"voidlinux/void-linux-x86_64-musl", "Void Linux"},
        {"oraclelinux", "Oracle Linux"},
        {"amazonlinux", "Amazon Linux"},
        {"crux", "CRUX"},
        {"openwrt/rootfs", "OpenWrt"},
        {"alt", "ALT Linux"},
        {"photon", "VMware Photon OS"},
    };

    DockerManager::DockerManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    // ═══════════════════════════════════════════════════════════════
    //  TUI 主菜单
    // ═══════════════════════════════════════════════════════════════

    void DockerManager::run_docker_menu() {
        while (true) {
            std::string available = is_docker_available()
                                        ? _("docker.status_installed")
                                        : _("docker.status_not_installed");

            std::string menu = cfg_.tui_bin +
                               " --title \"" + _("docker.title") + " — " + available + "\""
                               " --menu \"" + _("docker.menu_prompt") + "\" 0 0 0 "
                               "\"1\" \"" + _("docker.menu_pull_distro") + "\" "
                               "\"2\" \"" + _("docker.menu_portainer") + "\" "
                               "\"3\" \"" + _("docker.menu_export") + "\" "
                               "\"4\" \"" + _("docker.menu_import") + "\" "
                               "\"5\" \"" + _("docker.menu_mirror") + "\" "
                               "\"6\" \"" + _("docker.menu_install_ce") + "\" "
                               "\"7\" \"" + _("docker.menu_add_user") + "\" "
                               "\"0\" \"" + _("menu.tui.back_upper") + "\"";

            std::string choice = Executor::tui_select(menu);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                proot_warning_check();
                if (!check_docker_installation()) { Logger::press_enter(); continue; }
                choose_gnu_linux_docker_images(false);
            } else if (choice == "2") {
                if (!check_docker_installation()) { Logger::press_enter(); continue; }
                install_docker_portainer();
            } else if (choice == "3") {
                if (!check_docker_installation()) { Logger::press_enter(); continue; }
                export_docker_image_menu();
            } else if (choice == "4") {
                if (!check_docker_installation()) { Logger::press_enter(); continue; }
                import_docker_image_menu();
            } else if (choice == "5") {
                docker_mirror_source();
            } else if (choice == "6") {
                proot_warning_check();
                install_docker_ce_or_io();
            } else if (choice == "7") {
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
            Executor::passthrough(
                "dnf config-manager --add-repo https://mirrors.bfsu.edu.cn/docker-ce/linux/fedora/docker-ce.repo 2>/dev/null");
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
        // 对应 Bash install_docker_ce_or_io (604-621行)
        std::string menu = cfg_.tui_bin +
                           " --title \"" + _("docker.install_ce_or_io_title") + "\""
                           " --yes-button '" + _("docker.btn_ce") + "'"
                           " --no-button '" + _("docker.btn_io") + "'"
                           " --yesno \"" + _("docker.install_ce_or_io_prompt") + "\" 0 50";
        auto result = Executor::passthrough(menu);
        bool install_ce = result.exit_code == 0;

        bool ok;
        if (install_ce) ok = install_docker_ce();
        else ok = install_docker_io();

        if (ok) Executor::passthrough("docker version 2>/dev/null");
        return ok;
    }

    bool DockerManager::install_docker_portainer() {
        if (!is_docker_available()) {
            Logger::error(_("docker.install_docker_first"));
            return false;
        }

        // 对应 Bash install_docker_portainer (1139-1158行): 默认端口39080
        std::string port_cmd = cfg_.tui_bin +
                               " --title \"" + _("docker.portainer_port_title") + "\""
                               " --inputbox \"" + _("docker.portainer_port_input") + "\" 0 50 \"39080\"";
        std::string port_str = Executor::tui_select(port_cmd);
        int port = port_str.empty() ? 39080 : std::stoi(port_str);

        Logger::step(_("docker.installing_portainer"));
        ensure_docker_running();
        Executor::passthrough("docker stop portainer 2>/dev/null");
        Executor::passthrough("docker pull portainer/portainer-ce:latest");
        Executor::passthrough("docker rm portainer 2>/dev/null");

        std::ostringstream cmd;
        cmd << "docker run -d --name portainer --restart always "
                << "-p " << port << ":9000 "
                << "-v /var/run/docker.sock:/var/run/docker.sock "
                << "-v portainer_data:/data portainer/portainer-ce:latest";
        auto res = Executor::passthrough(cmd.str());
        if (res.ok()) {
            Logger::ok(_("docker.portainer_started"));
            Logger::info(_("docker.portainer_url") + ": http://localhost:" + std::to_string(port));
        }
        return res.ok();
    }

    // ═══════════════════════════════════════════════════════════════
    //  跨架构运行 Docker 容器 (对应 Bash run_docker_across_architectures 1022-1100行)
    // ═══════════════════════════════════════════════════════════════

    void DockerManager::choose_gnu_linux_docker_images(bool systemd_mode) {
        if (!check_docker_installation()) return;


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

        const auto &spec = DISTRO_SPECS[idx];
        current_docker_name_ = spec.image;
        current_docker_tag1_ = spec.tag1;
        current_docker_tag2_ = std::string(spec.tag2);
        current_container_name_ = std::string(spec.container);
        current_docker_name2_ = "";
        current_mgt_menu_type_ = 1;

        // 特殊路由：Kali/Arch/Gentoo/OpenWrt 根据架构选镜像
        std::string true_arch = detect_true_arch();

        if (idx == 3) {
            // Kali
            if (true_arch == "amd64") {
                current_docker_name_ = "kalilinux/kali-rolling";
                current_docker_name2_ = "kalilinux/kali";
            } else if (true_arch == "armhf") {
                current_docker_name_ = "rbartoli/kali-linux-arm";
                current_docker_name2_ = "williamlegourd/kali-gui";
                current_mgt_menu_type_ = 2;
            } else {
                current_docker_name_ = "donaldrich/kali-linux";
                current_docker_name2_ = "heywoodlh/kali-linux";
                current_mgt_menu_type_ = 2;
            }
            current_container_name_ = "kali";
        } else if (idx == 4) {
            // Arch
            if (true_arch == "amd64") {
                current_docker_name_ = "archlinux";
                current_mgt_menu_type_ = 3;
            } else {
                current_docker_name_ = "lopsided/archlinux";
                current_docker_name2_ = "agners/archlinuxarm";
                current_mgt_menu_type_ = 2;
            }
            current_container_name_ = "arch";
        } else if (idx == 8) {
            // Gentoo
            current_container_name_ = "gentoo";
            current_mgt_menu_type_ = 2;
            if (true_arch == "amd64") {
                current_docker_name_ = "gentoo/stage3-amd64";
                current_docker_name2_ = "gentoo/stage3-amd64-hardened-nomultilib";
            } else if (true_arch == "i386") {
                current_docker_name_ = "gentoo/stage3-x86";
                current_docker_name2_ = "gentoo/stage3-x86-hardened";
            } else {
                current_docker_name_ = "paralin/gentoo-stage3-armv7a";
                current_docker_name2_ = "applehq/gentoo-stage4";
            }
        } else if (idx == 14) {
            // OpenWrt
            current_container_name_ = "openwrt";
            current_mgt_menu_type_ = 2;
            if (true_arch == "amd64") {
                current_docker_name_ = "openwrtorg/rootfs";
                current_docker_name2_ = "katta/openwrt-rootfs";
            } else {
                current_docker_name_ = "buddyfly/openwrt-aarch64";
                current_docker_name2_ = "unifreq/openwrt-aarch64";
            }
        } else if (idx == 7) {
            // openSUSE
            current_docker_name2_ = "opensuse/leap";
            current_mgt_menu_type_ = 2;
            current_container_name_ = "suse";
        } else if (idx == 10) {
            // Void
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

        // 进入子菜单
        switch (current_mgt_menu_type_) {
            case 1:
                if (current_docker_tag2_.empty()) {
                    docker_management_menu_03(current_docker_name_, current_container_name_,
                                              current_docker_tag1_, systemd_mode);
                } else {
                    docker_management_menu_01(current_docker_name_, current_container_name_,
                                              current_docker_tag1_, current_docker_tag2_,
                                              systemd_mode);
                }
                break;
            case 2:
                docker_management_menu_02(current_docker_name_, current_docker_name2_,
                                          current_container_name_, current_docker_tag1_,
                                          systemd_mode);
                break;
            case 3:
                docker_management_menu_03(current_docker_name_, current_container_name_,
                                          current_docker_tag1_, systemd_mode);
                break;
        }

        Logger::press_enter();
        choose_gnu_linux_docker_images(systemd_mode);
    }

    // ── 容器管理子菜单 ──

    void DockerManager::docker_management_menu_01(const std::string &docker_name,
                                                  const std::string &container_name,
                                                  const std::string &tag1,
                                                  const std::string &tag2,
                                                  bool systemd) {
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"" + docker_name + " CONTAINER\""
                               " --menu \"" + _("docker.mgt_menu_prompt") + "\" 0 0 0 "
                               "\"1\" \"" + "\"" + tag1 + " (" + _("docker.mgt_run_tag") + ")\" " + "\" "
                               "\"2\" \"" + _f("docker.mgt_tag2", tag2) + "\" "
                               "\"3\" \"" + _("docker.mgt_custom_tag") + "\" "
                               "\"4\" \"" + "\"docker attach " + container_name + "\"" + "\" "
                               "\"5\" \"" + _("docker.mgt_readme") + "\" "
                               "\"6\" \"" + _("docker.mgt_reset") + "\" "
                               "\"7\" \"" + _("docker.mgt_delete") + "\" "
                               "\"0\" \"" + _("menu.tui.back_upper") + "\"";

            std::string choice = Executor::tui_select(menu);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                run_special_tag_docker_container(docker_name, tag1, container_name, systemd);
            } else if (choice == "2") {
                run_special_tag_docker_container(docker_name, tag2, container_name, systemd);
            } else if (choice == "3") {
                std::string custom_tag = custom_docker_container_tag(docker_name, container_name);
                if (!custom_tag.empty()) {
                    run_special_tag_docker_container(docker_name, custom_tag, container_name, systemd);
                }
            } else if (choice == "4") {
                docker_attach_container(container_name, docker_name, tag1, systemd);
            } else if (choice == "5") {
                tmoe_docker_readme(container_name);
            } else if (choice == "6") {
                // 询问确认
                Logger::info("\"reset " + container_name + " (" + docker_name + ":" + tag1 + ")?\"");
                std::string confirm = cfg_.tui_bin +
                                      " --title \"" + _("docker.mgt_reset") + "\""
                                      " --yesno \"" + "\"reset " + container_name + " (" + docker_name + ":" + tag1 + ")?\"" +
                                      "\" 0 50";
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

    void DockerManager::docker_management_menu_02(const std::string &docker_name,
                                                  const std::string &docker_name2,
                                                  const std::string &container_name,
                                                  const std::string &tag1,
                                                  bool systemd) {
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"" + docker_name + " CONTAINER\""
                               " --menu \"" + _("docker.mgt_menu_prompt") + "\" 0 0 0 "
                               "\"1\" \"" + docker_name + "\" "
                               "\"2\" \"" + docker_name2 + "\" "
                               "\"3\" \"" + _("docker.mgt_custom_tag") + "\" "
                               "\"4\" \"" + "\"docker attach " + container_name + "\"" + "\" "
                               "\"5\" \"" + _("docker.mgt_readme") + "\" "
                               "\"6\" \"" + _("docker.mgt_reset") + "\" "
                               "\"7\" \"" + _("docker.mgt_delete") + "\" "
                               "\"0\" \"" + _("menu.tui.back_upper") + "\"";

            std::string choice = Executor::tui_select(menu);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                run_special_tag_docker_container(docker_name, tag1, container_name, systemd);
            } else if (choice == "2") {
                run_special_tag_docker_container(docker_name2, tag1, container_name, systemd);
            } else if (choice == "3") {
                std::string ct = custom_docker_container_tag(docker_name, container_name);
                if (!ct.empty()) {
                    run_special_tag_docker_container(docker_name, ct, container_name, systemd);
                }
            } else if (choice == "4") {
                docker_attach_container(container_name, docker_name, tag1, systemd);
            } else if (choice == "5") {
                tmoe_docker_readme(container_name);
            } else if (choice == "6") {
                std::string confirm = cfg_.tui_bin +
                                      " --title \"" + _("docker.mgt_reset") + "\""
                                      " --yesno \"" + "\"reset " + container_name + " (" + docker_name + ":" + tag1 + ")?\"" +
                                      "\" 0 50";
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

    void DockerManager::docker_management_menu_03(const std::string &docker_name,
                                                  const std::string &container_name,
                                                  const std::string &tag1,
                                                  bool systemd) {
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"" + docker_name + " CONTAINER\""
                               " --menu \"" + _("docker.mgt_menu_prompt") + "\" 0 0 0 "
                               "\"1\" \"" + "\"" + tag1 + " (" + _("docker.mgt_run_tag") + ")\" " + "\" "
                               "\"2\" \"" + _("docker.mgt_custom_tag") + "\" "
                               "\"3\" \"" + "\"docker attach " + container_name + "\"" + "\" "
                               "\"4\" \"" + _("docker.mgt_readme") + "\" "
                               "\"5\" \"" + _("docker.mgt_reset") + "\" "
                               "\"6\" \"" + _("docker.mgt_delete") + "\" "
                               "\"0\" \"" + _("menu.tui.back_upper") + "\"";

            std::string choice = Executor::tui_select(menu);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                run_special_tag_docker_container(docker_name, tag1, container_name, systemd);
            } else if (choice == "2") {
                std::string ct = custom_docker_container_tag(docker_name, container_name);
                if (!ct.empty()) {
                    run_special_tag_docker_container(docker_name, ct, container_name, systemd);
                }
            } else if (choice == "3") {
                docker_attach_container(container_name, docker_name, tag1, systemd);
            } else if (choice == "4") {
                tmoe_docker_readme(container_name);
            } else if (choice == "5") {
                std::string confirm = cfg_.tui_bin +
                                      " --yesno \"" + "\"reset " + container_name + " (" + docker_name + ":" + tag1 + ")?\"" +
                                      "\" 0 50";
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

    bool DockerManager::run_special_tag_docker_container(const std::string &docker_name,
                                                         const std::string &docker_tag,
                                                         const std::string &container_name,
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
                Logger::info(_("docker.mount_volume_hint") + ": " + mount_dir.string());
            }
            Logger::info("\"docker exec -it " + container_name + " sh\"");

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
                    Executor::passthrough(
                        "docker exec -it " + container_name + " /bin/bash 2>/dev/null || docker attach " +
                        container_name);
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

        std::string mount = mount_path.empty() ? cfg_.container_root.string() : std::string(mount_path);
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

    // ═══════════════════════════════════════════════════════════════
    //  容器管理
    // ═══════════════════════════════════════════════════════════════

    std::vector<std::string> DockerManager::list_containers(bool all) const {
        std::vector<std::string> result;
        std::string flag = all ? "-a" : "";
        auto exec = Executor::shell(
            "docker ps " + flag + " --format \"{{.Names}} [{{.Image}}] {{.Status}}\" 2>/dev/null");
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

    bool DockerManager::delete_container_and_image(const std::string &docker_name,
                                                   const std::string &docker_tag,
                                                   const std::string &docker_tag2,
                                                   const std::string &container_name) {
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

    bool DockerManager::docker_attach_container(const std::string &container_name,
                                                const std::string &docker_name,
                                                const std::string &docker_tag,
                                                bool systemd) {
        Logger::step(_("docker.attach_hint"));
        ensure_docker_running();

        // 检查容器是否存在
        auto check = Executor::shell(
            "docker ps -a --format \"{{.Names}}\" 2>/dev/null | grep -q \"^" + container_name + "$\"");
        if (!check.ok()) {
            Logger::warn(_f("docker.attach_not_found", container_name));
            Logger::info(_f("docker.attach_pull_prompt", docker_name));
            std::string confirm = cfg_.tui_bin +
                                  " --yesno \"" + _f("docker.attach_pull_prompt", docker_name) + "\" 0 50";
            if (Executor::passthrough(confirm).exit_code == 0) {
                run_special_tag_docker_container(docker_name, docker_tag, container_name,
                                                 systemd);
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

    bool DockerManager::reset_docker_container(const std::string &docker_name,
                                               const std::string &docker_tag,
                                               const std::string &container_name,
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
                                                systemd);
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

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) return;

        // 选择保存路径
        std::string start_dir;
        std::string home_dir = std::getenv("HOME") ? std::getenv("HOME") : "";
        for (const auto &d: {
                 home_dir + "/sd/Download/backup/rootfs",
                 std::string{"/media/sd/Download/backup/rootfs"},
                 home_dir + "/sd",
                 home_dir
             }) {
            if (fs::is_directory(d)) {
                start_dir = d;
                break;
            }
        }
        if (start_dir.empty()) start_dir = "/tmp";

        std::string path_cmd = cfg_.tui_bin +
                               " --title \"FILE PATH\""
                               " --inputbox \"" + start_dir + "\" 0 50 \"" + start_dir + "\"";
        std::string target = Executor::tui_select(path_cmd);
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
        for (const auto &d: {
                 home_dir + "/sd/Download/backup/rootfs",
                 std::string{"/media/sd/Download/backup/rootfs"},
                 home_dir + "/sd",
                 home_dir
             }) {
            if (fs::is_directory(d)) {
                start_dir = d;
                break;
            }
        }
        if (start_dir.empty()) start_dir = "/tmp";

        std::string path_cmd = cfg_.tui_bin +
                               " --title \"" + _("docker.tar_path_title") + "\""
                               " --inputbox \"" + _("docker.tar_path_input") + "\" 0 50";
        std::string tarpath = Executor::tui_select(path_cmd);

        if (tarpath.empty() || !fs::exists(tarpath)) {
            Logger::warn(_("docker.tag_hint"));
            return;
        }

        std::string img_cmd = cfg_.tui_bin +
                              " --title \"" + _("docker.import_image_title") + "\""
                              " --inputbox \"" + _("docker.import_image_input") + "\" 0 50 \"myimage:latest\"";
        std::string img = Executor::tui_select(img_cmd);
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
        std::string cname = Executor::tui_select(name_cmd);
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

        // 对应 Bash: 优先用 $SUDO_USER（sudo提权时的原始用户），
        // 其次从 $HOME 在 /etc/passwd 中反查，最后用 whoami
        std::string cur_user;
        const char* sudo_user = std::getenv("SUDO_USER");
        if (sudo_user && sudo_user[0] != '\0') {
            cur_user = sudo_user;
        } else {
            const char* home = std::getenv("HOME");
            if (home) {
                auto result = Executor::shell(
                    std::string("grep \"") + home + "\" /etc/passwd | awk -F':' '{print $1}' | head -n 1");
                if (result.ok() && !result.stdout_data.empty()) {
                    cur_user = result.stdout_data;
                    cur_user.erase(std::remove(cur_user.begin(), cur_user.end(), '\n'), cur_user.end());
                }
            }
            if (cur_user.empty()) {
                auto result = Executor::shell("whoami");
                if (result.ok()) {
                    cur_user = result.stdout_data;
                    cur_user.erase(std::remove(cur_user.begin(), cur_user.end(), '\n'), cur_user.end());
                    cur_user.erase(std::remove(cur_user.begin(), cur_user.end(), '\r'), cur_user.end());
                }
            }
        }

        if (cur_user == "root" || cur_user.empty()) {
            Logger::info(_("docker.root_no_group_needed"));
            return true;
        }

        Executor::shell("groupadd docker 2>/dev/null");
        bool ok = Executor::shell("gpasswd -a " + cur_user + " docker").ok();
        if (ok) {
            Logger::ok(_("docker.user_added_to_group") + ": " + cur_user);
            Logger::info(_("docker.relogin_hint"));
        }
        return ok;
    }

    // ═══════════════════════════════════════════════════════════════
    //  信息输出
    // ═══════════════════════════════════════════════════════════════

    void DockerManager::tmoe_docker_readme(const std::string &container_name) const {
        Logger::info(_("docker.readme_content"));
    }

    std::string DockerManager::custom_docker_container_tag(const std::string &docker_name,
                                                           const std::string &container_name) {
        std::string docker_url;
        if (docker_name.find('/') != std::string::npos) {
            docker_url = "https://hub.docker.com/r/" + docker_name + "/tags";
        } else {
            docker_url = "https://hub.docker.com/_/" + docker_name + "?tab=tags";
        }

        std::string cmd = cfg_.tui_bin +
                          " --title \"" + _("docker.custom_tag_title") + "\""
                          " --inputbox \"" + _("docker.custom_tag_prompt") + "\n" + docker_url + "\" 0 50";
        std::string tag = Executor::tui_select(cmd);

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
            // 对应 Bash check_docker_installation (1132-1137行): 直接提示并安装，无确认弹窗
            Logger::warn(_("docker.docker_not_installed_prompt"));
            return install_docker_ce_or_io();
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
        std::string tag = Executor::tui_select(cmd);
        if (tag.empty()) return "latest";
        return tag;
    }
} // namespace tmoe::domain
