#!/usr/bin/env python3
"""Final audit: check both missing sudo AND unnecessary sudo."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

SYSTEM_PATHS = ['/etc/', '/usr/', '/opt/', '/var/', '/boot/', '/root/']
USER_PATHS = ['~/', '$HOME/', '~/.', '${HOME}']

PRIVILEGED_CMDS = ['systemctl ', 'service ', 'dpkg --', 'dpkg-reconfigure',
    'sed -i', 'sed -E -i', 'dd if=', 'chmod ', 'chown ',
    'add-apt-repository', 'apt edit-sources', 'apt-key',
    'fuser /var/lib/dpkg', 'dnf makecache', 'yum makecache']

READ_ONLY = ['systemctl status', 'systemctl is-', 'systemctl list',
    'systemctl show', 'dpkg --print', 'dpkg -l', 'dpkg-query',
    'dpkg --audit', 'dpkg --verify', 'grep ', 'test -f', 'test -x',
    'ls ', 'cat ', 'head ', 'tail ', 'find ', 'which ', 'pgrep ',
    'pkill ', 'apt-cache ', 'apt list', 'apt-mark show',
    'dnf list', 'yum list', 'neofetch', 'lolcat']

INTERACTIVE = ['nano ', 'vi ', 'vim ', '${EDITOR']

issues_missing_sudo = []
issues_unnecessary_sudo = []

for f in SRC.rglob("*.cpp"):
    try:
        text = f.read_text(encoding="utf-8", errors="ignore")
    except:
        continue

    for i, line in enumerate(text.split('\n'), 1):
        stripped = line.strip()
        rel = str(f.relative_to(ROOT))

        m = re.search(r'Executor::(?:passthrough|shell|run)\s*\(\s*"([^"]*)"', stripped)
        if not m:
            continue
        cmd = m.group(1)
        has_sudo = cmd.strip().startswith('sudo ')

        # Check if this cmd operates on user paths WITH sudo (should NOT)
        has_user_path = any(p in cmd for p in USER_PATHS)
        if has_sudo and has_user_path:
            if not any(x in cmd for x in READ_ONLY + INTERACTIVE):
                issues_unnecessary_sudo.append((rel, i, cmd[:100]))

        # Check if this cmd needs sudo but doesn't have it
        if has_sudo:
            continue  # already has sudo, skip

        # Skip read-only and interactive
        if any(x in cmd for x in READ_ONLY + INTERACTIVE):
            continue

        # Skip already-safe operations (just cd, mkdir in tmp, etc.)
        if re.match(r'^(cd |for |mkdir /tmp|rm /tmp)', cmd):
            continue

        # Check for privileged commands or system paths
        has_priv_cmd = any(p in cmd for p in PRIVILEGED_CMDS)
        has_sys_path = any(p in cmd for p in SYSTEM_PATHS)

        if has_priv_cmd or has_sys_path:
            # But also skip if it's a harmless operation on system paths
            if any(x in cmd for x in ['for f in ']):
                continue
            issues_missing_sudo.append((rel, i, cmd[:100]))

# Report
if issues_missing_sudo:
    print(f"=== MISSING SUDO ({len(issues_missing_sudo)}) ===")
    for path, line, cmd in issues_missing_sudo:
        print(f"  {path}:{line}")
        print(f"    {cmd}\n")

if issues_unnecessary_sudo:
    print(f"=== UNNECESSARY SUDO on USER PATHS ({len(issues_unnecessary_sudo)}) ===")
    for path, line, cmd in issues_unnecessary_sudo:
        print(f"  {path}:{line}")
        print(f"    {cmd}\n")

if not issues_missing_sudo and not issues_unnecessary_sudo:
    print("PASS: All permissions correct.")
else:
    total = len(issues_missing_sudo) + len(issues_unnecessary_sudo)
    print(f"TOTAL issues: {total}")
