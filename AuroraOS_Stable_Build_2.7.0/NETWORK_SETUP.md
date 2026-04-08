# AuroraNet VM setup notes

AuroraOS v16 now has a **first e1000 pilot driver path**.
That means:
- AuroraOS can still keep hostname/IP/gateway/DHCP state
- it can detect common PCI network adapters in a VM
- it can now try to start an **Intel e1000 pilot driver** with `netup`
- it can send a small **raw Ethernet test frame** with `netprobe`

## Good adapter choice in VirtualBox
Best target right now:
- **Intel PRO/1000 MT Desktop** (usually maps to `Intel PRO/1000 82540EM`)

Other cards may still be detected, but only e1000 has a pilot driver path in this build.

## Good adapter choices in QEMU
Examples:
- `-net nic,model=e1000`
- `-net nic,model=rtl8139`
- `-net nic,model=pcnet`

Right now, `e1000` is the main target.

## In AuroraOS
Useful commands:
- `network`
- `nic`
- `netup`
- `mac`
- `netprobe`
- `diag`
- `ping 127.0.0.1`

## Honest status
This is **not** a full network stack yet.
Still missing:
- ARP
- IP routing
- ICMP echo packets
- receive path validation
- socket layer

So the current e1000 work should be seen as:
- **real hardware driver groundwork**, and
- a **first raw TX test path**,
not as finished internet support yet.
