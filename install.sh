#!/usr/bin/env bash
# tmoes 一键安装脚本
#
# 用法（兼容 curl / wget，自动回退）:
#   (curl -fsSL https://raw.githubusercontent.com/OneFeiFan/Tmoe-for-Droidspaces/main/install.sh || wget -qO- https://raw.githubusercontent.com/OneFeiFan/Tmoe-for-Droidspaces/main/install.sh) | bash
#
# 自定义安装路径:
#   INSTALL_DIR=~/.local/bin (curl ... || wget ...) | bash
set -euo pipefail

REPO="OneFeiFan/Tmoe-for-Droidspaces"
INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"
BINARY_NAME="tmoes"

# ── 颜色 ──
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { printf "${GREEN}[+]${NC} %s\n" "$*"; }
warn()  { printf "${YELLOW}[!]${NC} %s\n" "$*"; }
err()   { printf "${RED}[x]${NC} %s\n" "$*"; exit 1; }

# ── 检测 OS / Arch ──
detect_platform() {
    case "$(uname -s)" in
        Linux)  OS="Linux" ;;
        Darwin) OS="macOS" ;;
        *)      err "不支持的操作系统: $(uname -s)" ;;
    esac

    case "$(uname -m)" in
        x86_64|amd64)   ARCH="x86_64" ;;
        aarch64|arm64)  ARCH="ARM64"  ;;
        armv7l)         ARCH="armv7l" ;;
        *)              err "不支持的架构: $(uname -m)" ;;
    esac

    info "检测到: ${OS} / ${ARCH}"
}

# ── 获取最新 release 下载 URL ──
fetch_latest() {
    local api="https://api.github.com/repos/${REPO}/releases/latest"

    if command -v curl &>/dev/null; then
        RELEASE_JSON=$(curl -fsSL "${api}" 2>/dev/null) || err "无法访问 GitHub API (curl)"
    elif command -v wget &>/dev/null; then
        RELEASE_JSON=$(wget -qO- "${api}" 2>/dev/null) || err "无法访问 GitHub API (wget)"
    else
        err "需要 curl 或 wget"
    fi

    TAG_NAME=$(echo "$RELEASE_JSON" | grep -oP '"tag_name":\s*"\K[^"]+')
    [[ -z "$TAG_NAME" ]] && err "未找到任何 release，请先创建 tag 并发布"

    # 匹配产物名: tmoes-v1.0.0-Linux-x86_64
    ASSET_PATTERN="tmoes-${TAG_NAME}-${OS}-${ARCH}"
    DOWNLOAD_URL=$(echo "$RELEASE_JSON" | grep -oP '"browser_download_url":\s*"\K[^"]*'"${ASSET_PATTERN}"'[^"]*')

    if [[ -z "$DOWNLOAD_URL" ]]; then
        # 尝试小写 os 变体
        local os_lower=$(echo "${OS}" | tr '[:upper:]' '[:lower:]')
        ASSET_PATTERN="tmoes-${TAG_NAME}-${os_lower}-${ARCH}"
        DOWNLOAD_URL=$(echo "$RELEASE_JSON" | grep -oP '"browser_download_url":\s*"\K[^"]*'"${ASSET_PATTERN}"'[^"]*')
    fi

    [[ -z "$DOWNLOAD_URL" ]] && err "未找到匹配的构建产物: ${ASSET_PATTERN}"

    info "最新版本: ${TAG_NAME}"
    info "下载地址: ${DOWNLOAD_URL}"
}

# ── 下载 & 安装 ──
install_binary() {
    local tmpdir; tmpdir=$(mktemp -d)
    local tmpfile="${tmpdir}/${BINARY_NAME}"
    trap "rm -rf ${tmpdir}" EXIT

    info "下载中..."
    if command -v curl &>/dev/null; then
        curl -fsSL -o "${tmpfile}" "${DOWNLOAD_URL}" || err "下载失败"
    else
        wget -q -O "${tmpfile}" "${DOWNLOAD_URL}" || err "下载失败"
    fi

    chmod +x "${tmpfile}"

    local dest="${INSTALL_DIR}/${BINARY_NAME}"
    if [[ -w "${INSTALL_DIR}" ]]; then
        mv "${tmpfile}" "${dest}"
    else
        info "需要提权写入 ${INSTALL_DIR}"
        sudo mv "${tmpfile}" "${dest}"
        sudo chmod 755 "${dest}"
    fi

    info "安装完成: ${dest}"
}

# ── 验证 ──
verify() {
    if command -v "${BINARY_NAME}" &>/dev/null; then
        printf "${CYAN}"
        "${BINARY_NAME}" --version 2>/dev/null || "${BINARY_NAME}" --help 2>/dev/null | head -1 || true
        printf "${NC}"
        info "运行 tmoes 开始使用"
    else
        warn "安装路径不在 PATH 中: ${INSTALL_DIR}"
        info "执行 ${INSTALL_DIR}/${BINARY_NAME} 启动"
    fi
}

# ── 入口 ──
main() {
    echo ""
    info "tmoes 一键安装 — ${REPO}"
    echo ""

    detect_platform
    fetch_latest
    install_binary
    verify

    echo ""
}
main "$@"
