#include "domain/runtime.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include <filesystem>
#include <cstdlib>
#include <regex>

#include "container.h"

namespace fs = std::filesystem;

namespace tmoe::domain {
    // ── Helpers ──

    std::string QemuRuntime::detect_qemu_bin() const {
        std::string base = config_.qemu_bin_path;
        if (base.empty()) {
            base = "/usr/bin";
        }
        if (!fs::exists(base)) {
            const char *prefix = std::getenv("PREFIX");
            base = prefix ? std::string(prefix) + "/bin" : "/usr/bin";
        }

        std::string arch = config_.qemu_bin_arch;
        if (arch == "i386") {
            return base + "/qemu-system-i386";
        }
        return base + "/qemu-system-x86_64";
    }

    int QemuRuntime::get_auto_memory_size(int reduced, int max_mem) {
        auto r = Executor::shell("free -m | awk '{print $NF}' | sed -n 2p");
        if (!r.ok()) return 1024;
        std::string free_str = r.stdout_data;
        free_str.erase(std::remove(free_str.begin(), free_str.end(), '\n'), free_str.end());
        try {
            int free_mem = std::stoi(free_str);
            int allocated = free_mem - reduced;
            if (allocated <= 256) return 256;
            if (allocated >= max_mem) return max_mem;
            if (allocated < 0) return 256;
            return allocated;
        } catch (...) {
            return 1024;
        }
    }

    // ── Config apply helpers (对应 Bash 的 set -- 参数构建) ──

    void QemuRuntime::apply_memory_config(const QemuConfig &cfg,
                                          std::vector<std::string> &args) const {
        int mem_size = cfg.memory_size;
        if (cfg.memory_allocation == "auto") {
            mem_size = get_auto_memory_size(cfg.memory_reduced, cfg.max_memory);
        }
        if (mem_size <= 0) mem_size = 1024;
        if (mem_size > cfg.max_memory) mem_size = cfg.max_memory;

        args.emplace_back("-m");
        args.emplace_back(std::to_string(mem_size));
    }

    void QemuRuntime::apply_storage_config(const QemuConfig &cfg,
                                           std::vector<std::string> &args) const {
        // VirtIO 磁盘 01
        if (cfg.virtio_disk[0].enabled && !cfg.virtio_disk[0].path.empty()) {
            std::string node = "libvirt-1-storage";
            std::string node_fmt = "libvirt-1-format";
            args.emplace_back("-blockdev");
            args.emplace_back("driver=file,filename=" + cfg.virtio_disk[0].path +
                              ",node-name=" + node + ",auto-read-only=on,discard=unmap");
            args.emplace_back("-blockdev");
            args.emplace_back("node-name=" + node_fmt + ",read-only=off,driver=" +
                              cfg.virtio_disk[0].format + ",file=" + node);
            std::string attach = "virtio-blk-pci,bus=pci.5,addr=0x0,drive=" + node_fmt + ",id=virtio-disk1";
            if (!cfg.virtio_disk[0].bootindex.empty()) {
                attach += ",bootindex=" + cfg.virtio_disk[0].bootindex;
            }
            args.emplace_back("-device");
            args.emplace_back(attach);
        }

        // VirtIO 磁盘 02
        if (cfg.virtio_disk[1].enabled && !cfg.virtio_disk[1].path.empty()) {
            std::string node = "libvirt-2-storage";
            std::string node_fmt = "libvirt-2-format";
            args.emplace_back("-blockdev");
            args.emplace_back("driver=file,filename=" + cfg.virtio_disk[1].path +
                              ",node-name=" + node + ",auto-read-only=on,discard=unmap");
            args.emplace_back("-blockdev");
            args.emplace_back("node-name=" + node_fmt + ",read-only=off,driver=" +
                              cfg.virtio_disk[1].format + ",file=" + node);
            std::string attach = "virtio-blk-pci,bus=pci.6,addr=0x0,drive=" + node_fmt + ",id=virtio-disk2";
            if (!cfg.virtio_disk[1].bootindex.empty()) {
                attach += ",bootindex=" + cfg.virtio_disk[1].bootindex;
            }
            args.emplace_back("-device");
            args.emplace_back(attach);
        }

        // CD-ROM 01
        if (cfg.cdrom_disk[0].enabled && !cfg.cdrom_disk[0].path.empty()) {
            std::string node = "libvirt-3-storage";
            std::string node_fmt = "libvirt-3-format";
            args.emplace_back("-blockdev");
            args.emplace_back("driver=file,filename=" + cfg.cdrom_disk[0].path +
                              ",node-name=" + node + ",auto-read-only=on,discard=unmap");
            args.emplace_back("-blockdev");
            args.emplace_back("node-name=" + node_fmt + ",read-only=on,driver=" +
                              cfg.cdrom_disk[0].format + ",file=" + node);
            std::string attach = "ide-cd,bus=ide.1,unit=0,drive=" + node_fmt + ",id=ide0-1-0";
            if (!cfg.cdrom_disk[0].bootindex.empty()) {
                attach += ",bootindex=" + cfg.cdrom_disk[0].bootindex;
            }
            args.emplace_back("-device");
            args.emplace_back(attach);
        }

        // CD-ROM 02
        if (cfg.cdrom_disk[1].enabled && !cfg.cdrom_disk[1].path.empty()) {
            std::string node = "libvirt-4-storage";
            std::string node_fmt = "libvirt-4-format";
            args.emplace_back("-blockdev");
            args.emplace_back("driver=file,filename=" + cfg.cdrom_disk[1].path +
                              ",node-name=" + node + ",auto-read-only=on,discard=unmap");
            args.emplace_back("-blockdev");
            args.emplace_back("node-name=" + node_fmt + ",read-only=on,driver=" +
                              cfg.cdrom_disk[1].format + ",file=" + node);
            std::string attach = "ide-cd,bus=ide.1,unit=1,drive=" + node_fmt + ",id=sata0-1-1";
            if (!cfg.cdrom_disk[1].bootindex.empty()) {
                attach += ",bootindex=" + cfg.cdrom_disk[1].bootindex;
            }
            args.emplace_back("-device");
            args.emplace_back(attach);
        }

        // SATA 磁盘 01
        if (cfg.sata_disk[0].enabled && !cfg.sata_disk[0].path.empty()) {
            std::string node = "libvirt-5-storage";
            std::string node_fmt = "libvirt-5-format";
            args.emplace_back("-blockdev");
            args.emplace_back("driver=file,filename=" + cfg.sata_disk[0].path +
                              ",node-name=" + node + ",auto-read-only=on,discard=unmap");
            args.emplace_back("-blockdev");
            args.emplace_back("node-name=" + node_fmt + ",read-only=off,driver=" +
                              cfg.sata_disk[0].format + ",file=" + node);
            std::string attach = "ide-hd,bus=ide.0,unit=0,drive=" + node_fmt + ",id=ide0-0-0";
            if (!cfg.sata_disk[0].bootindex.empty()) {
                attach += ",bootindex=" + cfg.sata_disk[0].bootindex;
            }
            args.emplace_back("-device");
            args.emplace_back(attach);
        }

        // USB 磁盘 01
        if (cfg.usb_disk[0].enabled && !cfg.usb_disk[0].path.empty()) {
            std::string node = "libvirt-7-storage";
            std::string node_fmt = "libvirt-7-format";
            args.emplace_back("-blockdev");
            args.emplace_back("driver=file,filename=" + cfg.usb_disk[0].path +
                              ",node-name=" + node + ",auto-read-only=on,discard=unmap");
            args.emplace_back("-blockdev");
            args.emplace_back("node-name=" + node_fmt + ",read-only=off,driver=" +
                              cfg.usb_disk[0].format + ",file=" + node);
            std::string attach = "usb-storage,bus=usb.0,port=4,drive=" + node_fmt +
                                 ",id=usb-disk1,removable=on";
            if (!cfg.usb_disk[0].bootindex.empty()) {
                attach += ",bootindex=" + cfg.usb_disk[0].bootindex;
            }
            args.emplace_back("-device");
            args.emplace_back(attach);
        }

        // Floppy 01
        if (cfg.floppy_disk[0].enabled && !cfg.floppy_disk[0].path.empty()) {
            args.emplace_back("-device");
            args.emplace_back("isa-fdc"); // Floppy controller
            std::string node = "libvirt-9-storage";
            std::string node_fmt = "libvirt-9-format";
            args.emplace_back("-blockdev");
            args.emplace_back("driver=file,filename=" + cfg.floppy_disk[0].path +
                              ",node-name=" + node + ",auto-read-only=on,discard=unmap");
            args.emplace_back("-blockdev");
            args.emplace_back("node-name=" + node_fmt + ",read-only=off,driver=" +
                              cfg.floppy_disk[0].format + ",file=" + node);
            std::string attach = "floppy,unit=0,drive=" + node_fmt + ",id=fdc0-0-0";
            if (!cfg.floppy_disk[0].bootindex.empty()) {
                attach += ",bootindex=" + cfg.floppy_disk[0].bootindex;
            }
            args.emplace_back("-device");
            args.emplace_back(attach);
        }

        // VirtIO 9p PCI 共享目录
        if (cfg.virtio_9p_enabled) {
            std::string share_path = cfg.virtio_9p_path;
            if (share_path.empty()) {
                // 自动检测
                const char *home = std::getenv("HOME");
                std::string home_s = home ? home : "";
                for (const auto &p: {
                         home_s, home_s + "/sd", std::string("/sd"),
                         std::string("/sdcard"), std::string("/media/docker"),
                         std::string("/media/sd")
                     }) {
                    if (!p.empty() && fs::exists(p)) {
                        share_path = p;
                        break;
                    }
                }
            }
            if (!share_path.empty()) {
                args.emplace_back("-fsdev");
                args.emplace_back("local,security_model=none,id=fsdev-fs0,path=" + share_path);
                args.emplace_back("-device");
                args.emplace_back("virtio-9p-pci,id=fs0,fsdev=fsdev-fs0,mount_tag=virtio9p01,bus=pci.0,addr=0x1d");
            }
        }
    }

    void QemuRuntime::apply_network_config(const QemuConfig &cfg,
                                           std::vector<std::string> &args) const {
        // 网卡 01
        if (cfg.net_devices[0].enabled) {
            std::string fwd;
            if (cfg.net_devices[0].tcp_fwd_enabled && !cfg.net_devices[0].tcp_fwd.empty()) {
                // 解析 "host_port:guest_port,..." 格式
                std::string s = cfg.net_devices[0].tcp_fwd;
                size_t pos = 0;
                while (pos < s.length()) {
                    size_t comma = s.find(',', pos);
                    std::string rule = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                    size_t colon = rule.find(':');
                    if (colon != std::string::npos) {
                        fwd += ",hostfwd=tcp::" + rule.substr(0, colon) + "-" + rule.substr(colon + 1);
                    }
                    if (comma == std::string::npos) break;
                    pos = comma + 1;
                }
            }
            if (cfg.net_devices[0].udp_fwd_enabled && !cfg.net_devices[0].udp_fwd.empty()) {
                std::string s = cfg.net_devices[0].udp_fwd;
                size_t pos = 0;
                while (pos < s.length()) {
                    size_t comma = s.find(',', pos);
                    std::string rule = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                    size_t colon = rule.find(':');
                    if (colon != std::string::npos) {
                        fwd += ",hostfwd=udp::" + rule.substr(0, colon) + "-" + rule.substr(colon + 1);
                    }
                    if (comma == std::string::npos) break;
                    pos = comma + 1;
                }
            }

            args.emplace_back("-netdev");
            args.emplace_back("user,id=hostnet0" + fwd);
            std::string mac_arg;
            if (!cfg.net_devices[0].mac.empty()) {
                mac_arg = ",mac=" + cfg.net_devices[0].mac;
            }
            args.emplace_back("-device");
            args.emplace_back(cfg.net_devices[0].model + ",netdev=hostnet0,id=net0,bus=pci.0,addr=0x3" + mac_arg);
        }

        // 网卡 02
        if (cfg.net_devices[1].enabled) {
            std::string fwd;
            if (cfg.net_devices[1].tcp_fwd_enabled && !cfg.net_devices[1].tcp_fwd.empty()) {
                std::string s = cfg.net_devices[1].tcp_fwd;
                size_t pos = 0;
                while (pos < s.length()) {
                    size_t comma = s.find(',', pos);
                    std::string rule = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                    size_t colon = rule.find(':');
                    if (colon != std::string::npos) {
                        fwd += ",hostfwd=tcp::" + rule.substr(0, colon) + "-" + rule.substr(colon + 1);
                    }
                    if (comma == std::string::npos) break;
                    pos = comma + 1;
                }
            }

            args.emplace_back("-netdev");
            args.emplace_back("user,id=hostnet1" + fwd);
            std::string mac_arg;
            if (!cfg.net_devices[1].mac.empty()) {
                mac_arg = ",mac=" + cfg.net_devices[1].mac;
            }
            args.emplace_back("-device");
            args.emplace_back(cfg.net_devices[1].model + ",netdev=hostnet1,id=net1,bus=pci.0,addr=0xa" + mac_arg);
        }

        // 网卡 03
        if (cfg.net_devices[2].enabled) {
            args.emplace_back("-netdev");
            args.emplace_back("user,id=hostnet2");
            args.emplace_back("-device");
            args.emplace_back(cfg.net_devices[2].model + ",netdev=hostnet2,id=net2,bus=pci.0,addr=0xc");
        }
    }

    void QemuRuntime::apply_gpu_config(const QemuConfig &cfg,
                                       std::vector<std::string> &args) const {
        std::string gpu_arg;
        if (cfg.gpu_model.find("virtio") != std::string::npos) {
            gpu_arg = "virtio-vga,id=video0,max_outputs=1,bus=pci.0,addr=0x2";
            if (cfg.virtio_3d_accel) {
                gpu_arg += ",virgl=on";
            }
        } else if (cfg.gpu_model == "qxl-vga" || cfg.gpu_model == "qxl*") {
            gpu_arg = "qxl-vga,id=video0,ram_size=" + cfg.gpu_ram_size +
                      ",vram_size=" + cfg.gpu_vram_size + ",vram64_size_mb=0,vgamem_mb=" +
                      std::to_string(cfg.gpu_vgamem_mb) + ",max_outputs=1,bus=pci.0,addr=0x2";
        } else if (cfg.gpu_model == "VGA" || cfg.gpu_model == "vga") {
            gpu_arg = "VGA,id=video0,vgamem_mb=" + std::to_string(cfg.gpu_vgamem_mb) +
                      ",bus=pci.0,addr=0x2";
        } else if (cfg.gpu_model.find("bochs") != std::string::npos) {
            gpu_arg = "bochs-display,id=video0,vgamem=16384k,bus=pci.0,addr=0x2";
        } else {
            gpu_arg = cfg.gpu_model + ",id=video0,bus=pci.0,addr=0x2";
        }
        args.emplace_back("-device");
        args.emplace_back(gpu_arg);
    }

    void QemuRuntime::apply_sound_config(const QemuConfig &cfg,
                                         std::vector<std::string> &args) const {
        if (!cfg.sound_card_enabled) return;

        // 第一声卡
        if (cfg.sound_card == "intel-hda" || cfg.sound_card == "hda") {
            args.emplace_back("-device");
            args.emplace_back("intel-hda,id=sound0,bus=pci.0,addr=0x4");
            args.emplace_back("-device");
            args.emplace_back("hda-duplex,id=sound0-codec0,bus=sound0.0,cad=0");
        } else if (cfg.sound_card == "ich9-intel-hda") {
            args.emplace_back("-device");
            args.emplace_back("ich9-intel-hda,id=sound0,bus=pci.0,addr=0x4");
            args.emplace_back("-device");
            args.emplace_back("hda-duplex,id=sound0-codec0,bus=sound0.0,cad=0");
        } else if (cfg.sound_card == "AC97" || cfg.sound_card == "ac97") {
            args.emplace_back("-device");
            args.emplace_back("AC97,id=sound0,bus=pci.0,addr=0x4");
        } else if (cfg.sound_card == "ES1370" || cfg.sound_card == "es1370") {
            args.emplace_back("-device");
            args.emplace_back("ES1370,id=sound0,bus=pci.0,addr=0x4");
        }

        // 第二声卡
        if (cfg.sound_card_02_enabled) {
            if (cfg.sound_card_02 == "intel-hda" || cfg.sound_card_02 == "hda") {
                args.emplace_back("-device");
                args.emplace_back("intel-hda,id=sound1,bus=pci.0,addr=0xb");
                args.emplace_back("-device");
                args.emplace_back("hda-duplex,id=sound1-codec0,bus=sound1.0,cad=0");
            } else if (cfg.sound_card_02 == "AC97" || cfg.sound_card_02 == "ac97") {
                args.emplace_back("-device");
                args.emplace_back("AC97,id=sound1,bus=pci.0,addr=0xb");
            } else if (cfg.sound_card_02 == "ES1370" || cfg.sound_card_02 == "es1370") {
                args.emplace_back("-device");
                args.emplace_back("ES1370,id=sound1,bus=pci.0,addr=0xb");
            }
        }

        // PulseAudio
        if (!cfg.audio_addr.empty()) {
            if (cfg.connection_type != "x11" && cfg.connection_type != "wayland") {
                // VNC/SPICE 模式设置 PULSE_SERVER
                // 在 QEMU 中通过环境变量传递
            }
        }
    }

    void QemuRuntime::apply_vnc_config(const QemuConfig &cfg,
                                       std::vector<std::string> &args) const {
        if (cfg.connection_type == "vnc") {
            int true_vnc_port = cfg.vnc_port - 5900;
            std::string vnc_addr = cfg.vnc_localhost ? "127.0.0.1" : "0.0.0.0";
            std::string vnc_arg = vnc_addr + ":" + std::to_string(true_vnc_port) + ",to=5000";
            if (cfg.vnc_password) {
                vnc_arg += ",password=on";
            }
            args.emplace_back("-vnc");
            args.emplace_back(vnc_arg);

            if (cfg.vnc_startup_prompt) {
                Logger::step(_("qemu.vnc_started"));
                Logger::step(_f("qemu.vnc_local", std::to_string(cfg.vnc_port)));
                if (!cfg.vnc_localhost) {
                    auto r = Executor::shell(
                        "ip -4 -br -c a | awk '{print $NF}' | cut -d '/' -f 1 | grep -v '127.0.0.1'");
                    if (r.ok() && !r.stdout_data.empty()) {
                        std::string ip = r.stdout_data;
                        ip.erase(std::remove(ip.begin(), ip.end(), '\n'), ip.end());
                        Logger::step(_f("qemu.vnc_lan", ip + ":" + std::to_string(cfg.vnc_port)));
                    }
                }
            }
        } else if (cfg.connection_type == "spice") {
            std::string spice_addr = cfg.vnc_localhost ? "127.0.0.1" : "0.0.0.0";
            std::string spice_arg = "port=" + std::to_string(cfg.vnc_port) + ",addr=" + spice_addr +
                                    ",image-compression=off,seamless-migration=on";
            if (cfg.vnc_password) {
                spice_arg += ",password";
            } else {
                spice_arg += ",disable-ticketing";
            }
            args.emplace_back("-spice");
            args.emplace_back(spice_arg);

            if (cfg.vnc_startup_prompt) {
                Logger::step(_("qemu.spice_started"));
                Logger::step(_f("qemu.spice_local", std::to_string(cfg.vnc_port)));
            }
        } else if (cfg.connection_type == "remote-x11") {
            std::string disp = cfg.remote_x11_addr;
            if (!disp.empty()) {
                // 注：Bash 原版在此处执行 export DISPLAY=...，
                //    但 C++ 每次 Executor::shell 调用独立 shell 进程，此 export 无效。
                //   如需设置环境变量，请使用 C++ setenv("DISPLAY", disp.c_str(), 1)。
            }
        }
    }

    // ── 命令生成 (对应 Bash 的 run_tmoe_qemu_command) ──

    void QemuRuntime::build_qemu_args(const QemuConfig &cfg, const Container &container,
                                      const LaunchContext *ctx, std::vector<std::string> &args) const {
        // 1. 机器类型
        std::string machine_arg;
        if (cfg.machine_type == "q35") {
            machine_arg = "q35,accel=" + cfg.machine_accel + ",usb=off,vmport=off,dump-guest-core=off";
            if (cfg.uefi_enabled) {
                machine_arg += ",pflash0=libvirt-pflash0-format,pflash1=libvirt-pflash1-format";
            }
            args.emplace_back("-machine");
            args.emplace_back(machine_arg);

            // UEFI
            if (cfg.uefi_enabled) {
                args.emplace_back("-blockdev");
                args.emplace_back("driver=file,filename=" + cfg.uefi_code_pflash +
                                  ",node-name=libvirt-pflash0-storage,auto-read-only=on,discard=unmap");
                args.emplace_back("-blockdev");
                args.emplace_back(
                    "node-name=libvirt-pflash0-format,read-only=on,driver=raw,file=libvirt-pflash0-storage");
                if (cfg.uefi_vars_pflash.empty()) {
                    std::string uefi_vars = std::getenv("HOME") ? std::string(std::getenv("HOME")) : "";
                    uefi_vars += "/.config/tmoe-linux/qemu/" + cfg.qemu_name + "-NVRAM.fd";
                    const_cast<QemuConfig &>(cfg).uefi_vars_pflash = uefi_vars;
                }
                args.emplace_back("-blockdev");
                args.emplace_back("driver=file,filename=" + cfg.uefi_vars_pflash +
                                  ",node-name=libvirt-pflash1-storage,auto-read-only=on,discard=unmap");
                args.emplace_back("-blockdev");
                args.emplace_back(
                    "node-name=libvirt-pflash1-format,read-only=off,driver=raw,file=libvirt-pflash1-storage");
            }

            // q35 芯片组全局设置
            args.emplace_back("-global");
            args.emplace_back("ICH9-LPC.disable_s3=1");
            args.emplace_back("-global");
            args.emplace_back("ICH9-LPC.disable_s4=1");

            // USB 控制器
            args.emplace_back("-device");
            args.emplace_back("qemu-xhci,p2=15,p3=15,id=usb,bus=pci.2,addr=0x0");
            args.emplace_back("-device");
            args.emplace_back("virtio-serial-pci,id=virtio-serial0,bus=pci.4,addr=0x0");
            args.emplace_back("-device");
            args.emplace_back("usb-tablet,id=input0,bus=usb.0,port=3");
        } else {
            // pc (i440fx)
            machine_arg = "pc,accel=" + cfg.machine_accel + ",usb=off,vmport=off,dump-guest-core=off";
            args.emplace_back("-machine");
            args.emplace_back(machine_arg);
            args.emplace_back("-global");
            args.emplace_back("PIIX4_PM.disable_s3=1");
            args.emplace_back("-global");
            args.emplace_back("PIIX4_PM.disable_s4=1");
            args.emplace_back("-device");
            args.emplace_back("ich9-usb-ehci1,id=usb,bus=pci.0,addr=0x5.0x7");
            args.emplace_back("-device");
            args.emplace_back("ich9-usb-uhci1,masterbus=usb.0,firstport=0,bus=pci.0,multifunction=on,addr=0x5");
            args.emplace_back("-device");
            args.emplace_back("ich9-usb-uhci2,masterbus=usb.0,firstport=2,bus=pci.0,addr=0x5.0x1");
            args.emplace_back("-device");
            args.emplace_back("ich9-usb-uhci3,masterbus=usb.0,firstport=4,bus=pci.0,addr=0x5.0x2");
            args.emplace_back("-device");
            args.emplace_back("virtio-serial-pci,id=virtio-serial0,bus=pci.0,addr=0x6");
        }

        // 2. KVM
        if (cfg.enable_kvm) {
            args.emplace_back("-global");
            args.emplace_back("kvm-pit.lost_tick_policy=delay");
        }

        // 3. CPU
        args.emplace_back("-cpu");
        args.emplace_back(cfg.cpu_model + "," + cfg.cpu_id_flags);

        // 4. SMP
        args.emplace_back("-smp");
        args.emplace_back(std::to_string(cfg.cpus) + ",sockets=" + std::to_string(cfg.sockets) +
                          ",cores=" + std::to_string(cfg.cores) + ",threads=" + std::to_string(cfg.threads));

        // 5. 内存
        apply_memory_config(cfg, args);

        // 6. 基本参数
        args.emplace_back("-monitor");
        args.emplace_back("stdio");
        args.emplace_back("-no-user-config");
        args.emplace_back("-nodefaults");
        args.emplace_back("-rtc");
        args.emplace_back("base=" + cfg.rtc_base + ",driftfix=slew");
        args.emplace_back("-no-hpet");
        args.emplace_back("-name");
        args.emplace_back("guest=" + cfg.qemu_name + ",debug-threads=on");

        // 7. 启动顺序
        if (cfg.boot_order.empty()) {
            args.emplace_back("-boot");
            args.emplace_back("menu=" + std::string(cfg.boot_menu ? "on" : "off") + ",strict=off");
        } else {
            args.emplace_back("-boot");
            args.emplace_back(
                "order=" + cfg.boot_order + ",menu=" + std::string(cfg.boot_menu ? "on" : "off") + ",strict=off");
        }

        // 8. 存储设备
        apply_storage_config(cfg, args);

        // 9. 声卡
        apply_sound_config(cfg, args);

        // 10. 网络
        apply_network_config(cfg, args);

        // 11. GPU
        apply_gpu_config(cfg, args);

        // 12. 自定义内核
        if (cfg.custom_kernel) {
            if (!cfg.kernel_file.empty()) {
                args.emplace_back("-kernel");
                args.emplace_back(cfg.kernel_file);
            }
            if (!cfg.initrd_file.empty()) {
                args.emplace_back("-initrd");
                args.emplace_back(cfg.initrd_file);
            }
            if (!cfg.kernel_append.empty()) {
                args.emplace_back("-append");
                args.emplace_back(cfg.kernel_append);
            }
        }

        // 13. Balloon 驱动
        args.emplace_back("-device");
        args.emplace_back("virtio-balloon-pci,id=balloon0,bus=pci.0,addr=0x7");

        // 14. Sandbox
        args.emplace_back("-sandbox");
        args.emplace_back("on,obsolete=deny,elevateprivileges=deny,spawn=deny,resourcecontrol=deny");

        // 15. 消息时间戳
        args.emplace_back("-msg");
        args.emplace_back("timestamp=on");

        // 16. 键盘
        args.emplace_back("-k");
        args.emplace_back("en-us");

        // 17. VNC/SPICE
        apply_vnc_config(cfg, args);

        // 18. PulseAudio
        if (!cfg.audio_addr.empty()) {
            if (cfg.connection_type != "x11" && cfg.connection_type != "wayland") {
                // 注：Bash 原版在此处执行 export PULSE_SERVER=...，
                //    但 C++ 每次 Executor::shell 调用独立 shell 进程，此 export 无效。
                //   如需设置环境变量，请使用 C++ setenv("PULSE_SERVER", cfg.audio_addr.c_str(), 1)。
            } else if (cfg.wayland_remote_pulse) {
                // 注：同上，export PULSE_SERVER 在独立 shell 进程中无效。
                //   如需设置环境变量，请使用 C++ setenv("PULSE_SERVER", cfg.audio_addr.c_str(), 1)。
            }
        }
    }

    std::string QemuRuntime::generate_launch_cmd(const Container &container,
                                                 const LaunchContext *ctx) const {
        std::vector<std::string> args;
        build_qemu_args(config_, container, ctx, args);

        std::string cmd = detect_qemu_bin() + " ";
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) cmd += " ";
            cmd += args[i];
        }
        return cmd;
    }

    // ── start/stop ──

    bool QemuRuntime::start(const Container &container, const LaunchContext *ctx) {
        Logger::step(_f("qemu.starting", container.name()));

        // 检查 QEMU 二进制文件
        std::string qemu_bin = detect_qemu_bin();
        if (!fs::exists(qemu_bin)) {
            Logger::error(_f("qemu.bin_not_found", qemu_bin));
            Logger::error(_("qemu.install_hint"));
            return false;
        }

        // UEFI 固件检查
        if (config_.uefi_enabled) {
            if (!fs::exists(config_.uefi_code_pflash)) {
                Logger::warn(_f("qemu.uefi_not_found", config_.uefi_code_pflash));
                Logger::warn(_("qemu.uefi_install_hint"));
            }
            // 创建 VARS 文件
            if (!config_.uefi_vars_pflash.empty() && !fs::exists(config_.uefi_vars_pflash)) {
                std::string vars_dir = config_.uefi_vars_pflash.substr(0, config_.uefi_vars_pflash.find_last_of('/'));
                Executor::shell("mkdir -pv " + vars_dir);
                if (fs::exists("/usr/share/OVMF/OVMF_VARS.fd")) {
                    Executor::shell("cp -f /usr/share/OVMF/OVMF_VARS.fd " + config_.uefi_vars_pflash);
                    Executor::shell("chmod a+rw " + config_.uefi_vars_pflash);
                }
            }
        }

        std::string cmd = generate_launch_cmd(container, ctx);
        Logger::step(_f("qemu.launch_cmd", cmd));

        // 注：Bash 原版在此处执行 unset LD_PRELOAD，
        //    但 C++ 每次 Executor::shell 调用独立 shell 进程，无需 unset。
        //   如需清除环境变量，请使用 C++ unsetenv("LD_PRELOAD")。

        return Executor::passthrough(cmd).ok();
    }

    bool QemuRuntime::stop(const Container &container) {
        Logger::step(_f("qemu.stopping", container.name()));
        // QEMU 可以通过 monitor 停止，或直接 kill
        std::string cmd = "pkill -f 'qemu-system.*" + container.name() + "'";
        Executor::shell(cmd);
        return true;
    }
} // namespace tmoe::domain
