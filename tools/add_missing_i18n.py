#!/usr/bin/env python3
"""Add missing i18n keys to locale files with sensible default translations."""
import json, re, os

BASE = r'E:\tmoe-cpp'
LOCALES = os.path.join(BASE, 'locales')
SRC_DIR = os.path.join(BASE, 'src')

# Collect ALL _("...") keys from entire src/
all_keys = set()
for root, dirs, files in os.walk(SRC_DIR):
    for fname in files:
        if fname.endswith('.cpp') or fname.endswith('.h'):
            fpath = os.path.join(root, fname)
            with open(fpath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                keys = re.findall(r'_\(\"([^\"]+)\"\)', content)
                all_keys.update(keys)

# Load existing
with open(os.path.join(LOCALES, 'zh_CN.json'), 'r', encoding='utf-8') as f:
    zh = json.load(f)
with open(os.path.join(LOCALES, 'en_US.json'), 'r', encoding='utf-8') as f:
    en = json.load(f)

missing = sorted(k for k in all_keys if k not in zh)
print(f"Total keys in src: {len(all_keys)}, existing: {len(zh)}, missing: {len(missing)}")

# Domain-specific translations
ZH_MAP = {
    "menu.tui.config": "⚙️ 系统配置",
    "menu.tui.software_center": "🛒 软件中心",
    "menu.tui.terminal_app": "💻 终端应用",
    "menu.tui.office": "📝 办公套件",
    "menu.tui.education": "📚 教育与学习",
    "menu.tui.input_method": "⌨️ 输入法管理",
    "menu.tui.browser": "🌐 浏览器管理",
    "menu.tui.beta": "🧪 测试功能 (Beta)",
    "menu.tui.dev_tools": "🔧 开发工具",
    "menu.tui.download_tools": "📥 下载工具",
    "zsh.comp_title": "🌈 Zsh 终端美化",
}
EN_MAP = {
    "menu.tui.config": "⚙️ System Config",
    "menu.tui.software_center": "🛒 Software Center",
    "menu.tui.terminal_app": "💻 Terminal Apps",
    "menu.tui.office": "📝 Office Suite",
    "menu.tui.education": "📚 Education & Learning",
    "menu.tui.input_method": "⌨️ Input Method",
    "menu.tui.browser": "🌐 Browser Manager",
    "menu.tui.beta": "🧪 Beta Features",
    "menu.tui.dev_tools": "🔧 Dev Tools",
    "menu.tui.download_tools": "📥 Download Tools",
    "zsh.comp_title": "🌈 Zsh Terminal Themes",
}

# Auto-generate for others
for key in missing:
    if key not in ZH_MAP:
        parts = key.split('.')
        name = parts[-1].replace('_', ' ').title() if len(parts) > 1 else key
        ZH_MAP[key] = name  # fallback
        EN_MAP[key] = name

# Also add specific translations for known domains
DOMAIN_ZH = {
    "config": {
        "title": "系统配置", "menu_prompt": "请选择配置项:",
        "dns": "DNS 配置", "hostname": "主机名设置",
        "locale": "区域/语言", "timezone": "时区设置",
        "password": "密码管理", "shared_dirs": "共享目录",
        "fortune": "Fortune 终端格言",
    },
    "beta": {
        "menu_title": "Beta 功能", "menu_prompt": "请选择测试功能:",
        "flameshot": "Flameshot 截图工具", "telegram": "Telegram 桌面版",
        "scrcpy": "Scrcpy 安卓投屏", "seahorse": "Seahorse 密钥管理",
        "kodi": "Kodi 媒体中心", "aptitude": "Aptitude 包管理器",
        "deepin_store": "Deepin 应用商店", "gnome_store": "GNOME 软件中心",
        "flatpak_snap": "Flatpak/Snap 支持", "drawing": "绘图工具",
        "r_lang": "R 语言", "file_mgr": "文件管理器",
        "multimedia": "多媒体工具", "store_menu": "应用商店选择",
        "deepin_menu": "Deepin 工具套件", "drawing_menu": "绘图工具选择",
        "r_lang_menu": "R 语言工具", "file_menu": "文件管理工具",
        "multimedia_menu": "多媒体工具选择",
        "deepin_calculator": "Deepin 计算器", "deepin_screen_recorder": "Deepin 屏幕录制",
        "deepin_album": "Deepin 相册", "deepin_draw": "Deepin 画板",
        "deepin_music": "Deepin 音乐", "deepin_movie": "Deepin 影院",
        "deepin_compressor": "Deepin 压缩", "deepin_picker": "Deepin 取色器",
        "deepin_font_manager": "Deepin 字体管理", "deepin_system_monitor": "Deepin 系统监视器",
        "krita": "Krita 数字绘画", "inkscape": "Inkscape 矢量图",
        "kolourpaint": "KolourPaint 绘图", "latexdraw": "LaTeXDraw",
        "librecad": "LibreCAD 2D CAD", "freecad": "FreeCAD 3D CAD",
        "kicad": "KiCad PCB 设计", "openscad": "OpenSCAD",
        "gnuplot": "Gnuplot 科学绘图", "rstudio": "RStudio IDE",
        "r_base": "R 语言基础环境", "r_rstudio": "R + RStudio 套装",
        "thunar": "Thunar 文件管理器", "nautilus": "Nautilus (GNOME Files)",
        "dolphin": "Dolphin (KDE)", "catfish": "Catfish 文件搜索",
        "gparted": "GParted 分区编辑器", "baobab": "Baobab 磁盘分析",
        "gnome_disk": "GNOME 磁盘工具", "partitionmanager": "KDE 分区管理器",
        "mc": "Midnight Commander", "ranger": "Ranger 终端文件管理器",
        "obs_studio": "OBS Studio 录屏直播", "evince": "Evince 文档查看器",
        "okular": "Okular 文档查看器", "kchmviewer": "KCHMViewer CHM 阅读器",
        "pdfchain": "PDF Chain PDF 工具", "xournal": "Xournal 笔记",
        "calibre": "Calibre 电子书管理", "fbreader": "FBReader 电子书阅读器",
    },
    "browser": {
        "menu_title": "浏览器管理", "menu_prompt": "请选择浏览器:",
        "firefox": "Firefox 火狐浏览器", "chromium": "Chromium 浏览器",
        "edge": "Microsoft Edge 浏览器", "falkon": "Falkon 浏览器",
        "vivaldi": "Vivaldi 浏览器", "midori": "Midori 浏览器",
        "epiphany": "Epiphany (GNOME Web)", "firefox_ppa": "添加 Firefox PPA 源",
        "chromium_ppa": "添加 Chromium PPA 源", "edge_repo": "添加 Edge 软件源",
        "no_sandbox_wrapper": "创建无沙箱启动脚本",
        "no_sandbox_info": "非沙箱模式可用于 root 环境",
        "no_sandbox_desktop": "创建 .desktop 启动器",
        "wrapper_created": "启动脚本已创建",
    },
    "devtools": {
        "menu_title": "开发工具", "menu_prompt": "请选择开发工具类别:",
        "build_menu": "构建工具", "build_essentials": "Build Essential 构建工具链",
        "all_build": "全部构建工具",
        "gcc_build_essential": "GCC + Build Essential", "clang": "Clang/LLVM 编译器",
        "cmake": "CMake 构建系统", "meson": "Meson 构建系统",
        "ninja": "Ninja 构建加速", "autotools": "Autotools (autoconf/automake)",
        "pkg_config": "pkg-config", "gdb": "GDB 调试器",
        "valgrind": "Valgrind 内存检测", "strace": "Strace 系统调用追踪",
        "lsof": "lsof 文件/网络诊断",
        "databases": "数据库", "db_menu": "数据库工具",
        "all_db": "全部数据库",
        "mariadb": "MariaDB 数据库", "mysql": "MySQL 数据库",
        "postgresql": "PostgreSQL 数据库", "mongodb": "MongoDB 数据库",
        "redis": "Redis 缓存", "sqlite": "SQLite 嵌入式数据库",
        "editors": "编辑器/IDE", "editor_menu": "编辑器与 IDE",
        "vim": "Vim 编辑器", "neovim": "Neovim 编辑器",
        "emacs": "Emacs 编辑器", "nano": "Nano 编辑器",
        "gedit": "Gedit 文本编辑器", "geany": "Geany IDE",
        "kate": "Kate 高级编辑器", "vscode": "VS Code",
        "code_server": "Code Server (Web VS Code)",
        "code_server_curl": "通过 curl 安装 Code Server",
        "web_servers": "Web 服务器", "web_menu": "Web 服务器选择",
        "all_web": "全部 Web 服务器",
        "nginx": "Nginx", "apache2": "Apache2",
        "caddy": "Caddy Web Server", "lighttpd": "Lighttpd",
        "languages": "编程语言", "lang_menu": "编程语言",
        "all_lang": "全部语言 SDK",
        "python": "Python 3", "python3_pip": "pip 包管理器",
        "python_venv": "venv 虚拟环境", "python_poetry": "Poetry 包管理",
        "python_all": "Python 全家桶", "python_conda": "Conda/Miniconda",
        "python_jupyter": "Jupyter Notebook", "conda_manual": "请手动安装 Miniconda",
        "nodejs": "Node.js", "nodejs_npm": "npm 包管理器",
        "nodejs_nvm": "nvm 版本管理器", "nodejs_pnpm": "pnpm",
        "nodejs_yarn": "Yarn", "nodejs_all": "Node.js 全家桶",
        "nvm_curl": "通过 curl 安装 nvm",
        "golang": "Go 语言", "golang_sdk": "Go SDK",
        "golang_tools": "Go 常用工具",
        "rust": "Rust 语言", "rust_sdk": "Rust SDK",
        "rust_rustup": "通过 rustup 安装", "rust_analyzer": "Rust Analyzer",
        "rustup_curl": "通过 curl 安装 rustup",
        "ruby": "Ruby 语言", "ruby_sdk": "Ruby SDK",
        "ruby_rails": "Rails 框架",
        "perl": "Perl 语言", "perl_sdk": "Perl SDK",
        "perl_cpanm": "cpanminus 模块管理",
        "php": "PHP 语言", "php_sdk": "PHP SDK",
        "php_composer": "Composer 包管理", "php_laravel": "Laravel 框架",
        "lua": "Lua 语言", "lua_sdk": "Lua SDK",
        "lua_luarocks": "LuaRocks 包管理",
        "java_jdk": "Java JDK", "java_jdk11": "OpenJDK 11",
        "java_jdk17": "OpenJDK 17", "java_maven": "Maven 构建工具",
        "java_gradle": "Gradle 构建工具", "java_all": "Java 全家桶",
        "gradle_manual": "请手动安装 Gradle",
        "monitoring": "监控与运维", "mon_menu": "监控工具",
        "all_mon": "全部监控工具",
        "btop": "Btop 资源监视器", "htop": "Htop 进程查看器",
        "glances": "Glances 系统监控", "neofetch": "Neofetch 系统信息",
        "nmon": "nmon 性能监控", "iotop": "Iotop 磁盘 IO 监控",
        "iftop": "Iftop 网络流量监控", "sysstat": "Sysstat 系统统计",
        "network_tools": "网络工具", "net_menu": "网络工具",
        "all_net": "全部网络工具",
        "nmap": "Nmap 网络扫描", "tcpdump": "TCPDump 抓包",
        "wireshark": "Wireshark 协议分析", "netcat": "Netcat 网络调试",
        "curl_wget": "curl + wget", "httpie": "HTTPie",
        "openssh": "OpenSSH 服务端", "rsync": "rsync 同步工具",
        "shell_enhance": "Shell 增强工具", "shell_menu": "Shell 增强",
        "all_shell": "全部 Shell 工具",
        "fish": "Fish Shell", "fzf": "Fzf 模糊搜索",
        "zoxide": "Zoxide 智能跳转", "tmux": "Tmux 终端复用器",
        "screen": "Screen 终端复用器", "delta": "Delta Git 差异工具",
        "ripgrep": "Ripgrep 代码搜索", "fd_find": "Fd 文件搜索",
        "bat_cat": "Bat (cat 增强版)", "exa": "Exa (ls 增强版)",
        "jq": "jq JSON 处理", "yq": "yq YAML 处理",
        "lang_prompt": "请选择编程语言:",
    },
    "download": {
        "menu_title": "下载工具", "menu_prompt": "请选择下载工具:",
        "aria2_menu": "Aria2 下载管理器", "aria2_install": "安装 Aria2",
        "aria2_installing": "正在安装 Aria2...", "aria2_start": "启动 Aria2",
        "aria2_stop": "停止 Aria2", "aria2_config": "配置 Aria2",
        "aria2_manager": "Aria2 管理", "aria2_configured": "Aria2 配置完成",
        "aria2_rpc_port": "RPC 端口: 6800", "aria2_webui": "Aria2 WebUI",
        "aria2_web_installing": "正在安装 Aria2 WebUI...",
        "aria2_web_done": "Aria2 WebUI 安装完成",
        "video_dl": "视频下载工具",
        "yt_dlp": "yt-dlp (YouTube等)", "yt_dlp_pip": "通过 pip 安装 yt-dlp",
        "yt_dlp_example": "yt-dlp <视频URL>",
        "you_get": "You-Get 视频下载", "you_get_example": "you-get <视频URL>",
        "annie": "Annie (lux) 视频下载", "lux": "Lux 视频下载",
        "lux_example": "lux <视频URL>",
        "gallery_dl": "Gallery-DL 图库下载",
        "crawler": "爬虫与镜像工具", "crawler_installing": "正在安装爬虫工具...",
        "crawler_aria2_batch": "Aria2 批量下载", "crawler_curl_batch": "Curl 批量下载",
        "crawler_httrack": "HTTrack 网站镜像", "crawler_scrapy": "Scrapy 爬虫框架",
        "crawler_wget_mirror": "Wget 镜像下载", "httrack_example": "httrack <网站URL>",
        "wget_mirror_example": "wget -m <URL>",
        "thunder": "迅雷 (Xunlei)", "thunder_install": "安装迅雷",
        "thunder_web": "迅雷 Web 版", "thunder_info": "迅雷 Linux 兼容版",
    },
    "edu": {
        "menu_title": "教育与学习", "menu_prompt": "请选择学习类别:",
        "gaokao": "高考备考", "gaokao_menu": "高考科目",
        "gaokao_math": "高考数学", "gaokao_physics": "高考物理",
        "gaokao_chemistry": "高考化学", "gaokao_biology": "高考生物",
        "kaoyan": "考研备考", "kaoyan_menu": "考研科目",
        "kaoyan_math": "考研数学", "kaoyan_english": "考研英语",
        "kaoyan_politics": "考研政治",
        "english": "英语学习", "english_menu": "英语工具",
        "cet4": "CET-4 四级", "cet6": "CET-6 六级",
        "exam": "考试备考", "exam_menu": "考试工具",
        "math": "数学工具", "math_menu": "数学软件",
        "maxima": "Maxima 代数系统", "octave": "GNU Octave",
        "scilab": "Scilab", "freemat": "FreeMat",
        "geogebra": "GeoGebra 几何", "physics": "物理工具",
        "physics_menu": "物理与工程",
        "geant4": "Geant4 粒子物理", "openfoam": "OpenFOAM CFD",
        "gromacs": "GROMACS 分子动力学", "cp2k": "CP2K 量子化学",
        "nwchem": "NWChem 计算化学", "chemistry": "化学工具",
        "chem_menu": "化学与分子",
        "avogadro": "Avogadro 分子编辑", "kalzium": "Kalzium 元素周期表",
        "pymol": "PyMOL 分子可视化", "step": "Step 物理模拟",
        "notes": "笔记与知识管理",
        "goldendict": "GoldenDict 词典", "goldendict_wordnet": "WordNet 词典数据",
        "fcitx5_moegirl": "萌娘百科词库 (Fcitx5)", "fcitx5_zhwiki": "中文维基词库 (Fcitx5)",
        "download_done": "下载完成", "downloading": "正在下载...",
    },
    "input": {
        "menu_title": "输入法管理", "menu_prompt": "请选择输入法框架:",
        "fcitx4": "Fcitx4 输入法", "fcitx5": "Fcitx5 输入法",
        "ibus": "IBus 输入法", "sogou": "搜狗拼音",
        "fcitx4_menu": "Fcitx4 配置", "fcitx5_menu": "Fcitx5 配置",
        "ibus_menu": "IBus 配置",
        "fcitx4_tools": "Fcitx4 工具套件", "fcitx4_rime": "Rime 中州韵 (Fcitx4)",
        "fcitx4_libpinyin": "LibPinyin 智能拼音", "fcitx4_sunpinyin": "SunPinyin 拼音",
        "fcitx4_googlepinyin": "Google 拼音", "fcitx4_cloudpinyin": "云拼音",
        "fcitx4_kde_config": "KDE 配置工具",
        "fcitx5_core": "Fcitx5 核心 (带中文)", "fcitx5_rime": "Rime 中州韵 (Fcitx5)",
        "fcitx5_material": "Material 主题", "fcitx5_kcm": "KCM 配置工具",
        "fcitx5_kimpanel": "KimPanel 面板", "fcitx5_qt_gtk": "Qt/GTK 输入模块",
        "ibus_pinyin": "IBus Pinyin 拼音", "ibus_rime": "IBus Rime",
        "ibus_sunpinyin": "IBus SunPinyin", "ibus_chewing": "IBus Chewing 注音",
        "ibus_onboard": "Onboard 虚拟键盘",
        "autostart": "配置自启动", "env_writing": "正在写入环境变量...",
        "env_done": "环境变量配置完成",
        "faq": "常见问题 (FAQ)", "faq_1": "Fcitx5 不工作？",
        "faq_2": "IBus 不工作？", "faq_3": "搜狗拼音不工作？",
        "faq_4": "如何切换输入法？", "faq_5": "如何添加词库？",
        "faq_6": "Emoji 输入支持？",
    },
    "office": {
        "menu_title": "办公套件", "menu_prompt": "请选择办公软件:",
        "libreoffice": "LibreOffice 办公套件", "libreoffice_zh": "LibreOffice 中文语言包",
        "wps": "WPS Office 办公套件", "wps_fonts": "WPS 字体包",
        "freeoffice": "FreeOffice 办公套件", "yozo": "永中 Office",
        "yozo_license": "永中 Office 许可证",
        "meld": "Meld 文件对比", "kdiff3": "KDiff3 文件对比",
        "manpages_zh": "中文 Man 手册", "font_cache": "刷新字体缓存",
        "proot_patch": "Proot 字体补丁",
    },
    "swcenter": {
        "menu_title": "软件中心", "menu_prompt": "请选择软件类别:",
        "games": "游戏", "games_menu": "游戏选择",
        "steam": "Steam 游戏平台", "wesnoth": "Battle for Wesnoth",
        "supertuxkart": "SuperTuxKart 赛车", "cataclysm": "Cataclysm: DDA",
        "retroarch": "RetroArch 模拟器", "dolphin_emu": "Dolphin 模拟器",
        "media": "多媒体", "media_menu": "多媒体工具",
        "mpv": "MPV 播放器", "smplayer": "SMPlayer",
        "parole": "Parole 播放器", "clementine": "Clementine 音乐播放器",
        "audacity": "Audacity 音频编辑", "ardour": "Ardour 专业音频",
        "obs_studio": "OBS Studio 录屏直播", "peek": "Peek GIF 录制",
        "gimp": "GIMP 图像处理", "kolourpaint": "KolourPaint 绘图",
        "sns": "社交与即时通讯", "sns_menu": "社交工具",
        "qq": "QQ", "wechat": "微信",
        "skype": "Skype", "mitalk": "米聊",
        "empathy": "Empathy 即时通讯", "pidgin": "Pidgin 即时通讯",
        "xchat": "XChat IRC", "evolution": "Evolution 邮件客户端",
        "kmail": "KMail 邮件客户端", "thunderbird": "Thunderbird 邮件",
        "docs": "办公文档", "docs_menu": "文档工具",
        "docs_wps": "WPS Office", "docs_baidu": "百度文库下载",
        "docs_qq": "腾讯文档", "docs_thunder": "迅雷文档",
        "fileshare": "文件共享", "fileshare_menu": "文件共享工具",
        "filebrowser": "File Browser (Web 文件管理)", "nginx_webdav": "Nginx WebDAV",
        "cleanup": "系统清理", "cleanup_purge": "彻底清除 (purge)",
        "cleanup_autoremove": "自动移除 (autoremove)", "cleanup_running": "正在清理...",
        "cleanup_pkg_name": "包名", "cleanup_pkg_prompt": "请输入要清除的包名:",
        "baidunetdisk": "百度网盘", "bleachbit": "BleachBit 系统清理",
        "gdebi": "GDebi 包安装器",
        "pkg_gui": "图形化包管理", "pkg_gui_menu": "包管理 GUI",
        "synaptic": "Synaptic 包管理器", "pamac": "Pamac 包管理器",
        "downloading": "正在下载...",
    },
    "term": {
        "menu_title": "终端应用", "menu_prompt": "请选择终端应用:",
    },
    "dns": {
        "title": "DNS 配置", "menu_prompt": "请选择 DNS 服务器:",
        "applying": "正在应用 DNS 配置...", "failed": "DNS 配置失败",
    },
    "hostname": {
        "title": "主机名设置", "prompt": "请输入新主机名:",
        "current": "当前主机名", "failed": "主机名设置失败",
    },
    "tz": {
        "title": "时区设置", "select_region": "选择区域",
        "select_city": "选择城市", "current": "当前时区",
        "failed": "时区设置失败",
    },
    "locale": {
        "title": "区域/语言设置", "select_region": "选择语言区域",
        "select_prompt": "请选择:", "current": "当前设置",
        "failed": "设置失败",
    },
    "passwd": {
        "title": "密码管理", "enter": "请输入密码:",
        "confirm": "请确认密码:", "mismatch": "密码不匹配",
        "changed": "密码已修改", "cancelled": "已取消",
        "failed": "密码修改失败",
    },
    "sharedir": {
        "title": "共享目录", "menu_prompt": "共享目录配置",
        "saved": "共享目录已保存",
    },
    "app": {
        "installing": "正在安装", "install_done": "安装完成",
        "install_failed": "安装失败", "removing": "正在卸载",
        "installing_eatmydata": "正在安装 eatmydata...",
        "retry_without_eatmydata": "尝试不带 eatmydata 重试...",
        "updating": "正在更新",
    },
}

# Now add to locale files
for key in missing:
    parts = key.split('.')
    domain = parts[0]
    sub = '.'.join(parts[1:])

    # Chinese: use domain-specific map, then ZH_MAP, then fallback
    zh_val = None
    if domain in DOMAIN_ZH and sub in DOMAIN_ZH[domain]:
        zh_val = DOMAIN_ZH[domain][sub]
    if zh_val is None:
        zh_val = ZH_MAP.get(key, sub.replace('_', ' '))

    # English: use EN_MAP, or generate readable label from key
    en_val = EN_MAP.get(key)
    if en_val is None:
        # Convert snake_case to Title Case
        en_val = sub.replace('_', ' ').title()

    zh[key] = zh_val
    en[key] = en_val

# Write back sorted
zh_sorted = dict(sorted(zh.items()))
en_sorted = dict(sorted(en.items()))

with open(os.path.join(LOCALES, 'zh_CN.json'), 'w', encoding='utf-8') as f:
    json.dump(zh_sorted, f, ensure_ascii=False, indent=2)

with open(os.path.join(LOCALES, 'en_US.json'), 'w', encoding='utf-8') as f:
    json.dump(en_sorted, f, ensure_ascii=False, indent=2)

print(f"Done! Added {len(missing)} keys to locale files.")
print(f"zh_CN now has {len(zh_sorted)} keys, en_US now has {len(en_sorted)} keys.")
