#include "domain/docker.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace tmoe::domain {

// 17 种支持的 Linux 发行版 Docker 镜像
static const std::vector<std::pair<std::string, std::string>> DISTRO_IMAGES = {
    {"alpine",        "Alpine Linux — 最小体积"},
    {"debian",        "Debian Sid — 滚动更新"},
    {"ubuntu",        "Ubuntu — 最流行"},
    {"kali-rolling",  "Kali Linux — 渗透测试"},
    {"archlinux",     "Arch Linux — 极简主义"},
    {"fedora",        "Fedora — 前沿技术"},
    {"centos",        "CentOS — 企业级稳定"},
    {"opensuse/tumbleweed", "openSUSE Tumbleweed"},
    {"gentoo/stage3", "Gentoo — 从源码编译"},
    {"clearlinux",    "Clear Linux — Intel 优化"},
    {"voidlinux/void-linux-x86_64-musl", "Void Linux"},
    {"oraclelinux",   "Oracle Linux"},
    {"amazonlinux",   "Amazon Linux"},
    {"crux",          "CRUX — BSD 风格"},
    {"openwrt/rootfs","OpenWrt — 嵌入式"},
    {"alt",           "ALT Linux"},
    {"photon",        "VMware Photon OS"},
};

DockerManager::DockerManager(const TmoeConfig& cfg) : cfg_(cfg) {}

// ═══════════════════════════════════════════════════════════════
//  TUI 主菜单
// ═══════════════════════════════════════════════════════════════

void DockerManager::run_docker_menu() {
    while (true) {
        std::string available = is_docker_available() ?
            "✅ Docker 已安装" : "❌ Docker 未安装";

        std::string menu = cfg_.tui_bin +
            " --title \"🐳 Docker 容器管理 — " + available + "\""
            " --menu \"请选择操作:\" 0 0 0 "
            "\"1\" \"📥 安装 Docker CE / Docker.io\" "
            "\"2\" \"⬇️  拉取 Linux 发行版镜像 (17种)\" "
            "\"3\" \"🚀 运行 Docker 容器\" "
            "\"4\" \"🌐 安装 Portainer Web 管理\" "
            "\"5\" \"💾 导出/导入容器\" "
            "\"6\" \"🔧 配置 Docker 镜像源\" "
            "\"7\" \"👥 添加用户到 docker 组\" "
            "\"8\" \"🔄 安装 qemu-user-static (跨架构)\" "
            "\"9\" \"📋 列出镜像/容器\" "
            "\"0\" \"返回上级菜单\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        if (choice == "1") {
            std::string sub_menu = cfg_.tui_bin +
                " --title \"选择版本\" --menu \"安装哪个版本?\" 0 0 0 "
                "\"1\" \"Docker CE (社区版 — 推荐)\" "
                "\"2\" \"Docker.io (发行版仓库版)\"";
            std::string sub = Executor::tui_select(sub_menu);
            if (sub == "2") install_docker_io();
            else install_docker_ce();
        } else if (choice == "2") {
            tui_pull_distro_image();
        } else if (choice == "3") {
            std::string img_cmd = cfg_.tui_bin +
                " --title \"镜像\" --inputbox \"输入镜像名 (如 debian:latest):\" 0 0 \"debian:latest\"";
            auto result = Executor::shell("echo \"$(" + img_cmd + " 2>&1 1>/dev/tty)\"");
            std::string image = result.stdout_data;
            image.erase(std::remove(image.begin(), image.end(), '\n'), image.end());
            if (image.empty()) continue;

            std::string name_cmd = cfg_.tui_bin +
                " --title \"容器名\" --inputbox \"输入容器名称:\" 0 0 \"tmoe-container\"";
            result = Executor::shell("echo \"$(" + name_cmd + " 2>&1 1>/dev/tty)\"");
            std::string name = result.stdout_data;
            name.erase(std::remove(name.begin(), name.end(), '\n'), name.end());
            if (name.empty()) continue;

            run_container(image, name);
        } else if (choice == "4") {
            std::string port_cmd = cfg_.tui_bin +
                " --title \"Portainer 端口\" --inputbox \"输入 Portainer 访问端口:\" 0 0 \"9000\"";
            auto result = Executor::shell("echo \"$(" + port_cmd + " 2>&1 1>/dev/tty)\"");
            std::string port_str = result.stdout_data;
            port_str.erase(std::remove(port_str.begin(), port_str.end(), '\n'), port_str.end());
            int port = port_str.empty() ? 9000 : std::stoi(port_str);
            install_portainer(port);
        } else if (choice == "5") {
            std::string sub_menu = cfg_.tui_bin +
                " --title \"导入/导出\" --menu \"选择操作:\" 0 0 0 "
                "\"1\" \"📤 导出容器\" "
                "\"2\" \"📥 导入镜像\"";
            std::string sub = Executor::tui_select(sub_menu);
            if (sub == "1") {
                std::string name_cmd = cfg_.tui_bin +
                    " --title \"容器名\" --inputbox \"输入要导出的容器名:\" 0 0";
                auto result = Executor::shell("echo \"$(" + name_cmd + " 2>&1 1>/dev/tty)\"");
                std::string cname = result.stdout_data;
                cname.erase(std::remove(cname.begin(), cname.end(), '\n'), cname.end());
                if (!cname.empty()) {
                    std::string out = cfg_.backup_dir.string() + "/docker_" + cname + ".tar";
                    export_container(cname, out);
                }
            } else {
                std::string path_cmd = cfg_.tui_bin +
                    " --title \"TAR 路径\" --inputbox \"输入 .tar 文件路径:\" 0 0";
                auto result = Executor::shell("echo \"$(" + path_cmd + " 2>&1 1>/dev/tty)\"");
                std::string tarpath = result.stdout_data;
                tarpath.erase(std::remove(tarpath.begin(), tarpath.end(), '\n'), tarpath.end());
                if (!tarpath.empty() && fs::exists(tarpath)) {
                    std::string img_cmd = cfg_.tui_bin +
                        " --title \"镜像名\" --inputbox \"输入镜像名:tag:\" 0 0 \"myimage:latest\"";
                    result = Executor::shell("echo \"$(" + img_cmd + " 2>&1 1>/dev/tty)\"");
                    std::string img = result.stdout_data;
                    img.erase(std::remove(img.begin(), img.end(), '\n'), img.end());
                    if (!img.empty()) {
                        auto colon = img.find(':');
                        if (colon != std::string::npos) {
                            import_image(tarpath, img.substr(0, colon), img.substr(colon + 1));
                        } else {
                            import_image(tarpath, img);
                        }
                    }
                }
            }
        } else if (choice == "6") {
            configure_mirror();
        } else if (choice == "7") {
            add_user_to_docker_group();
        } else if (choice == "8") {
            install_qemu_user_static();
        } else if (choice == "9") {
            auto images = list_images();
            Logger::info("=== Docker 镜像 ===");
            for (const auto& img : images) {
                Logger::info("  📦 " + img.full_name());
            }
            auto containers = list_containers(true);
            Logger::info("=== Docker 容器 ===");
            for (const auto& c : containers) {
                Logger::info("  📦 " + c);
            }
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  Docker 安装
// ═══════════════════════════════════════════════════════════════

bool DockerManager::install_docker_ce() {
    Logger::step("安装 Docker CE...");

    if (is_debian_family()) {
        if (!add_docker_debian_repo()) {
            Logger::error("添加 Docker 软件源失败");
            return false;
        }
        std::string install_cmd = cfg_.install_command;
        // 替换 apt install -y 为 apt install -y docker-ce docker-ce-cli containerd.io
        size_t pos = install_cmd.find("install -y");
        if (pos != std::string::npos) {
            install_cmd = install_cmd.substr(0, pos) + "install -y docker-ce docker-ce-cli containerd.io";
        } else {
            install_cmd = "apt install -y docker-ce docker-ce-cli containerd.io";
        }
        return Executor::shell(install_cmd).ok();
    }

    if (is_redhat_family()) {
        Executor::shell("dnf config-manager --add-repo https://download.docker.com/linux/fedora/docker-ce.repo 2>/dev/null");
        return Executor::shell("dnf install -y docker-ce docker-ce-cli containerd.io").ok();
    }

    if (is_arch_family()) {
        return Executor::shell("pacman -S --noconfirm docker").ok();
    }

    if (is_alpine()) {
        Executor::shell("apk add docker docker-cli-compose");
        Executor::shell("rc-update add docker boot");
        return Executor::shell("service docker start").ok();
    }

    Logger::warn("当前发行版不在自动安装支持列表中，请手动安装 Docker");
    return false;
}

bool DockerManager::install_docker_io() {
    Logger::step("安装 Docker.io...");

    if (is_debian_family()) {
        return Executor::shell(cfg_.install_command + " docker.io").ok();
    }
    Logger::warn("Docker.io 仅在 Debian/Ubuntu 上可用，使用 Docker CE 替代");
    return install_docker_ce();
}

bool DockerManager::install_portainer(int port) {
    Logger::step("安装 Portainer Web 管理界面...");

    if (!is_docker_available()) {
        Logger::error("请先安装 Docker");
        return false;
    }

    std::ostringstream cmd;
    cmd << "docker run -d --name portainer --restart=always "
        << "-p " << port << ":9000 "
        << "-v /var/run/docker.sock:/var/run/docker.sock "
        << "portainer/portainer-ce:latest";
    auto result = Executor::shell(cmd.str());
    if (result.ok()) {
        Logger::ok("Portainer 已启动！");
        Logger::info("访问地址: http://localhost:" + std::to_string(port));
    }
    return result.ok();
}

// ═══════════════════════════════════════════════════════════════
//  镜像管理
// ═══════════════════════════════════════════════════════════════

bool DockerManager::pull_image(std::string_view image, std::string_view tag) {
    Logger::step("拉取 Docker 镜像: " + std::string(image) + ":" + std::string(tag));
    std::string full = std::string(image) + ":" + std::string(tag);
    return Executor::shell("docker pull " + full).ok();
}

std::vector<DockerImageInfo> DockerManager::list_images() const {
    std::vector<DockerImageInfo> result;
    auto exec = Executor::shell("docker images --format \"{{.Repository}} {{.Tag}}\"");
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
    // 构建 17 种发行版的 TUI 菜单
    std::string menu = cfg_.tui_bin +
        " --title \"选择 Linux 发行版镜像\" --menu \"请选择要拉取的镜像:\" 0 0 0 ";

    for (size_t i = 0; i < DISTRO_IMAGES.size(); ++i) {
        menu += "\"" + std::to_string(i + 1) + "\" \""
             + DISTRO_IMAGES[i].second + "\" ";
    }
    menu += "\"0\" \"返回\"";

    std::string choice = Executor::tui_select(menu);
    if (choice == "0" || choice.empty()) return false;

    int idx = std::stoi(choice) - 1;
    if (idx < 0 || idx >= static_cast<int>(DISTRO_IMAGES.size())) return false;

    std::string image_name = DISTRO_IMAGES[idx].first;
    std::string tag = tui_input_tag();

    return pull_image(image_name, tag);
}

// ═══════════════════════════════════════════════════════════════
//  容器管理
// ═══════════════════════════════════════════════════════════════

bool DockerManager::run_container(std::string_view image,
                                   std::string_view name,
                                   std::string_view mount_path,
                                   int host_port, int container_port) {
    Logger::step("运行 Docker 容器: " + std::string(name));

    std::ostringstream cmd;
    cmd << "docker run -itd --name \"" << name << "\""
        << " --privileged=true"
        << " --env LANG=" << cfg_.locale
        << " --restart on-failure";

    // 持久化挂载
    std::string mount = mount_path.empty() ?
        cfg_.container_root.string() : std::string(mount_path);
    if (!mount.empty() && fs::exists(mount)) {
        cmd << " -v \"" << mount << "\":\"" << mount << "\"";
    }

    // 端口映射
    if (host_port > 0 && container_port > 0) {
        cmd << " -p " << host_port << ":" << container_port;
    }

    cmd << " \"" << image << "\"";

    auto result = Executor::shell(cmd.str());
    if (result.ok()) {
        Logger::ok("容器已启动: " + std::string(name));
    }
    return result.ok();
}

bool DockerManager::run_cross_arch_container(std::string_view image,
                                               std::string_view name,
                                               std::string_view arch) {
    Logger::step("跨架构运行容器 (arch: " + std::string(arch) + ")");

    // 确保 qemu-user-static 已注册
    install_qemu_user_static();

    std::ostringstream cmd;
    cmd << "docker run -itd --name \"" << name << "\""
        << " --privileged=true"
        << " --env LANG=" << cfg_.locale
        << " --restart on-failure"
        << " \"" << image << "\"";

    return Executor::shell(cmd.str()).ok();
}

std::vector<std::string> DockerManager::list_containers(bool all) const {
    std::vector<std::string> result;
    std::string flag = all ? "-a" : "";
    auto exec = Executor::shell("docker ps " + flag + " --format \"{{.Names}} [{{.Image}}] {{.Status}}\"");
    if (!exec.ok()) return result;

    std::istringstream iss(exec.stdout_data);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty()) result.push_back(line);
    }
    return result;
}

bool DockerManager::remove_container(std::string_view name) {
    Logger::step("移除 Docker 容器: " + std::string(name));
    // 先停止再删除
    Executor::shell("docker stop \"" + std::string(name) + "\" 2>/dev/null");
    return Executor::shell("docker rm \"" + std::string(name) + "\"").ok();
}

// ═══════════════════════════════════════════════════════════════
//  导入/导出
// ═══════════════════════════════════════════════════════════════

bool DockerManager::export_container(std::string_view name,
                                       std::string_view output_path) {
    Logger::step("导出 Docker 容器: " + std::string(name));

    // 确保输出目录存在
    fs::path out(output_path);
    if (!fs::exists(out.parent_path())) {
        fs::create_directories(out.parent_path());
    }

    std::string cmd = "docker export \"" + std::string(name) +
                      "\" > \"" + std::string(output_path) + "\"";
    auto result = Executor::shell(cmd);
    if (result.ok()) {
        auto size_mb = fs::file_size(output_path) / (1024 * 1024);
        Logger::ok("容器已导出: " + std::string(output_path) +
                   " (" + std::to_string(size_mb) + " MB)");
    }
    return result.ok();
}

bool DockerManager::import_image(std::string_view tar_path,
                                   std::string_view image_name,
                                   std::string_view tag) {
    Logger::step("导入 Docker 镜像: " + std::string(image_name) + ":" + std::string(tag));

    std::string full_name = std::string(image_name) + ":" + std::string(tag);
    std::string cmd = "docker import - \"" + full_name +
                      "\" < \"" + std::string(tar_path) + "\"";
    auto result = Executor::shell(cmd);
    if (result.ok()) {
        Logger::ok("镜像已导入: " + full_name);
    }
    return result.ok();
}

// ═══════════════════════════════════════════════════════════════
//  Docker 配置
// ═══════════════════════════════════════════════════════════════

bool DockerManager::configure_mirror() {
    Logger::step("配置 Docker 镜像源...");

    std::string menu = cfg_.tui_bin +
        " --title \"Docker 镜像源\" --menu \"选择镜像源:\" 0 0 0 "
        "\"1\" \"中科大 USTC\" "
        "\"2\" \"网易 163\" "
        "\"3\" \"阿里云\" "
        "\"4\" \"腾讯云\" "
        "\"0\" \"返回\"";

    std::string choice = Executor::tui_select(menu);
    if (choice == "0" || choice.empty()) return false;

    std::string mirror_url;
    if (choice == "1") mirror_url = "https://docker.mirrors.ustc.edu.cn";
    else if (choice == "2") mirror_url = "https://hub-mirror.c.163.com";
    else if (choice == "3") mirror_url = "https://registry.cn-hangzhou.aliyuncs.com";
    else if (choice == "4") mirror_url = "https://mirror.ccs.tencentyun.com";

    // 创建/修改 daemon.json
    fs::path daemon_dir = "/etc/docker";
    if (!fs::exists(daemon_dir)) fs::create_directories(daemon_dir);

    fs::path daemon_json = daemon_dir / "daemon.json";

    // 使用简单写入
    std::string content = "{\n  \"registry-mirrors\": [\"" + mirror_url + "\"]\n}\n";
    std::ofstream ofs(daemon_json);
    if (!ofs) {
        Logger::error("无法写入 " + daemon_json.string());
        return false;
    }
    ofs << content;
    ofs.close();

    Logger::ok("Docker 镜像源已配置为: " + mirror_url);
    Logger::info("请重启 Docker 服务使配置生效: systemctl restart docker");
    return true;
}

bool DockerManager::add_user_to_docker_group() {
    Logger::step("将当前用户添加到 docker 组...");

    std::string cur_user = "root";
    auto result = Executor::shell("whoami");
    if (result.ok()) {
        cur_user = result.stdout_data;
        cur_user.erase(std::remove(cur_user.begin(), cur_user.end(), '\n'), cur_user.end());
        cur_user.erase(std::remove(cur_user.begin(), cur_user.end(), '\r'), cur_user.end());
    }

    if (cur_user == "root") {
        Logger::info("当前为 root 用户，无需添加到 docker 组");
        return true;
    }

    Executor::shell("groupadd docker 2>/dev/null");
    bool ok = Executor::shell("usermod -aG docker " + cur_user).ok();
    if (ok) {
        Logger::ok("用户 " + cur_user + " 已添加到 docker 组");
        Logger::info("请重新登录使更改生效");
    }
    return ok;
}

bool DockerManager::install_qemu_user_static() {
    Logger::step("安装 qemu-user-static 跨架构支持...");

    // 方法 1: 通过包管理器安装
    if (is_debian_family() || is_redhat_family() || is_arch_family()) {
        std::string install_cmd = cfg_.install_command + " qemu-user-static";
        Executor::shell(install_cmd);
    }

    // 方法 2: 通过 Docker 注册 binfmt
    if (is_docker_available()) {
        auto result = Executor::shell(
            "docker run --rm --privileged multiarch/qemu-user-static:register --reset 2>/dev/null");
        if (result.ok()) {
            Logger::ok("qemu-user-static 已注册 — 支持跨架构运行容器");
            return true;
        }
    }

    Logger::warn("qemu-user-static 注册可能未成功");
    return false;
}

bool DockerManager::is_docker_available() const {
    return Executor::shell("docker info 2>/dev/null 1>/dev/null").ok();
}

// ═══════════════════════════════════════════════════════════════
//  内部辅助
// ═══════════════════════════════════════════════════════════════

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
    Logger::step("添加 Docker Debian 软件源...");

    std::string release = detect_linux_release();
    if (release.empty()) release = "debian";

    // 检测版本代号
    std::string code = detect_distro_code();
    if (code.empty()) {
        if (release == "debian") code = "bookworm";
        else if (release == "ubuntu") code = "jammy";
        else code = "stable";
    }

    // 安装依赖
    Executor::shell(cfg_.install_command + " ca-certificates curl gnupg lsb-release");

    // 下载 GPG 密钥
    std::string mirror = "https://mirrors.bfsu.edu.cn/docker-ce/linux/" + release;
    std::string gpg_cmd = "curl -fsSL " + mirror + "/gpg 2>/dev/null | "
                          "gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg 2>/dev/null";
    Executor::shell(gpg_cmd);

    // 写 sources.list
    std::string repo_line = "deb [arch=$(dpkg --print-architecture) "
                            "signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] "
                            + mirror + " " + code + " stable";

    // 确保目录存在
    fs::create_directories("/etc/apt/sources.list.d");

    std::ofstream ofs("/etc/apt/sources.list.d/docker-ce.list");
    if (!ofs) return false;
    ofs << repo_line << "\n";
    ofs.close();

    // 更新索引
    Executor::shell(cfg_.update_command);

    Logger::ok("Docker 软件源已添加");
    return true;
}

std::string DockerManager::tui_input_tag() {
    std::string cmd = cfg_.tui_bin +
        " --title \"标签\" --inputbox \"输入镜像标签:\" 0 0 \"latest\"";
    auto result = Executor::shell("echo \"$(" + cmd + " 2>&1 1>/dev/tty)\"");
    std::string tag = result.stdout_data;
    tag.erase(std::remove(tag.begin(), tag.end(), '\n'), tag.end());
    return tag.empty() ? "latest" : tag;
}

} // namespace tmoe::domain
