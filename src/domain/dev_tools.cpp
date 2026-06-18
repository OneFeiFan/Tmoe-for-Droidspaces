#include "dev_tools.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "package_manager.h"

namespace tmoe::domain {

DeveloperTools::DeveloperTools(const TmoeConfig& cfg) : cfg_(cfg) {}

void DeveloperTools::run_dev_tools_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("devtools.menu_title")
            + "\" --menu \"" + _("devtools.menu_prompt") + "\" 0 0 0 "
            "\"1\" \""  + _("devtools.build_essentials") + "\" "
            "\"2\" \""  + _("devtools.editors") + "\" "
            "\"3\" \""  + _("devtools.languages") + "\" "
            "\"4\" \""  + _("devtools.databases") + "\" "
            "\"5\" \""  + _("devtools.web_servers") + "\" "
            "\"6\" \""  + _("devtools.network_tools") + "\" "
            "\"7\" \""  + _("devtools.shell_enhance") + "\" "
            "\"8\" \""  + _("devtools.monitoring") + "\" "
            "\"0\" \""  + _("menu.tui.back_upper") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;
        if (ch == "1") run_build_menu();
        else if (ch == "2") run_editor_menu();
        else if (ch == "3") run_language_menu();
        else if (ch == "4") run_database_menu();
        else if (ch == "5") run_web_server_menu();
        else if (ch == "6") run_network_menu();
        else if (ch == "7") run_shell_enhance_menu();
        else if (ch == "8") run_monitoring_menu();
        Logger::press_enter();
    }
}

// ==================== 1. 基础构建工具 ====================
void DeveloperTools::run_build_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("devtools.build_menu")
            + "\" --menu \"\" 0 0 0 "
            "\"1\" \""  + _("devtools.gcc_build_essential") + "\" "
            "\"2\" \""  + _("devtools.cmake") + "\" "
            "\"3\" \""  + _("devtools.gdb") + "\" "
            "\"4\" \""  + _("devtools.autotools") + "\" "
            "\"5\" \""  + _("devtools.clang") + "\" "
            "\"6\" \""  + _("devtools.pkg_config") + "\" "
            "\"7\" \""  + _("devtools.strace") + "\" "
            "\"8\" \""  + _("devtools.valgrind") + "\" "
            "\"9\" \""  + _("devtools.ninja") + "\" "
            "\"10\" \"" + _("devtools.meson") + "\" "
            "\"11\" \"" + _("devtools.all_build") + "\" "
            "\"0\" \""  + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        auto family = infer_family_from_config(cfg_.linux_distro);
        if (ch == "1")      install_pkg("devtools.gcc_build_essential", "build-essential");
        else if (ch == "2") install_pkg("devtools.cmake", "cmake");
        else if (ch == "3") install_pkg("devtools.gdb", "gdb");
        else if (ch == "4") install_pkgs({"autoconf", "automake", "libtool", "pkg-config"});
        else if (ch == "5") install_pkgs({"clang", "lldb", "lld"});
        else if (ch == "6") install_pkg("devtools.pkg_config", "pkg-config");
        else if (ch == "7") install_pkg("devtools.strace", "strace");
        else if (ch == "8") install_pkg("devtools.valgrind", "valgrind");
        else if (ch == "9") install_pkg("devtools.ninja", "ninja-build");
        else if (ch == "10") install_pkg("devtools.meson", "meson");
        else if (ch == "11") PackageManager::install(
            {"build-essential", "cmake", "gdb", "autoconf", "automake",
             "libtool", "pkg-config", "clang", "lldb", "strace",
             "valgrind", "ninja-build", "meson", "git"}, family);
    }
}

// ==================== 2. 编辑器/IDE ====================
void DeveloperTools::run_editor_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("devtools.editor_menu")
            + "\" --menu \"\" 0 0 0 "
            "\"1\" \""  + _("devtools.vim") + "\" "
            "\"2\" \""  + _("devtools.neovim") + "\" "
            "\"3\" \""  + _("devtools.emacs") + "\" "
            "\"4\" \""  + _("devtools.vscode") + "\" "
            "\"5\" \""  + _("devtools.code_server") + "\" "
            "\"6\" \""  + _("devtools.geany") + "\" "
            "\"7\" \""  + _("devtools.kate") + "\" "
            "\"8\" \""  + _("devtools.gedit") + "\" "
            "\"9\" \""  + _("devtools.nano") + "\" "
            "\"0\" \""  + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1")      install_pkg("devtools.vim", "vim");
        else if (ch == "2") install_pkg("devtools.neovim", "neovim");
        else if (ch == "3") install_pkg("devtools.emacs", "emacs");
        else if (ch == "4") install_pkg("devtools.vscode", "code");
        else if (ch == "5") {
            // code-server: 尝试包安装，失败则提供 curl 脚本
            Logger::step(_("devtools.code_server"));
            auto family = infer_family_from_config(cfg_.linux_distro);
            if (!PackageManager::install("code-server", family)) {
                Logger::info(_("devtools.code_server_curl"));
                Executor::shell(
                    "curl -fsSL https://code-server.dev/install.sh | sh 2>/dev/null || true"
                );
            }
        }
        else if (ch == "6") install_pkg("devtools.geany", "geany");
        else if (ch == "7") install_pkg("devtools.kate", "kate");
        else if (ch == "8") install_pkg("devtools.gedit", "gedit");
        else if (ch == "9") install_pkg("devtools.nano", "nano");
    }
}

// ==================== 3. 编程语言 ====================
void DeveloperTools::run_language_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("devtools.lang_menu")
            + "\" --menu \"" + _("devtools.lang_prompt") + "\" 0 0 0 "
            "\"1\" \""  + _("devtools.python") + "\" "
            "\"2\" \""  + _("devtools.nodejs") + "\" "
            "\"3\" \""  + _("devtools.java_jdk") + "\" "
            "\"4\" \""  + _("devtools.golang") + "\" "
            "\"5\" \""  + _("devtools.rust") + "\" "
            "\"6\" \""  + _("devtools.ruby") + "\" "
            "\"7\" \""  + _("devtools.php") + "\" "
            "\"8\" \""  + _("devtools.perl") + "\" "
            "\"9\" \""  + _("devtools.lua") + "\" "
            "\"10\" \"" + _("devtools.all_lang") + "\" "
            "\"0\" \""  + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1") run_lang_python();
        else if (ch == "2") run_lang_nodejs();
        else if (ch == "3") run_lang_java();
        else if (ch == "4") run_lang_go();
        else if (ch == "5") run_lang_rust();
        else if (ch == "6") run_lang_ruby();
        else if (ch == "7") run_lang_php();
        else if (ch == "8") run_lang_perl();
        else if (ch == "9") run_lang_lua();
        else if (ch == "10") {
            auto family = infer_family_from_config(cfg_.linux_distro);
            Logger::step(_("devtools.all_lang"));
            PackageManager::install(
                {"python3", "python3-pip", "python3-venv",
                 "nodejs", "npm",
                 "default-jdk", "maven",
                 "golang-go",
                 "rustc", "cargo",
                 "ruby", "ruby-dev",
                 "php", "php-cli", "composer",
                 "perl",
                 "lua5.4", "luajit"}, family);
        }
    }
}

void DeveloperTools::run_lang_python() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"Python\" --menu \"\" 0 0 0 "
        "\"1\" \""  + _("devtools.python3_pip") + "\" "
        "\"2\" \""  + _("devtools.python_venv") + "\" "
        "\"3\" \""  + _("devtools.python_poetry") + "\" "
        "\"4\" \""  + _("devtools.python_conda") + "\" "
        "\"5\" \""  + _("devtools.python_jupyter") + "\" "
        "\"6\" \""  + _("devtools.python_all") + "\" "
        "\"0\" \""  + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;
    if (ch == "1") PackageManager::install({"python3", "python3-pip"}, family);
    else if (ch == "2") PackageManager::install({"python3", "python3-pip", "python3-venv"}, family);
    else if (ch == "3") {
        PackageManager::install({"python3", "python3-pip"}, family);
        Executor::shell("pip3 install poetry 2>/dev/null || curl -sSL https://install.python-poetry.org | python3 - 2>/dev/null || true");
    }
    else if (ch == "4") {
        Logger::info(_("devtools.conda_manual"));
        Executor::shell("curl -fsSL https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -o /tmp/miniconda.sh 2>/dev/null && bash /tmp/miniconda.sh -b -p $HOME/miniconda3 2>/dev/null || true");
    }
    else if (ch == "5") {
        PackageManager::install({"python3", "python3-pip"}, family);
        Executor::shell("pip3 install jupyter jupyterlab 2>/dev/null || true");
    }
    else if (ch == "6") {
        PackageManager::install({"python3", "python3-pip", "python3-venv", "python3-dev"}, family);
        Executor::shell("pip3 install poetry jupyter jupyterlab ipython 2>/dev/null || true");
    }
}

void DeveloperTools::run_lang_nodejs() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"Node.js\" --menu \"\" 0 0 0 "
        "\"1\" \""  + _("devtools.nodejs_npm") + "\" "
        "\"2\" \""  + _("devtools.nodejs_yarn") + "\" "
        "\"3\" \""  + _("devtools.nodejs_pnpm") + "\" "
        "\"4\" \""  + _("devtools.nodejs_nvm") + "\" "
        "\"5\" \""  + _("devtools.nodejs_all") + "\" "
        "\"0\" \""  + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;
    if (ch == "1") PackageManager::install({"nodejs", "npm"}, family);
    else if (ch == "2") {
        PackageManager::install({"nodejs", "npm"}, family);
        Executor::shell("npm install -g yarn 2>/dev/null || true");
    }
    else if (ch == "3") {
        PackageManager::install({"nodejs", "npm"}, family);
        Executor::shell("npm install -g pnpm 2>/dev/null || true");
    }
    else if (ch == "4") {
        Logger::info(_("devtools.nvm_curl"));
        Executor::shell("curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash 2>/dev/null || true");
    }
    else if (ch == "5") {
        PackageManager::install({"nodejs", "npm"}, family);
        Executor::shell("npm install -g yarn pnpm typescript ts-node 2>/dev/null || true");
    }
}

void DeveloperTools::run_lang_java() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"Java\" --menu \"\" 0 0 0 "
        "\"1\" \""  + _("devtools.java_jdk11") + "\" "
        "\"2\" \""  + _("devtools.java_jdk17") + "\" "
        "\"3\" \""  + _("devtools.java_maven") + "\" "
        "\"4\" \""  + _("devtools.java_gradle") + "\" "
        "\"5\" \""  + _("devtools.java_all") + "\" "
        "\"0\" \""  + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;
    if (ch == "1") install_pkg("devtools.java_jdk11", "openjdk-11-jdk");
    else if (ch == "2") install_pkg("devtools.java_jdk17", "openjdk-17-jdk");
    else if (ch == "3") {
        PackageManager::install({"openjdk-11-jdk", "maven"}, family);
    }
    else if (ch == "4") {
        PackageManager::install("openjdk-11-jdk", family);
        Logger::info(_("devtools.gradle_manual"));
        Executor::shell(
            "curl -fsSL https://services.gradle.org/distributions/gradle-8.6-bin.zip "
            "-o /tmp/gradle.zip 2>/dev/null && "
            "unzip -o /tmp/gradle.zip -d /opt/ 2>/dev/null || true"
        );
    }
    else if (ch == "5") {
        PackageManager::install({"openjdk-11-jdk", "openjdk-17-jdk", "maven"}, family);
    }
}

void DeveloperTools::run_lang_go() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"Go\" --menu \"\" 0 0 0 "
        "\"1\" \""  + _("devtools.golang_sdk") + "\" "
        "\"2\" \""  + _("devtools.golang_tools") + "\" "
        "\"0\" \""  + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;
    if (ch == "1") install_pkg("devtools.golang", "golang-go");
    else if (ch == "2") {
        PackageManager::install("golang-go", family);
        Executor::shell(
            "go install golang.org/x/tools/gopls@latest 2>/dev/null; "
            "go install github.com/go-delve/delve/cmd/dlv@latest 2>/dev/null || true"
        );
    }
}

void DeveloperTools::run_lang_rust() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"Rust\" --menu \"\" 0 0 0 "
        "\"1\" \""  + _("devtools.rust_sdk") + "\" "
        "\"2\" \""  + _("devtools.rust_rustup") + "\" "
        "\"3\" \""  + _("devtools.rust_analyzer") + "\" "
        "\"0\" \""  + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;
    if (ch == "1") PackageManager::install({"rustc", "cargo", "rustfmt", "clippy"}, family);
    else if (ch == "2") {
        Logger::info(_("devtools.rustup_curl"));
        Executor::shell(
            "curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | "
            "sh -s -- -y 2>/dev/null || true"
        );
    }
    else if (ch == "3") {
        PackageManager::install({"rustc", "cargo"}, family);
        Executor::shell("rustup component add rust-analyzer 2>/dev/null || true");
    }
}

void DeveloperTools::run_lang_ruby() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"Ruby\" --menu \"\" 0 0 0 "
        "\"1\" \""  + _("devtools.ruby_sdk") + "\" "
        "\"2\" \""  + _("devtools.ruby_rails") + "\" "
        "\"0\" \""  + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;
    if (ch == "1") PackageManager::install({"ruby", "ruby-dev", "rubygems"}, family);
    else if (ch == "2") {
        PackageManager::install({"ruby", "ruby-dev", "rubygems", "nodejs"}, family);
        Executor::shell("gem install rails bundler 2>/dev/null || true");
    }
}

void DeveloperTools::run_lang_php() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"PHP\" --menu \"\" 0 0 0 "
        "\"1\" \""  + _("devtools.php_sdk") + "\" "
        "\"2\" \""  + _("devtools.php_composer") + "\" "
        "\"3\" \""  + _("devtools.php_laravel") + "\" "
        "\"0\" \""  + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;
    if (ch == "1") PackageManager::install({"php", "php-cli", "php-mbstring", "php-xml", "php-curl", "php-zip", "php-mysql"}, family);
    else if (ch == "2") {
        PackageManager::install({"php", "php-cli"}, family);
        Executor::shell(
            "curl -sS https://getcomposer.org/installer | php -- --install-dir=/usr/local/bin --filename=composer 2>/dev/null || true"
        );
    }
    else if (ch == "3") {
        PackageManager::install({"php", "php-cli", "php-mbstring", "php-xml", "php-curl", "php-zip", "php-mysql"}, family);
        Executor::shell(
            "curl -sS https://getcomposer.org/installer | php -- --install-dir=/usr/local/bin --filename=composer 2>/dev/null; "
            "composer global require laravel/installer 2>/dev/null || true"
        );
    }
}

void DeveloperTools::run_lang_perl() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"Perl\" --menu \"\" 0 0 0 "
        "\"1\" \""  + _("devtools.perl_sdk") + "\" "
        "\"2\" \""  + _("devtools.perl_cpanm") + "\" "
        "\"0\" \""  + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;
    if (ch == "1") install_pkg("devtools.perl", "perl");
    else if (ch == "2") {
        PackageManager::install("perl", family);
        Executor::shell("curl -L https://cpanmin.us | perl - --sudo App::cpanminus 2>/dev/null || true");
    }
}

void DeveloperTools::run_lang_lua() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"Lua\" --menu \"\" 0 0 0 "
        "\"1\" \""  + _("devtools.lua_sdk") + "\" "
        "\"2\" \""  + _("devtools.lua_luarocks") + "\" "
        "\"3\" \"LuaJIT\" "
        "\"0\" \""  + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;
    if (ch == "1") PackageManager::install({"lua5.4", "lua5.4-dev"}, family);
    else if (ch == "2") PackageManager::install({"lua5.4", "lua5.4-dev", "luarocks"}, family);
    else if (ch == "3") install_pkg("devtools.lua", "luajit");
}

// ==================== 4. 数据库 ====================
void DeveloperTools::run_database_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("devtools.db_menu")
            + "\" --menu \"\" 0 0 0 "
            "\"1\" \""  + _("devtools.mysql") + "\" "
            "\"2\" \""  + _("devtools.postgresql") + "\" "
            "\"3\" \""  + _("devtools.redis") + "\" "
            "\"4\" \""  + _("devtools.mongodb") + "\" "
            "\"5\" \""  + _("devtools.sqlite") + "\" "
            "\"6\" \""  + _("devtools.mariadb") + "\" "
            "\"7\" \""  + _("devtools.all_db") + "\" "
            "\"0\" \""  + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        auto family = infer_family_from_config(cfg_.linux_distro);
        if (ch == "1")      install_pkg("devtools.mysql", "mysql-server");
        else if (ch == "2") install_pkg("devtools.postgresql", "postgresql");
        else if (ch == "3") install_pkg("devtools.redis", "redis-server");
        else if (ch == "4") {
            Logger::step(_("devtools.mongodb"));
            auto family = infer_family_from_config(cfg_.linux_distro);
            if (family == DistroFamily::Debian) {
                Executor::shell(
                    "curl -fsSL https://www.mongodb.org/static/pgp/server-7.0.asc | "
                    "gpg --dearmor -o /usr/share/keyrings/mongodb-server-7.0.gpg 2>/dev/null; "
                    "echo 'deb [signed-by=/usr/share/keyrings/mongodb-server-7.0.gpg] "
                    "https://repo.mongodb.org/apt/debian bookworm/mongodb-org/7.0 main' > "
                    "/etc/apt/sources.list.d/mongodb-org-7.0.list 2>/dev/null; "
                    "apt-get update && apt-get install -y mongodb-org 2>/dev/null || true"
                );
            } else {
                PackageManager::install("mongodb", family);
            }
        }
        else if (ch == "5") install_pkg("devtools.sqlite", "sqlite3");
        else if (ch == "6") install_pkg("devtools.mariadb", "mariadb-server");
        else if (ch == "7") PackageManager::install(
            {"mysql-server", "postgresql", "redis-server", "sqlite3", "mariadb-server"}, family);
    }
}

// ==================== 5. Web 服务器 ====================
void DeveloperTools::run_web_server_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("devtools.web_menu")
            + "\" --menu \"\" 0 0 0 "
            "\"1\" \""  + _("devtools.nginx") + "\" "
            "\"2\" \""  + _("devtools.apache2") + "\" "
            "\"3\" \""  + _("devtools.caddy") + "\" "
            "\"4\" \""  + _("devtools.lighttpd") + "\" "
            "\"5\" \""  + _("devtools.all_web") + "\" "
            "\"0\" \""  + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1")      install_pkg("devtools.nginx", "nginx");
        else if (ch == "2") install_pkg("devtools.apache2", "apache2");
        else if (ch == "3") {
            Logger::step(_("devtools.caddy"));
            Executor::shell(
                "curl -fsSL https://getcaddy.com | bash -s personal 2>/dev/null || "
                "apt-get install -y caddy 2>/dev/null || true"
            );
        }
        else if (ch == "4") install_pkg("devtools.lighttpd", "lighttpd");
        else if (ch == "5") {
            auto family = infer_family_from_config(cfg_.linux_distro);
            PackageManager::install({"nginx", "apache2", "lighttpd"}, family);
        }
    }
}

// ==================== 6. 网络/API 工具 ====================
void DeveloperTools::run_network_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("devtools.net_menu")
            + "\" --menu \"\" 0 0 0 "
            "\"1\" \""  + _("devtools.curl_wget") + "\" "
            "\"2\" \""  + _("devtools.httpie") + "\" "
            "\"3\" \""  + _("devtools.netcat") + "\" "
            "\"4\" \""  + _("devtools.nmap") + "\" "
            "\"5\" \""  + _("devtools.tcpdump") + "\" "
            "\"6\" \""  + _("devtools.wireshark") + "\" "
            "\"7\" \""  + _("devtools.openssh") + "\" "
            "\"8\" \""  + _("devtools.rsync") + "\" "
            "\"9\" \""  + _("devtools.all_net") + "\" "
            "\"0\" \""  + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1")      install_pkgs({"curl", "wget"});
        else if (ch == "2") install_pkg("devtools.httpie", "httpie");
        else if (ch == "3") install_pkg("devtools.netcat", "netcat-openbsd");
        else if (ch == "4") install_pkg("devtools.nmap", "nmap");
        else if (ch == "5") install_pkg("devtools.tcpdump", "tcpdump");
        else if (ch == "6") install_pkg("devtools.wireshark", "wireshark");
        else if (ch == "7") install_pkg("devtools.openssh", "openssh-server");
        else if (ch == "8") install_pkg("devtools.rsync", "rsync");
        else if (ch == "9") {
            auto family = infer_family_from_config(cfg_.linux_distro);
            PackageManager::install(
                {"curl", "wget", "httpie", "netcat-openbsd", "nmap",
                 "tcpdump", "wireshark", "openssh-server", "rsync"}, family);
        }
    }
}

// ==================== 7. Shell 增强 ====================
void DeveloperTools::run_shell_enhance_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("devtools.shell_menu")
            + "\" --menu \"\" 0 0 0 "
            "\"1\" \""  + _("devtools.tmux") + "\" "
            "\"2\" \""  + _("devtools.screen") + "\" "
            "\"3\" \""  + _("devtools.fzf") + "\" "
            "\"4\" \""  + _("devtools.ripgrep") + "\" "
            "\"5\" \""  + _("devtools.fd_find") + "\" "
            "\"6\" \""  + _("devtools.bat_cat") + "\" "
            "\"7\" \""  + _("devtools.jq") + "\" "
            "\"8\" \""  + _("devtools.yq") + "\" "
            "\"9\" \""  + _("devtools.exa") + "\" "
            "\"10\" \"" + _("devtools.fish") + "\" "
            "\"11\" \"" + _("devtools.zoxide") + "\" "
            "\"12\" \"" + _("devtools.delta") + "\" "
            "\"13\" \"" + _("devtools.all_shell") + "\" "
            "\"0\" \""  + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1")      install_pkg("devtools.tmux", "tmux");
        else if (ch == "2") install_pkg("devtools.screen", "screen");
        else if (ch == "3") install_pkg("devtools.fzf", "fzf");
        else if (ch == "4") install_pkg("devtools.ripgrep", "ripgrep");
        else if (ch == "5") install_pkg("devtools.fd_find", "fd-find");
        else if (ch == "6") install_pkg("devtools.bat_cat", "bat");
        else if (ch == "7") install_pkg("devtools.jq", "jq");
        else if (ch == "8") {
            Logger::step(_("devtools.yq"));
            Executor::shell(
                "pip3 install yq 2>/dev/null || "
                "snap install yq 2>/dev/null || "
                "wget -qO /usr/local/bin/yq https://github.com/mikefarah/yq/releases/latest/download/yq_linux_amd64 2>/dev/null && chmod +x /usr/local/bin/yq 2>/dev/null || true"
            );
        }
        else if (ch == "9") install_pkg("devtools.exa", "exa");
        else if (ch == "10") install_pkg("devtools.fish", "fish");
        else if (ch == "11") install_pkg("devtools.zoxide", "zoxide");
        else if (ch == "12") {
            Logger::step(_("devtools.delta"));
            auto family = infer_family_from_config(cfg_.linux_distro);
            PackageManager::install("git-delta", family);
        }
        else if (ch == "13") {
            auto family = infer_family_from_config(cfg_.linux_distro);
            PackageManager::install(
                {"tmux", "fzf", "ripgrep", "fd-find", "bat",
                 "jq", "fish", "zoxide", "git-delta"}, family);
        }
    }
}

// ==================== 8. 监控/性能 ====================
void DeveloperTools::run_monitoring_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("devtools.mon_menu")
            + "\" --menu \"\" 0 0 0 "
            "\"1\" \""  + _("devtools.htop") + "\" "
            "\"2\" \""  + _("devtools.btop") + "\" "
            "\"3\" \""  + _("devtools.neofetch") + "\" "
            "\"4\" \""  + _("devtools.sysstat") + "\" "
            "\"5\" \""  + _("devtools.iotop") + "\" "
            "\"6\" \""  + _("devtools.iftop") + "\" "
            "\"7\" \""  + _("devtools.nmon") + "\" "
            "\"8\" \""  + _("devtools.glances") + "\" "
            "\"9\" \""  + _("devtools.lsof") + "\" "
            "\"10\" \"" + _("devtools.all_mon") + "\" "
            "\"0\" \""  + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1")      install_pkg("devtools.htop", "htop");
        else if (ch == "2") install_pkg("devtools.btop", "btop");
        else if (ch == "3") install_pkg("devtools.neofetch", "neofetch");
        else if (ch == "4") install_pkg("devtools.sysstat", "sysstat");
        else if (ch == "5") install_pkg("devtools.iotop", "iotop");
        else if (ch == "6") install_pkg("devtools.iftop", "iftop");
        else if (ch == "7") install_pkg("devtools.nmon", "nmon");
        else if (ch == "8") {
            Logger::step(_("devtools.glances"));
            Executor::shell("pip3 install glances 2>/dev/null || apt-get install -y glances 2>/dev/null || true");
        }
        else if (ch == "9") install_pkg("devtools.lsof", "lsof");
        else if (ch == "10") {
            auto family = infer_family_from_config(cfg_.linux_distro);
            PackageManager::install(
                {"htop", "btop", "neofetch", "sysstat",
                 "iotop", "iftop", "nmon", "lsof"}, family);
        }
    }
}

// ==================== 辅助方法 ====================
void DeveloperTools::install_pkg(const std::string& i18n_key, const std::string& pkg) {
    Logger::step(_(i18n_key));
    auto family = infer_family_from_config(cfg_.linux_distro);
    PackageManager::install(pkg, family);
}

void DeveloperTools::install_pkgs(const std::vector<std::string>& pkgs) {
    auto family = infer_family_from_config(cfg_.linux_distro);
    PackageManager::install(pkgs, family);
}

} // namespace tmoe::domain
