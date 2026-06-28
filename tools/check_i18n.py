#!/usr/bin/env python3
"""静态扫描源码中 _(...) 和 _f(...) 的 key，与 JSON 语言包双向比对。

用法:
    python tools/check_i18n.py              # 比对 zh_CN + en_US
    python tools/check_i18n.py --lang zh_CN  # 仅比对指定语言
    python tools/check_i18n.py --fix         # 交互式补全缺失 key（带空翻译占位）
"""
import json, os, re, sys, argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "src"
LOCALES_DIR = ROOT / "locales"

# 匹配 _("key") 和 _f("key", ...) — 单行场景
RE_DIRECT = re.compile(r'\b_[f]?\s*\(\s*"((?:[^"\\]|\\.)*)"')

# 多行 _("...") — 跨行字符串拼接（C++ 自动拼接相邻字面量）
# 匹配: _("key1" "key2") 这种拼接形式（少见但也覆盖）
RE_MULTI = re.compile(r'_\s*\(\s*"((?:[^"\\]|\\.)*)"\s*"((?:[^"\\]|\\.)*)"')


def extract_keys_from_file(filepath: Path) -> set[str]:
    """从单个源文件中提取所有 i18n key。"""
    keys = set()
    try:
        text = filepath.read_text(encoding="utf-8", errors="ignore")
    except Exception:
        return keys

    # 单行匹配
    for m in RE_DIRECT.finditer(text):
        key = m.group(1)
        # 过滤明显不是 i18n key 的字符串（如格式化占位符、纯路径）
        if key and not key.startswith("/") and not key.startswith("{"):
            keys.add(key)

    # 多行字符串拼接 _("line1" "line2")
    for m in RE_MULTI.finditer(text):
        key = m.group(1) + m.group(2)
        if key:
            keys.add(key)

    return keys


def extract_all_code_keys(src_dir: Path) -> set[str]:
    """扫描 src/ 下所有 .cpp/.h/.hpp/.c/.cc 文件。"""
    all_keys = set()
    extensions = {".cpp", ".h", ".hpp", ".c", ".cc"}
    for f in src_dir.rglob("*"):
        if f.suffix in extensions:
            all_keys |= extract_keys_from_file(f)
    return all_keys


def load_json_keys(json_path: Path) -> set[str]:
    """加载 JSON 语言包中的所有 key。"""
    if not json_path.exists():
        print(f"  WARNING: file not found: {json_path}")
        return set()
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return set(data.keys())


def report(missing: set[str], unused: set[str], lang: str):
    """输出报告。"""
    if missing:
        print(f"\n{'='*60}")
        print(f"[{lang}] 缺失翻译 ({len(missing)} 个) — 代码用了但 JSON 里没有:")
        print(f"{'='*60}")
        for k in sorted(missing):
            print(f'  "{k}": "",')
        print(f"{'='*60}")
        print(f"[{lang}] Copy the above into locales/{lang}.json 并填写翻译\n")

    if unused:
        print(f"\n{'='*60}")
        print(f"[{lang}] 未使用翻译 ({len(unused)} 个) — JSON 有但代码没用过:")
        print(f"{'='*60}")
        for k in sorted(unused):
            print(f'  "{k}",')
        print(f"{'='*60}")
        print(f"[{lang}] These keys can be safely removed from locales/{lang}.json 中删除\n")

    if not missing and not unused:
        print(f"[{lang}] All keys translated, no unused keys found.")


def fix_missing(json_path: Path, missing: set[str]):
    """将缺失 key 以空翻译占位追加到 JSON 文件。"""
    if not missing:
        return
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    added = 0
    for k in sorted(missing):
        if k not in data:
            data[k] = ""
            added += 1

    if added == 0:
        return

    backup = json_path.with_suffix(".json.bak")
    import shutil
    shutil.copy2(json_path, backup)

    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write("\n")

    print(f"[{json_path.name}] 已补全 {added} 个缺失 key（备份: {backup.name}），请填写翻译内容。")


def load_json_dict(json_path: Path) -> dict[str, str]:
    """加载 JSON 语言包，返回 {key: value}。"""
    if not json_path.exists():
        return {}
    with open(json_path, "r", encoding="utf-8") as f:
        return json.load(f)


def match_renames(missing: set[str], unused: set[str],
                   en_dict: dict[str, str], zh_dict: dict[str, str]) -> list[tuple[str, str, str]]:
    """尝试匹配"缺失 key"与"未使用 key"的改名对。

    策略: 最后一节相同(非通用词) + 长度>=6 → 高置信
    """
    results = []

    def segments(k: str) -> set[str]:
        parts = k.replace("_", ".").replace("-", ".").split(".")
        return {p for p in parts if len(p) >= 3 and not p.isdigit()}

    def last_seg(k: str) -> str:
        return k.replace("_", ".").split(".")[-1]

    # 排除这些通用结尾，太泛了无法配对
    generic_last = {"prompt", "title", "item", "msg", "info", "warn", "error",
                    "hint", "desc", "header", "footer", "yesno", "input",
                    "btn", "opt", "ok", "msgbox", "select", "back",
                    "failed", "installed", "downloading", "configured", "removed",
                    "started", "stopped", "detected", "enabled", "disabled",
                    "found", "done", "complete", "completed", "setting", "set",
                    "note", "warning", "detail", "status", "result",
                    "install", "remove", "create", "delete", "update", "check",
                    "config", "start", "stop", "enable", "disable"}
    stopwords = {"gui", "menu", "title", "prompt", "item", "select", "msg", "ok", "hint",
                 "info", "warn", "error", "btn", "opt", "desc", "header", "footer", "msgbox"}

    unused_list = sorted(unused)
    for mk in sorted(missing):
        mk_segs = segments(mk)
        mk_last = last_seg(mk)
        for uk in unused_list:
            uk_segs = segments(uk)
            uk_last = last_seg(uk)

            # 规则1: 最后一节相同 + 非通用词 + 长度>=6
            if mk_last == uk_last and len(mk_last) >= 8 and mk_last not in generic_last:
                results.append((mk, uk, "last_seg:" + mk_last))
                continue

    return results


def group_report(missing: set[str], unused: set[str],
                  en_dict: dict[str, str], zh_dict: dict[str, str]):
    """分组报告 + 改名推测。"""
    matches = match_renames(missing, unused, en_dict, zh_dict)

    matched_missing = {m[0] for m in matches}
    matched_unused = {m[1] for m in matches}

    # Pick best match per missing key (prioritize last_seg over shared words)
    best_matches = {}
    for mk, uk, reason in sorted(matches, key=lambda x: (0 if "last_seg" in x[2] else 1)):
        if mk not in best_matches:
            best_matches[mk] = (uk, reason)
    matched_missing = set(best_matches.keys())
    matched_unused = {uk for uk, _ in best_matches.values()}

    if best_matches:
        print(f"\n{'='*60}")
        print(f"Likely renamed: {len(best_matches)} pairs")
        print(f"{'='*60}")
        for mk, (uk, reason) in sorted(best_matches.items()):
            zh_val = zh_dict.get(uk, "")
            print(f"  [{reason}]")
            print(f"    NEW: \"{mk}\"")
            print(f"    OLD: \"{uk}\"")
            if zh_val.strip():
                print(f"    ZH:  {zh_val[:60]}")
            print()

    remaining_missing = missing - matched_missing
    if remaining_missing:
        print(f"\n{'='*60}")
        print(f"Confirmed new: {len(remaining_missing)} keys (need manual translation)")
        print(f"{'='*60}")
        for k in sorted(remaining_missing):
            print(f'  "{k}": "",')

    remaining_unused = unused - matched_unused
    if remaining_unused:
        print(f"\n{'='*60}")
        print(f"Confirmed unused: {len(remaining_unused)} keys (safe to delete)")
        print(f"{'='*60}")
        for k in sorted(remaining_unused):
            print(f'  "{k}",')


def rename_matched_pairs(lang: str, best_matches: dict[str, tuple[str, str]]):
    """在 JSON 中将 matched OLD key 改名为 NEW key，保留翻译值。"""
    json_path = LOCALES_DIR / f"{lang}.json"
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    renamed = 0
    for new_key, (old_key, _) in best_matches.items():
        if old_key in data and new_key not in data:
            data[new_key] = data.pop(old_key)
            renamed += 1

    if renamed == 0:
        return

    backup = json_path.with_suffix(".json.bak")
    import shutil
    shutil.copy2(json_path, backup)

    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write("\n")

    print(f"[{lang}] Renamed {renamed} old keys → new keys (backup: {backup.name})")


def main():
    parser = argparse.ArgumentParser(description="i18n key static checker")
    parser.add_argument("--lang", help="Check specific language only (e.g. zh_CN, en_US)")
    parser.add_argument("--fix", action="store_true", help="Add missing keys as empty placeholders to JSON")
    parser.add_argument("--match", action="store_true", help="Show rename pairs + new + unused report")
    parser.add_argument("--rename", action="store_true", help="Auto-rename matched old JSON keys to new names")
    args = parser.parse_args()

    print("Scanning source code for i18n keys...")
    code_keys = extract_all_code_keys(SRC_DIR)
    print(f"   Found {len(code_keys)} unique keys")

    langs = [args.lang] if args.lang else ["zh_CN", "en_US"]

    en_dict = load_json_dict(LOCALES_DIR / "en_US.json")
    zh_dict = load_json_dict(LOCALES_DIR / "zh_CN.json")

    # Compute rename pairs once
    best_matches = None
    if args.match or args.rename:
        all_missing = set()
        all_unused = set()
        for lang in langs:
            json_keys = load_json_keys(LOCALES_DIR / f"{lang}.json")
            all_missing |= (code_keys - json_keys)
            all_unused |= (json_keys - code_keys)
        matches = match_renames(all_missing, all_unused, en_dict, zh_dict)
        best_matches = {}
        for mk, uk, reason in sorted(matches, key=lambda x: (0 if "last_seg" in x[2] else 1)):
            if mk not in best_matches:
                best_matches[mk] = (uk, reason)

    if args.rename and best_matches:
        for lang in langs:
            rename_matched_pairs(lang, best_matches)

    for lang in langs:
        json_path = LOCALES_DIR / f"{lang}.json"
        print(f"\n>> {lang} ...")
        json_keys = load_json_keys(json_path)
        print(f"   JSON: {len(json_keys)} keys")

        missing = code_keys - json_keys
        unused = json_keys - code_keys

        if args.fix:
            fix_missing(json_path, missing)
        elif args.match and best_matches:
            group_report(missing, unused, en_dict, zh_dict)
        else:
            report(missing, unused, lang)


if __name__ == "__main__":
    main()
