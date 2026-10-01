**English** · [Tiếng Việt](INSTALL.vi.md) · [中文](INSTALL.zh.md) · [日本語](INSTALL.ja.md)

# Deskhub — Install

Choose your platform below to install a prebuilt release. You do not need to check out
the source code. If you want to build Deskhub yourself, see [`BUILD.md`](BUILD.md).

Desktop installers and the Android APK are on the
[Releases page](https://github.com/manhpham90vn/Deskhub/releases). You can also get the
mobile beta through TestFlight or Google Play.

| Platform | File | How to install |
| --- | --- | --- |
| 🪟 Windows | `deskhub-v*-windows-setup.exe` | Download and install; open DeskHub from Start or Desktop |
| 🍎 macOS | `deskhub-v*-macos.dmg` | Open the dmg and drag Deskhub into Applications |
| 🐧 Ubuntu, Kubuntu, Debian, Mint | `deskhub-v*-amd64.deb` | `sudo apt install ./deskhub-v*-amd64.deb` |
| 🐧 Fedora (Workstation & KDE spin) | `deskhub-v*-x86_64.rpm` | `sudo dnf install ./deskhub-v*-x86_64.rpm` |
| 🐧 openSUSE Tumbleweed | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Arch and other Linux distributions | `deskhub-v*-linux-x86_64` | `chmod +x deskhub-v*-linux-x86_64 && ./deskhub-v*-linux-x86_64` |
| 🤖 Android | `deskhub-v*-android.apk` | Install the apk, or join the Play beta |
| 📱 iOS | — | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

There is also `deskhub-cli` for terminal commands and scripts. On Windows and Linux it
can open a window to display a remote screen (on Linux an X11 window, so a Wayland desktop
needs XWayland). See [Command line](#-command-line).

You can also install through `winget` on Windows, Homebrew on macOS, or the
[apt repository](#-linux) on Ubuntu, Kubuntu, Debian and Mint. Use the package manager's
upgrade command when you want a newer release.

---

## 🪟 Windows

Download `deskhub-v*-windows-setup.exe` and run it. Setup installs DeskHub for your
Windows user, adds it to the Start Menu, and creates a Desktop shortcut by default.
You can launch DeskHub at the end of setup. No account or background service is needed.
The separate `deskhub-v*-windows.exe` file remains available for portable use.

Or let winget install the same setup package and keep it current — `winget upgrade` brings
each new release and `winget uninstall ManhPham.Deskhub` removes it:

```powershell
winget install ManhPham.Deskhub
```

If you installed the earlier portable winget package, run
`winget uninstall ManhPham.Deskhub` once, then install it again. Your settings and keys
in `%USERPROFILE%\.deskhub` are kept.

Two things to expect:

- **Administrator, every time it starts.** Windows shows a UAC prompt on each launch,
  because injecting mouse and keyboard into elevated windows is not possible without it.
  With *Start Deskhub when you log in* turned on, it starts at sign-in as an elevated
  logon task, without the prompt.
- **A Windows Firewall rule**, added by the app itself the first time you share.

Uninstall DeskHub from Windows Settings or with `winget uninstall ManhPham.Deskhub`.
For the portable version, delete its exe. Settings and keys remain in
`%USERPROFILE%\.deskhub` until you remove that folder too, and files other machines sent
you stay in `%USERPROFILE%\Deskhub` (or the folder you picked) until you delete them.

## 🍎 macOS

Deskhub needs macOS 14 Sonoma or later.

Download `deskhub-v*-macos.dmg`, open it, drag the app into *Applications*. The dmg is
signed with a Developer ID and notarized by Apple, so it opens without a Gatekeeper
warning.

Or through Homebrew, which `brew upgrade` then keeps current:

```bash
brew install --cask manhpham90vn/tap/deskhub
```

Hosting a screen needs two macOS permissions, both requested from the **Settings** page
in the app, which also shows their live state and a button straight to the matching
System Settings pane:

| Permission | Needed for |
| --- | --- |
| **Screen Recording** | Capturing this Mac's display |
| **Accessibility** | Letting a viewer move this Mac's mouse and keyboard |

Viewing another machine needs neither. After you turn Screen Recording on in System
Settings, quit and reopen Deskhub — macOS applies it only to a fresh launch, and the
Settings page reminds you.

Two more prompts come from macOS itself: **Local Network** access, which Deskhub needs to
reach the other machine whether it is sharing or connecting, and permission to show
**notifications**, asked the first time there is a connection request to announce.

## 🐧 Linux

**To connect and view, start with the app package.** The H.264 decoder is included, so
you do not need to install an FFmpeg package separately. Your distribution must also
provide the desktop libraries the app uses, including GTK3, PipeWire and libva.

The deb and the rpm carry identical content — pick the one your package manager
understands. Both ship the `/dev/uinput` udev rule described in requirement 3 below, so
remote input works right after install, with no group change and no re-login. Every Linux
build — deb, rpm and portable — needs x86_64 with glibc 2.35+ (Ubuntu 22.04, Debian 12,
Fedora 36, openSUSE Tumbleweed, any current Arch). openSUSE Leap 15.5 ships glibc 2.31 and
cannot run it.

The CLI has separate deb and rpm packages. Install `deskhub-cli` only if you need the
terminal commands; it can be installed on its own, or alongside a desktop app of the same
version or newer — upgrade both together, since the package manager will not keep an
older `deskhub` next to a newer `deskhub-cli`. Both packages place their commands in
`/usr/bin`, and the desktop app adds a launcher to the application menu.

On Ubuntu, Kubuntu, Debian and Mint, the Deskhub apt repository installs the same deb and
lets `sudo apt upgrade` bring every later release. It serves Ubuntu 22.04 and newer, and
Debian 12 and newer:

```bash
sudo install -d /etc/apt/keyrings
curl -fsSL https://manhpham90vn.github.io/Deskhub/apt/deskhub.gpg | sudo tee /etc/apt/keyrings/deskhub.gpg >/dev/null
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/deskhub.gpg] https://manhpham90vn.github.io/Deskhub/apt stable main" \
  | sudo tee /etc/apt/sources.list.d/deskhub.list
sudo apt update && sudo apt install deskhub
```

Install the CLI separately with `sudo apt install deskhub-cli`. If upgrading from an
older `deskhub` package that included the CLI, install `deskhub-cli` to keep that command.

**To share this machine's screen**, three more things must be in place.

### 1. A screen-capture portal

Deskhub always captures through `xdg-desktop-portal` — it is what shows the "which screen
to share?" dialog. Your choice there is remembered, so the dialog appears only the first
time you share. When you tick a screen on the Host page that the remembered choice does
not cover, Deskhub forgets it and the dialog comes back on its own. To drop the choice by
hand, run `deskhub-cli displays --forget`.

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

### 2. A hardware H.264 encoder

H.264 is encoded on the GPU; there is no software fallback. Which encoder a host uses
depends on the GPU that draws the desktop:

- **NVIDIA with the proprietary driver** — NVENC, through the driver's own libraries, as
  long as the driver supports NVENC API 13.0 or newer. There is nothing more to install,
  and `vainfo` may list no H.264 encoder on such a machine without stopping it from
  hosting. `nvidia-vaapi-driver` and `libva-nvidia-driver` only decode: they speed up
  viewing, not sharing.
- **AMD, Intel and everything else** — VA-API, which needs a driver with an H.264 encoder:

```bash
# Ubuntu / Debian / Mint
sudo apt install va-driver-all vainfo        # NVIDIA, decoding only: nvidia-vaapi-driver

# Fedora — stock Mesa has H.264 disabled; the working drivers live in RPM Fusion:
sudo dnf install libva-utils
sudo dnf install mesa-va-drivers-freeworld   # AMD (RPM Fusion)
sudo dnf install intel-media-driver          # Intel (RPM Fusion)
sudo dnf install nvidia-vaapi-driver         # NVIDIA, decoding only (RPM Fusion)

# openSUSE
sudo zypper install libva-utils              # plus your GPU vendor's VA-API driver

# Arch
sudo pacman -S libva-utils
sudo pacman -S libva-mesa-driver             # AMD · Intel: intel-media-driver · NVIDIA, decoding only: libva-nvidia-driver

# then, on AMD or Intel:
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
a source checkout the same thing is `make setup-linux-permissions`. Besides the rule, the
script adds the user who ran it with sudo to the `input` group, which takes effect at the
next login: an SSH or headless session needs it to reach `/dev/uinput`, and it also turns
on host wins (below).

Without the uinput grant the app still runs and can still view — it just cannot inject
mouse or keyboard into this machine.

<a id="host-wins"></a>

### Letting the person at the machine win

While you use this machine's own mouse or keyboard, Deskhub pauses remote input ("host
wins"). To notice you, it reads `/dev/input/event*`, which only members of the `input`
group can do — and the deb and the rpm do not add anyone to it. Without it, sharing and
remote input still work, but a viewer's input is not paused while you type. To turn it
on:

```bash
sudo usermod -aG input "$USER"               # then log out and back in
```

Membership in `input` lets every program you run read every keyboard and mouse on the
machine, not only Deskhub, so leave it off where that matters more than host wins.

### Firewall

If you enabled a firewall, open UDP 47777:

```bash
sudo ufw allow 47777/udp                                  # Ubuntu / Debian / Mint
sudo firewall-cmd --add-port=47777/udp --permanent        # Fedora / openSUSE
```

### Uninstall

```bash
sudo apt remove deskhub deskhub-cli   # whichever you installed; or dnf remove / zypper remove
rm -rf ~/.deskhub            # settings, keys, allowed clients and trusted hosts
sudo rm -f /etc/apt/sources.list.d/deskhub.list /etc/apt/keyrings/deskhub.gpg   # the apt repository, if you added it
```

Files other machines sent you stay in `~/Deskhub` (or the folder you picked) until you
delete them. The portable binary is a single file — delete it. `setup-uinput.sh` leaves
its udev rule, its module-load file and your `input` group membership behind; remove them
by hand if nothing else needs them:

```bash
sudo rm -f /etc/udev/rules.d/60-deskhub-uinput.rules /etc/modules-load.d/deskhub.conf
sudo gpasswd -d "$USER" input
```

## 🤖 Android

Deskhub needs **Android 8.0** or later. Hosting is a view-only screen share and needs
**Android 10+**.

**Direct apk** — download `deskhub-v*-android.apk` from
[Releases](https://github.com/manhpham90vn/Deskhub/releases) and install it. If Android
refuses to install it over a copy from Google Play, or the other way round, uninstall the
other copy first — that removes its key, so hosts have to let the device in again.

**Play beta** — three steps, all with the **same Google account** as your phone's Play
Store:

1. Join the tester group: [groups.google.com/g/deskhub-test](https://groups.google.com/g/deskhub-test)
2. Become a tester: [play.google.com/apps/testing/com.manhpham.deskhub](https://play.google.com/apps/testing/com.manhpham.deskhub)
3. Install (give Play a few minutes to sync): [play.google.com/store/apps/details?id=com.manhpham.deskhub](https://play.google.com/store/apps/details?id=com.manhpham.deskhub)

Please keep the beta installed **14+ days** — Google requires that before the app can go
public. The Play beta can trail the apk on Releases: each release reaches Google Play's
internal track first and the beta only when it is promoted.

## 📱 iOS

Deskhub needs iOS or iPadOS 17 or later. An ipa cannot be sideloaded, so the beta runs
through TestFlight:

1. Install [TestFlight](https://apps.apple.com/app/testflight/id899247664).
2. Join the beta: **[testflight.apple.com/join/7qY7wgpd](https://testflight.apple.com/join/7qY7wgpd)**

Like Android, an iPhone or iPad hosts view-only — no mobile OS lets an app inject input
into the device it runs on.

---

## 💻 Command line

`deskhub-cli` provides commands for sharing a screen, opening a remote shell and
connecting from a script or over SSH. On Windows and Linux, `connect` opens a window for
the remote screen — an X11 window on Linux, which a Wayland desktop shows through
XWayland; on macOS, use the desktop app to watch a screen. Run `deskhub-cli help`
for the command list. The CLI and app use the same settings, machine key, allowed clients,
connection requests and trusted hosts.

| Platform | File |
| --- | --- |
| 🪟 Windows | `deskhub-cli-v*-windows-setup.exe` — installs `deskhub-cli` on your user `PATH` |
| 🍎 macOS | `deskhub-cli-v*-macos` — one binary for Apple Silicon and Intel |
| 🐧 Linux | `deskhub-cli-v*-amd64.deb`, `deskhub-cli-v*-x86_64.rpm`, or portable `deskhub-cli-v*-linux-x86_64` |

For a downloaded Linux package, use `sudo apt install ./deskhub-cli-v*-amd64.deb` on
Ubuntu/Debian or `sudo dnf install ./deskhub-cli-v*-x86_64.rpm` on Fedora. These install
the command in `/usr/bin` without installing the desktop app.

Only the portable macOS and Linux binaries may need `chmod +x` after download; `.deb` and
`.rpm` packages are installed with the package manager. The portable macOS binary is not
signed or notarized like the dmg, so its first run may need
`xattr -d com.apple.quarantine deskhub-cli-v*-macos`, or *Open Anyway* in System Settings
→ Privacy & Security.

The Windows setup package adds the CLI to your user `PATH` without requiring an administrator.
The separate `deskhub-cli-v*-windows.exe` remains available for portable use.
Every installer and package carries `THIRD_PARTY_NOTICES.md` and the licence texts of the
libraries inside it; the portable binaries cannot, so each release also offers
`deskhub-v*-licenses.zip` with `LICENSE`, `THIRD_PARTY_NOTICES.md` and every licence text.
Package managers also put the command on `PATH`:
`winget install ManhPham.DeskhubCLI` on Windows (winget installs the portable exe, not the
setup package), `brew install manhpham90vn/tap/deskhub-cli` on macOS — Homebrew installs it
without the quarantine flag. On Ubuntu or Debian, run
`sudo apt install deskhub-cli` after adding the DeskHub apt repository above.

On Windows, open a new PowerShell window after the winget install and run
`deskhub-cli help`. To check which executable Windows finds, run
`Get-Command deskhub-cli`. The portable exe does not add itself to `PATH`.

On Linux it shares a screen through the same portal and VA-API driver the app needs, and
relies on the same `/dev/uinput` rule for remote input, so everything under
[Linux](#-linux) applies to it too. On macOS it shares a screen and opens shells, but it
cannot *watch* one — `connect` needs a window layer the command-line build does not have,
and says so; use the app for that.

---

## 🔒 Before you share a screen

Everything a session carries — video, keystrokes, mouse, clipboard and terminal traffic —
runs over **QUIC/TLS**, and access works like SSH: a host lets in only the devices whose
key it lists, and every device has one key. There are three ways to put a device on that
list, and you only need one of them:

- **Scan the QR code.** While the host is sharing, press **Show QR code** beside its
  address list. On the phone, press **Scan QR code** on the Client page and point it at
  the screen; on any other device, copy the link under the code and paste it into the
  address field. The device is trusted, allowed and connected in one step.
- **Approve the request.** On the device, type the host's address and press *Connect*.
  The host shows a *Connection request* notification and lists it under **Connection
  requests** on its Host and Devices pages, with its name, key fingerprint and address;
  press **Approve** and the device connects on its next try —
  it keeps trying for two minutes on its own.
- **Paste the key.** On the device, **Devices** → **Copy public key**; on the host,
  **Devices** → **Devices allowed to connect to this machine** → *Allow*, paste, done.

Connecting by address the first time shows a **New host** dialog with the host's key
fingerprint: compare it with the one under **This machine** on the host's Devices page, then
press *Trust and connect*. Scanning a QR code in the app skips that dialog, because the
code carries the fingerprint. From then on the host is under **Trusted hosts**, and stays
trusted even when its address changes.

[Keys and access](#-keys-and-access) walks through each way in the app and the CLI,
along with scripts and revoking.

There is no passcode and no switch that lets unknown machines in: nobody connects without
your *Approve*, a QR code you showed, or a key you pasted. Deskhub never scans the network.

Use Deskhub on a **network you trust** or through a **VPN**. Do not port-forward UDP
47777. Encryption protects session contents, but the first connection to a host trusts
the key it is shown unless you compare the fingerprint. For remote
access, you can install [Tailscale](https://tailscale.com) on both machines and connect
to the `100.x.y.z` address.

[`SECURITY.md`](../SECURITY.md) has the full threat model, what is and isn't protected,
and how to report a vulnerability.

## 🔑 Keys and access

Deskhub signs in with key pairs, the way SSH does. Every machine has **one key**, created
the first time Deskhub runs, and that key is the machine whether it is sharing or
connecting: the fingerprint you compare when you connect to it and the public key a host
stores when it lets it in are the same key. A host lets in only the keys it lists, and a
client connects only to hosts whose key it trusts. All of it lives on the **Devices**
page, as four sections: **This machine**, **Connection requests**, **Devices allowed to connect
to this machine** and **Trusted hosts**.
Each step below also has a `deskhub-cli` command; the app and the CLI read the same
files, so a change made in one shows up in the other.

### Your key

**Devices** → **This machine** shows this device's name and the key's fingerprint, `SHA256:…`, with a *Copy* button — that is what someone connecting to
you compares against. **Copy public key** beside it (CLI: `deskhub-cli key public`) copies
one line such as `ecdsa-sha2-nistp256 AAAA… laptop`; the label at the end is this device's
name, set under **Settings** → *General* → **Device name**, so a host's owner can tell whose
key it is. That line is what you hand to the owner of a host when neither the QR code nor a
request is convenient — it is safe to share. The private half never leaves the machine,
there is nothing to generate or import, and the key is never replaced on its own. If you
delete `host_key.pem`, the machine gets a new identity: hosts have to let it in again,
and devices that trusted it see it as a new host.

### Letting a device in

The host's owner decides, in one of three ways. Whichever you use, the device then appears
under **Devices** → **Devices allowed to connect to this machine**, labelled with its name; *Remove* beside it takes it out again, and *Remove every
client* empties the list after asking you to confirm.

**Connecting with a QR code** — the quickest, and the only one that needs no fingerprint
comparison. While the host is sharing, press **Show QR code** beside its address list. The
code carries the host's addresses and port, its key fingerprint, its name and a random
one-time token that lasts five minutes.

- On a phone or tablet, open the Client page and press **Scan QR code**. The first time,
  the system asks for camera permission; Deskhub uses the camera only here, decodes the
  frames on the device and stores nothing. Opening a `deskhub://pair/…` link with the
  system camera or from a message works too, but because such a link can come from
  anywhere, the app first shows the **New host** dialog with the fingerprint from the
  link when that host is not trusted yet, and connects only once you compare it and press
  *Trust and connect*; a host you already trust connects straight away.
- On any device, including a desktop, copy the link shown under the code and paste it
  into the address field, then press *Connect* (CLI: `deskhub-cli connect 'deskhub://pair/…'`;
  `sources`, `shell` and `send` take the link too).

The device checks that the machine answering holds the key printed in the code — if a
different machine answers at that address, it stops and says the QR code is not from that
machine — then trusts the host, sends the token, and is let in and connected in one go.
Each code works once; press **Hide QR code**, or stop sharing, and it stops working even
if nobody used it. A device that is already allowed can scan the code just to pick up the
host's current address.

**Approving a connection request** — when the device is not in the same room.

1. On the device, type the host's address (`192.168.1.10`, or `192.168.1.10:PORT` when
   the host does not use 47777) and press *Connect*; confirm the **New host** dialog as
   described below. The page then says it is waiting for the host's owner to approve, and
   keeps trying on its own for two minutes. *Cancel* stops it.
2. On the host, the Host page lists the device under **Connection requests** — its name,
   the start of its fingerprint and its address, with **Approve** and **Deny**. Check that
   the fingerprint matches the one on the device's Devices page and that the address is
   where you expect the device to be, then press *Approve*. The device connects on its next
   try. *Deny* drops the request; the device is only told that nobody approved it in time.
3. A request stays for ten minutes and a host keeps at most sixteen. If you approve after
   the device gave up, it only has to press *Connect* once more.

From the command line: `deskhub-cli access requests` (add `--json` for scripts) lists what
is waiting, `deskhub-cli access approve --fingerprint SHA256:…` lets one in and
`deskhub-cli access deny --fingerprint SHA256:…` drops it. A running `deskhub-cli share`
prints each new request as it arrives, with the approve command to paste into another
terminal.

**Pasting the key** — when you would rather move the key yourself.

1. Ask the person connecting to press **Copy public key** on their Devices page (see
   [Your key](#your-key)) and send you the line.
2. Open **Devices** → **Devices allowed to connect to this machine**, paste the line and press *Allow*. Only Ed25519 and ECDSA P-256 public
   keys are accepted.

From the command line, pipe the line into `access add`:

```sh
deskhub-cli key public                                         # on the device
deskhub-cli access add --stdin                                 # on the host: paste, then Ctrl-D
deskhub-cli key public | ssh me@host deskhub-cli access add --stdin
deskhub-cli access list                                        # on the host
deskhub-cli access remove --fingerprint SHA256:…               # on the host
```

`deskhub-cli access clear` does what *Remove every client* does.

### Connecting the first time

A QR code does this for you. Connecting by address the first time works like SSH:

1. On the client, type the host's address and press *Connect*.
2. A **New host** dialog shows the host's key fingerprint, `SHA256:…`. If that address
   used to belong to another host you trust, the dialog says so and names it — a
   different key at a known address is a different machine, so be sure you know which one
   is answering before you go on.
3. On the host, open **Devices** → **This machine** (*Copy* beside the fingerprint puts it
   on the clipboard). Compare the two fingerprints over a
   channel you already trust — in person, by phone, over chat you know is theirs.
4. If they match, press *Trust and connect*. If they don't, press *Cancel*.

The host is now listed under **Trusted hosts** with its name, fingerprint and the last
address it answered at. Trust follows the key, not the address: when the host gets a new
address, type it and connect — no dialog, no new approval — and the list remembers the new
address for next time.

**Pinning a host in advance.** With the CLI you can save the host key before the first
connection, so no dialog is needed:

```sh
deskhub-cli host-key public                                    # on the host
deskhub-cli host add office --address 192.168.1.10 --host-key-stdin
```

Paste the host's line into the second command (on the client), then Ctrl-D. `office` is an
alias of your choosing: `connect office`, `sources office`, `shell office` and
`send office FILE` all accept it. `deskhub-cli host list` shows every saved host, including
the ones trusted from the app — those get an alias built from their address, such as
`192-168-1-10-47777`. `host update ALIAS` changes the address (`--address`) or the pinned
host key (`--host-key-stdin`); `host remove ALIAS` forgets the host, and so does
`deskhub-cli trust forget` given its fingerprint, alias or last address.

### Scripts and the command line

- **Unknown hosts are refused.** `sources`, `connect`, `shell` and `send` will not talk to
  a host whose key is not saved. They print its fingerprint instead; compare it with the
  host, then rerun with `--accept-new-host-key` to save it. Given an invite link instead of
  an address, they pin the host from the link and need no flag.
- **Waiting for approval.** A host that has not allowed this machine yet is asked to;
  `connect`, `shell`, `send` and `sources` print one line saying so and wait up to two
  minutes for its owner to press *Approve*, then fail with the reason if nobody did.
- **Showing the code.** `deskhub-cli share --qr` prints the QR code in block characters
  together with the invite link, for a phone to scan off the terminal.
- **A separate configuration.** `--config-dir DIR` (before or after the command) or the
  `DESKHUB_CONFIG_DIR` environment variable points the CLI at another directory holding
  the key, allowed clients, requests, trusted hosts and settings — handy for a service
  account or a test setup.
- **Removed in 8.0.** `key generate`, `key import`, `key delete`, `key list`, `key public
  --name`, `devices identities`, `--identity` and `host add --identity` are gone: there is
  one key per machine. Exit code `5` ("the host key changed") is gone too, since a
  different key is now an unknown host.
- **Exit codes.** A script can tell a refused connection from other failures by the exit
  code; the codes are listed under
  [The command-line client](BUILD.md#the-command-line-client) in `BUILD.md`. Listing
  commands take `--json`.

### Revoking a device

On the host, press *Remove* beside its key, or run
`deskhub-cli access remove --fingerprint SHA256:…`. Any session that device has open ends
at once, and it cannot connect again until you let it in again — by approving its next
request, showing it the QR code, or pasting its key.

**When a different key answers at a known address.** That happens when the host
reinstalled Deskhub or lost its settings folder — or when a different machine has taken
that address. Deskhub does not refuse; it treats the machine as one you have never met
and shows the **New host** dialog with a warning naming the host that used to answer
there. Trust it only if you know why. The old host stays under **Trusted hosts** until you
*Remove* it (or `deskhub-cli host remove ALIAS`); with the CLI,
`host update ALIAS --host-key-stdin` re-pins an alias to the new key directly.

### Coming from an older Deskhub

**From 8.0.0.** 9.0 speaks a newer authentication version than 8.0.0, so an
8.0.0 Deskhub on either side cannot connect and is refused with "That machine uses an
incompatible authentication version". Update both machines; nothing else changes — keys,
allowed devices and trusted hosts all carry over.

**From 7.0.x.** Both machines need 9.0 — a 7.0.x Deskhub on either side cannot connect and
is refused with "That machine uses an incompatible authentication version". Each machine
keeps the key and fingerprint it already had, so hosts you trusted stay trusted. What
changes is the key a device signs in *with*: it is now that same machine key, so every
device has to be let in once more — one *Approve*, one scan of the QR code, or one paste of
its key. The `client_key*.pem` and `host_cert.pem` files 7.0.x wrote are ignored; nothing
in them is read or migrated, and you can delete them.

**From 6.x or older.** Nothing is migrated. Passcodes and the list of paired devices are
not carried over: let each device in and trust each host again, as described above.

## 🆘 If something doesn't work

- **Nothing to connect to** — both machines must be on the same network (or the same
  Tailscale tailnet), and UDP 47777 must be open on the host.
- **"Waiting for the owner of … to approve this device"** — the host does not list this
  device yet; its owner needs to press *Approve* under **Connection requests** on their
  Host page, show you the QR code, or paste your key — see
  [Letting a device in](#letting-a-device-in). Devices allowed by 7.0.x must be let in
  again.
- **"The owner of that machine did not approve this device in time"** — two minutes
  passed without an *Approve*. The request stays on the host for ten minutes: ask, then
  press *Connect* again.
- **"The machine that answered is not the one that made this QR code"** — something else
  is answering at the address in the code. Show the code again on the host and scan it
  once more; if it keeps happening, check which machine has that address.
- **A New host dialog for a host you already trust** — a different key is answering at
  that address; see [Revoking a device](#revoking-a-device).
- **"That machine uses an incompatible authentication version"** — one side runs an older
  Deskhub; update both machines. See [Coming from an older Deskhub](#coming-from-an-older-deskhub).
- **Linux: sharing fails immediately** — on AMD or Intel, run
  `vainfo | grep -E 'H264.*Enc'`; an empty result means this machine has no usable H.264
  encoder and cannot host. On NVIDIA, the log in `~/.deskhub` says whether the driver's
  NVENC is too old.
- **Linux: a viewer keeps control while you type** — your user is not in the `input`
  group; see [Letting the person at the machine win](#host-wins).
- **Linux: the pointer doesn't move** — the `/dev/uinput` rule from requirement 3 is
  missing.
- **macOS: a black screen or dead input** — check Screen Recording and Accessibility on
  the Settings page.
- **Still stuck** — open an
  [issue](https://github.com/manhpham90vn/Deskhub/issues) and include your device model,
  OS version and what the status line on the Host or Client page says.
