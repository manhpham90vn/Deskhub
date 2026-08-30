**English** · [Tiếng Việt](INSTALL.vi.md) · [中文](INSTALL.zh.md) · [日本語](INSTALL.ja.md)

# Deskhub — Install

Every platform on one page. Nothing here needs a source checkout — every release is
prebuilt. Compiling it yourself is [`BUILD.md`](BUILD.md).

All downloads live on the
[Releases page](https://github.com/manhpham90vn/Deskhub/releases); the mobile builds are
distributed by TestFlight and Google Play.

| Platform | File | One-line install |
| --- | --- | --- |
| 🪟 Windows | `deskhub-v*-windows.exe` | Download and run it |
| 🍎 macOS | `deskhub-v*-macos.dmg` | Open the dmg, drag the app across |
| 🐧 Ubuntu, Kubuntu, Debian, Mint | `deskhub-v*-amd64.deb` | `sudo apt install ./deskhub-v*-amd64.deb` |
| 🐧 Fedora (Workstation & KDE spin) | `deskhub-v*-x86_64.rpm` | `sudo dnf install ./deskhub-v*-x86_64.rpm` |
| 🐧 openSUSE | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Arch, anything else | `deskhub-v*-linux-x86_64` | `chmod +x deskhub-v*-linux-x86_64 && ./deskhub-v*-linux-x86_64` |
| 🤖 Android | `deskhub-v*-android.apk` | Install the apk, or join the Play beta |
| 📱 iOS | — | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

There is also `deskhub-cli`, the same client with no window of its own — see
[Command line](#-command-line).

---

## 🪟 Windows

Download `deskhub-v*-windows.exe` and run it. There is no installer, no background
service and no account — the whole app is that one file.

Two things happen the first time you use it:

- **Administrator, once at startup.** Injecting mouse and keyboard into elevated windows
  is not possible without it.
- **A Windows Firewall rule**, added by the app itself the first time you share.

To remove Deskhub, delete the exe. Settings and keys stay in `%USERPROFILE%\.deskhub`
until you delete that folder too.

## 🍎 macOS

Download `deskhub-v*-macos.dmg`, open it, drag the app into *Applications*. The dmg is
signed with a Developer ID and notarized by Apple, so it opens without a Gatekeeper
warning.

Hosting a screen needs two macOS permissions, both requested from the **Settings** page
in the app, which also shows their live state and a button straight to the matching
System Settings pane:

| Permission | Needed for |
| --- | --- |
| **Screen Recording** | Capturing this Mac's display |
| **Accessibility** | Letting a viewer move this Mac's mouse and keyboard |

Viewing another machine needs neither.

## 🐧 Linux

**To connect and view, installing is all it takes.** The app links only against GTK3,
PipeWire and libva, which every stock desktop already has; the H.264 decoder is compiled
in, so no FFmpeg package is involved.

The deb and the rpm carry identical content — pick the one your package manager
understands. Both ship the `/dev/uinput` udev rule described in requirement 3 below, so
remote input works right after install, with no group change and no re-login. The
portable binary runs on any x86_64 distro with glibc 2.35+ (Ubuntu 22.04, Fedora 36,
openSUSE 15.5, any current Arch).

The deb and the rpm also install `deskhub-cli` as `/usr/bin/deskhub-cli` — see
[Command line](#-command-line) below.

**To share this machine's screen**, three more things must be in place.

### 1. A screen-capture portal

Deskhub always captures through `xdg-desktop-portal` — it is what shows the "which screen
to share?" dialog. Your choice there is remembered, so the dialog appears only the first
time you share; *Choose screens again* on the Host page brings it back when you want a
different screen.

GNOME and KDE ship their portal backend out of the box on every major distro — **nothing
to do** on Ubuntu, Kubuntu, Fedora Workstation, Fedora KDE, openSUSE or Arch with
GNOME/KDE. Standalone window managers do need one:

```bash
sudo apt install xdg-desktop-portal-wlr      # sway / river / Wayfire on Debian-family
sudo dnf install xdg-desktop-portal-wlr      # …on Fedora
sudo pacman -S xdg-desktop-portal-wlr        # …on Arch
```

sway, river and Wayfire are Wayland compositors built on the **wlroots** library; unlike
GNOME/KDE they ship no portal backend of their own, and `-wlr` is the backend that
implements screen capture for all of them. Hyprland has its own
`xdg-desktop-portal-hyprland`.

### 2. A VA-API driver

H.264 is encoded on the GPU; there is no software fallback.

```bash
# Ubuntu / Debian / Mint
sudo apt install va-driver-all vainfo        # NVIDIA also needs: nvidia-vaapi-driver

# Fedora — stock Mesa has H.264 disabled; the working drivers live in RPM Fusion:
sudo dnf install libva-utils
sudo dnf install mesa-va-drivers-freeworld   # AMD (RPM Fusion)
sudo dnf install intel-media-driver          # Intel (RPM Fusion)
sudo dnf install nvidia-vaapi-driver         # NVIDIA (RPM Fusion)

# openSUSE
sudo zypper install libva-utils              # plus your GPU vendor's VA-API driver

# Arch
sudo pacman -S libva-utils
sudo pacman -S libva-mesa-driver             # AMD · Intel: intel-media-driver · NVIDIA: libva-nvidia-driver

# then on every distro:
vainfo | grep -E 'H264.*Enc'                 # must print ≥1 line, or this machine cannot host
```

### 3. Write access to `/dev/uinput`

This is how mouse and keyboard get injected. The deb and the rpm install the udev rule
for you — nothing to do. On the portable binary, one command sets it up, with no clone
and no re-login on the desktop:

```bash
curl -fsSL https://raw.githubusercontent.com/manhpham90vn/Deskhub/main/scripts/setup-uinput.sh | sudo bash
```

Prefer reading before piping to sudo? Download
[`scripts/setup-uinput.sh`](../scripts/setup-uinput.sh) first — it is a dozen lines. From
a source checkout the same thing is `make setup-linux-permissions`.

Without the uinput grant the app still runs and can still view — it just cannot inject
mouse or keyboard into this machine.

### Firewall

If you enabled a firewall, open UDP 47777:

```bash
sudo ufw allow 47777/udp                                  # Ubuntu / Debian / Mint
sudo firewall-cmd --add-port=47777/udp --permanent        # Fedora / openSUSE
```

### Uninstall

```bash
sudo apt remove deskhub      # or: sudo dnf remove deskhub / sudo zypper remove deskhub
rm -rf ~/.deskhub            # settings, keys and paired machines
```

The portable binary is a single file — delete it.

## 🤖 Android

Hosting is a view-only screen share and needs **Android 10+**. Viewing works on older
releases.

**Direct apk** — download `deskhub-v*-android.apk` from
[Releases](https://github.com/manhpham90vn/Deskhub/releases) and install it. It is signed
with the same key as the Google Play build.

**Play beta** — three steps, all with the **same Google account** as your phone's Play
Store:

1. Join the tester group: [groups.google.com/g/deskhub-test](https://groups.google.com/g/deskhub-test)
2. Become a tester: [play.google.com/apps/testing/com.manhpham.deskhub](https://play.google.com/apps/testing/com.manhpham.deskhub)
3. Install (give Play a few minutes to sync): [play.google.com/store/apps/details?id=com.manhpham.deskhub](https://play.google.com/store/apps/details?id=com.manhpham.deskhub)

Please keep the beta installed **14+ days** — Google requires that before the app can go
public.

## 📱 iOS

An ipa cannot be sideloaded, so the beta runs through TestFlight:

1. Install [TestFlight](https://apps.apple.com/app/testflight/id899247664).
2. Join the beta: **[testflight.apple.com/join/7qY7wgpd](https://testflight.apple.com/join/7qY7wgpd)**

Like Android, an iPhone or iPad hosts view-only — no mobile OS lets an app inject input
into the device it runs on.

---

## 💻 Command line

`deskhub-cli` is the same client without a window of its own: it shares a screen, opens a
remote shell and drives a host from a script or over SSH. Run `deskhub-cli help` for the
list. Everything it reads and writes — settings, paired machines, trusted host keys — is
the same as the app's, so the two agree.

| Platform | File |
| --- | --- |
| 🪟 Windows | `deskhub-cli-v*-windows.exe` — download and run it, no installer |
| 🍎 macOS | `deskhub-cli-v*-macos` — one binary for Apple Silicon and Intel |
| 🐧 Linux | `deskhub-cli-v*-linux-x86_64`, or already there if you installed the deb or the rpm |

The macOS and Linux files arrive without the executable bit, so `chmod +x` them once.
Neither is signed or notarized the way the dmg is: on macOS the first run needs
`xattr -d com.apple.quarantine deskhub-cli-v*-macos`, or *Open Anyway* in System Settings
→ Privacy & Security.

On Linux it shares a screen through the same portal and VA-API driver the app needs, and
relies on the same `/dev/uinput` rule for remote input, so everything under
[Linux](#-linux) applies to it too. On macOS it shares a screen and opens shells, but it
cannot *watch* one — `connect` needs a window layer the command-line build does not have,
and says so; use the app for that.

---

## 🔒 Before you share a screen

Everything a session carries — video, keystrokes, mouse, clipboard and terminal traffic —
runs over **QUIC/TLS**, and an unknown machine only gets in through a pairing handshake:
it must prove it knows the host's passcode via **SPAKE2** (the code itself never travels,
and each connection allows exactly one guess), or wait for the person at the host to
answer *Let this machine in?*.

Encrypted is not the same as Internet-proof. The port still answers discovery probes, and
the first pairing with a machine you have never met is a leap of faith. Prefer a **network
you trust**, or a **VPN** — install [Tailscale](https://tailscale.com) on both machines
and connect to the `100.x.y.z` address. **Never port-forward UDP 47777.**

[`SECURITY.md`](../SECURITY.md) has the full threat model, what is and isn't protected,
and how to report a vulnerability.

## 🆘 If something doesn't work

- **Nothing to connect to** — both machines must be on the same network (or the same
  Tailscale tailnet), and UDP 47777 must be open on the host.
- **Linux: sharing fails immediately** — run `vainfo | grep -E 'H264.*Enc'`; an empty
  result means this machine has no usable H.264 encoder and cannot host.
- **Linux: the pointer doesn't move** — the `/dev/uinput` rule from requirement 3 is
  missing.
- **macOS: a black screen or dead input** — check Screen Recording and Accessibility on
  the Settings page.
- **Still stuck** — open an
  [issue](https://github.com/manhpham90vn/Deskhub/issues) and include your device model,
  OS version and what the status line on the Host or Client page says.
