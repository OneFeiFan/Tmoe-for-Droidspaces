#pragma once
#include <string>

namespace tmoe::domain::gui_config {
  // ═══════════════════════════════════════════════════════
  // 静态配置模板 — 从 gui.cpp 提取, 避免硬编码字符串
  // ═══════════════════════════════════════════════════════

  inline const char *XFCE_PANEL_XML = R"(<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfce4-panel" version="1.0">
  <property name="configver" type="int" value="2"/>
  <property name="panels" type="array">
    <value type="int" value="1"/>
    <property name="panel-1" type="empty">
      <property name="position" type="string" value="p=8;x=0;y=0"/>
      <property name="length" type="uint" value="100"/>
      <property name="position-locked" type="bool" value="true"/>
      <property name="size" type="uint" value="38"/>
      <property name="plugin-ids" type="array">
        <value type="int" value="1"/>
        <value type="int" value="2"/>
        <value type="int" value="3"/>
        <value type="int" value="4"/>
        <value type="int" value="5"/>
        <value type="int" value="6"/>
        <value type="int" value="7"/>
        <value type="int" value="8"/>
        <value type="int" value="9"/>
      </property>
    </property>
  </property>
  <property name="plugins" type="empty">
    <property name="plugin-1" type="string" value="applicationsmenu"/>
    <property name="plugin-2" type="string" value="tasklist"/>
    <property name="plugin-3" type="string" value="separator"/>
    <property name="plugin-4" type="string" value="clock">
      <property name="digital-layout" type="uint" value="2"/>
      <property name="mode" type="uint" value="2"/>
    </property>
    <property name="plugin-5" type="string" value="separator"/>
    <property name="plugin-6" type="string" value="systray"/>
    <property name="plugin-7" type="string" value="pulseaudio"/>
    <property name="plugin-8" type="string" value="notification-plugin"/>
    <property name="plugin-9" type="string" value="actions"/>
  </property>
</channel>
)";

  inline const char *XFCE_DESKTOP_XML = R"(<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfce4-desktop" version="1.0">
  <property name="backdrop" type="empty">
    <property name="screen0" type="empty">
      <property name="monitor0" type="empty">
        <property name="image-path" type="string" value="/usr/share/backgrounds/xfce/xfce-stripes.png"/>
        <property name="last-image" type="string" value="/usr/share/backgrounds/xfce/xfce-stripes.png"/>
        <property name="image-style" type="int" value="5"/>
      </property>
      <property name="monitor1" type="empty">
        <property name="image-path" type="string" value="/usr/share/backgrounds/xfce/xfce-stripes.png"/>
        <property name="last-image" type="string" value="/usr/share/backgrounds/xfce/xfce-stripes.png"/>
        <property name="image-style" type="int" value="5"/>
      </property>
      <property name="monitorVNC-0" type="empty">
        <property name="image-path" type="string" value="/usr/share/backgrounds/xfce/xfce-stripes.png"/>
        <property name="last-image" type="string" value="/usr/share/backgrounds/xfce/xfce-stripes.png"/>
        <property name="image-style" type="int" value="5"/>
      </property>
      <property name="monitorrdp0" type="empty">
        <property name="image-path" type="string" value="/usr/share/backgrounds/xfce/xfce-stripes.png"/>
        <property name="last-image" type="string" value="/usr/share/backgrounds/xfce/xfce-stripes.png"/>
        <property name="image-style" type="int" value="5"/>
      </property>
      <property name="monitorscreen" type="empty">
        <property name="image-path" type="string" value="/usr/share/backgrounds/xfce/xfce-stripes.png"/>
        <property name="last-image" type="string" value="/usr/share/backgrounds/xfce/xfce-stripes.png"/>
        <property name="image-style" type="int" value="5"/>
      </property>
    </property>
  </property>
</channel>
)";

  inline const char *XFCE_TERMINAL_RC = R"([Configuration]
ColorForeground=#e6e1cf
ColorBackground=#0f1419
ColorCursor=#f29718
ColorPalette=#000000;#ff3333;#b8cc52;#e7c547;#36a3d9;#f07178;#95e6cb;#ffffff;#323232;#ff6565;#eafe84;#fff779;#68d5ff;#ffa3aa;#c7fffd;#ffffff
MiscAlwaysShowTabs=FALSE
MiscBell=FALSE
MiscBellUrgent=FALSE
MiscBordersDefault=TRUE
MiscCursorBlinks=FALSE
MiscCursorShape=TERMINAL_CURSOR_SHAPE_BLOCK
MiscDefaultGeometry=80x24
MiscInheritGeometry=FALSE
MiscMenubarDefault=TRUE
MiscMouseAutohide=FALSE
MiscMouseWheelZoom=TRUE
MiscToolbarDefault=TRUE
MiscConfirmClose=TRUE
MiscCycleTabs=TRUE
MiscTabCloseButtons=TRUE
MiscTabCloseMiddleClick=TRUE
MiscTabPosition=GTK_POS_TOP
MiscHighlightUrls=TRUE
MiscMiddleClickOpensUri=FALSE
MiscCopyOnSelect=FALSE
MiscShowRelaunchDialog=TRUE
MiscRewrapOnResize=TRUE
MiscUseShiftArrowsToScroll=FALSE
MiscSlimTabs=FALSE
MiscNewTabAdjacent=FALSE
BackgroundMode=TERMINAL_BACKGROUND_TRANSPARENT
BackgroundDarkness=0.730000
ScrollingUnlimited=TRUE
)";

  inline const char *GNOME_SHELL_X11 = "#!/usr/bin/env sh\n# tmoe gnome-shell-x11\n\nexec gnome-shell --x11 \"$@\"\n";

  inline const char *GNOME_FLASHBACK_METACITY =
      "#!/usr/bin/env sh\n# tmoe gnome-flashback-metacity\n\n"
      "export XDG_CURRENT_DESKTOP=GNOME-Flashback:GNOME\n"
      "export GNOME_SHELL_SESSION_MODE=flashback\n"
      "exec gnome-session --session=gnome-flashback-metacity --disable-acceleration-check \"$@\"\n";

  inline const char *GNOME_SESSION_CLASSIC =
      "#!/usr/bin/env sh\n# tmoe gnome-session-classic\n\n"
      "export GNOME_SHELL_SESSION_MODE=classic\n"
      "exec gnome-session --session=gnome-classic --disable-acceleration-check \"$@\"\n";

  inline const char *GNOME_SESSION_UBUNTU =
      "#!/usr/bin/env sh\n# tmoe gnome-session-ubuntu\n\n"
      "if [ -z $XDG_CURRENT_DESKTOP ]; then\n"
      "    export XDG_CURRENT_DESKTOP=\"ubuntu:GNOME\"\n"
      "fi\n\n"
      "export DESKTOP_SESSION=ubuntu\n"
      "export GNOME_SHELL_SESSION_MODE=ubuntu\n"
      "export XDG_CONFIG_DIRS=/etc/xdg/xdg-ubuntu:/etc/xdg\n\n"
      "exec gnome-session --session=ubuntu --disable-acceleration-check \"$@\"\n";

  inline const char *BUDGIE_DESKTOP_BUILTIN =
      "#!/usr/bin/env sh\n# tmoe budgie-desktop-builtin\n"
      "budgie-wm &\n"
      "exec budgie-panel \"$@\"\n";

  inline const char *UPDATE_ICON_CACHES_SCRIPT =
      "#!/bin/sh\n"
      "case \"$1\" in\n"
      "    \"\"|-h|--help)\n"
      "        printf \"%s\\n\" \"Usage: $0 directory [ ... ]\"\n"
      "        exit 1\n"
      "        ;;\n"
      "esac\n\n"
      "for dir in \"$@\"; do\n"
      "    if [ ! -d \"$dir\" ]; then\n"
      "        continue\n"
      "    fi\n"
      "    if [ -f \"$dir\"/index.theme ]; then\n"
      "        if ! gtk-update-icon-cache --force --quiet \"$dir\"; then\n"
      "            printf \"%s\\n\" \"WARNING: icon cache generation failed for $dir\"\n"
      "        fi\n"
      "    else\n"
      "        rm -f \"$dir\"/icon-theme.cache\n"
      "        rmdir -p --ignore-fail-on-non-empty \"$dir\"\n"
      "    fi\n"
      "done\n"
      "exit 0\n";

  inline const char *POLKIT_COLORD_CONF =
      "polkit.addRule(function(action, subject) {\n"
      "if ((action.id == \"org.freedesktop.color-manager.create-device\" || "
      "action.id == \"org.freedesktop.color-manager.create-profile\" || "
      "action.id == \"org.freedesktop.color-manager.delete-device\" || "
      "action.id == \"org.freedesktop.color-manager.delete-profile\" || "
      "action.id == \"org.freedesktop.color-manager.modify-device\" || "
      "action.id == \"org.freedesktop.color-manager.modify-profile\") && "
      "subject.isInGroup(\"{group}\"))\n"
      "{\n"
      "return polkit.Result.YES;\n"
      "}\n"
      "});\n";

  inline const char *POLKIT_COLORD_PKLA =
      "[Allow Colord all Users]\n"
      "Identity=unix-user:*\n"
      "Action=org.freedesktop.color-manager.create-device;"
      "org.freedesktop.color-manager.create-profile;"
      "org.freedesktop.color-manager.delete-device;"
      "org.freedesktop.color-manager.delete-profile;"
      "org.freedesktop.color-manager.modify-device;"
      "org.freedesktop.color-manager.modify-profile\n"
      "ResultAny=no\n"
      "ResultInactive=no\n"
      "ResultActive=yes\n\n"
      "[Allow Package Management all Users]\n"
      "Identity=unix-user:*\n"
      "Action=org.debian.apt.*;io.snapcraft.*;org.freedesktop.packagekit.*;"
      "com.ubuntu.update-notifier.*\n"
      "ResultAny=no\n"
      "ResultInactive=no\n"
      "ResultActive=yes\n";
} // namespace tmoe::domain::gui_config
