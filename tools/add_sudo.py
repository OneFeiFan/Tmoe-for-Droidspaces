#!/usr/bin/env python3
"""Add sudo to all system-level commands in Executor calls that don't have it."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

# Commands that should always run with sudo when they modify the system
PRIVILEGED = {
    "systemctl ", "service ", "dpkg --", "dpkg-reconfigure",
    "rm -rf /etc", "rm -fv /etc", "rm -v /etc",
    "chmod ",  # only for /etc/, /usr/, /opt/
    "chown ", "chgrp ",
    "sed -i", "sed -E -i",
    "cp -f /etc", "cp -sv /usr", "mv -vf /usr", "mv -f /usr",
    "cp /etc", "cp /usr",
    "wget -O /usr", "curl -o /usr",
    "mkdir -p /etc", "mkdir -pv /etc",
    "dd if=",  # writing to boot/block devices
    "cat >/etc", "tee /etc",
    "update-icon-caches",
    "xrdp",  # service management
    "fuser /var/lib/dpkg",  # needs root to kill locks
    "apt edit-sources",
}

def add_sudo_to_cmd(cmd: str) -> tuple[str, bool]:
    """Add sudo prefix if command needs it and doesn't already have it."""
    # Already has sudo
    if re.match(r'^\s*sudo\s', cmd):
        return cmd, False

    # Check if any privileged command pattern matches
    for pat in PRIVILEGED:
        if pat in cmd:
            # Don't add sudo to read-only operations
            if any(x in cmd for x in ['systemctl status', 'dpkg --print',
                                        'dpkg -l', 'dpkg-query', 'grep ',
                                        'test -f', 'test -x', 'test -d',
                                        'ls ', 'cat ', 'head ', 'tail ',
                                        'pgrep ', 'pkill ', 'find ',
                                        'which ', 'nano ', 'vi ', 'vim ',
                                        '${EDITOR', 'dpkg --audit',
                                        'dpkg --verify', 'apt-cache ',
                                        'apt list', 'apt-mark show',
                                        'dnf list', 'yum list',
                                        'systemctl list', 'systemctl status',
                                        'systemctl is-', 'systemctl show']):
                continue
            return "sudo " + cmd, True
    return cmd, False

modified = 0
for f in SRC.rglob("*.cpp"):
    try:
        content = f.read_text(encoding="utf-8", errors="replace")
    except:
        continue

    new_lines = []
    changed = False

    for line in content.split('\n'):
        # Only modify Executor::passthrough("...") or Executor::shell("...") lines
        m = re.match(r'(\s*)(Executor::(?:passthrough|shell)\s*\(")([^"]*)(".*)', line)
        if m:
            indent, prefix, cmd, suffix = m.groups()
            new_cmd, was_modified = add_sudo_to_cmd(cmd)
            if was_modified:
                new_lines.append(f"{indent}{prefix}{new_cmd}{suffix}")
                modified += 1
                changed = True
                continue

        new_lines.append(line)

    if changed:
        f.write_text('\n'.join(new_lines), encoding="utf-8")

print(f"Modified {modified} commands across all files")
