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
| 🐧 openSUSE | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Arch and other Linux distributions | `deskhub-v*-linux-x86_64` | `chmod +x deskhub-v*-linux-x86_64 && ./deskhub-v*-linux-x86_64` |
| 🤖 Android | `deskhub-v*-android.apk` | Install the apk, or join the Play beta |
| 📱 iOS | — | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

There is also `deskhub-cli` for terminal commands and scripts. On Windows and Linux it
can open a window to display a remote screen. See [Command line](#-command-line).

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

Two things happen the first time you use it:

- **Administrator, once at startup.** Injecting mouse and keyboard into elevated windows
  is not possible without it.
- **A Windows Firewall rule**, added by the app itself the first time you share.

Uninstall DeskHub from Windows Settings or with `winget uninstall ManhPham.Deskhub`.
For the portable version, delete its exe. Settings and keys remain in
`%USERPROFILE%\.deskhub` until you remove that folder too.

## 🍎 macOS

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

Viewing another machine needs neither.

## 🐧 Linux

**To connect and view, start with the app package.** The H.264 decoder is included, so
you do not need to install an FFmpeg package separately. Your distribution must also
provide the desktop libraries the app uses, including GTK3, PipeWire and libva.

The deb and the rpm carry identical content — pick the one your package manager
understands. Both ship the `/dev/uinput` udev rule described in requirement 3 below, so
remote input works right after install, with no group change and no re-login. The
portable binary runs on any x86_64 distro with glibc 2.35+ (Ubuntu 22.04, Fedora 36,
openSUSE 15.5, any current Arch).

The CLI has separate deb and rpm packages. Install `deskhub-cli` only if you need the
terminal commands; it can be installed alongside the desktop app or on its own. Both
packages place their commands in `/usr/bin`, and the desktop app adds a launcher to the
application menu.

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
rm -rf ~/.deskhub            # settings, keys, allowed clients and trusted hosts
sudo rm -f /etc/apt/sources.list.d/deskhub.list /etc/apt/keyrings/deskhub.gpg   # the apt repository, if you added it
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

`deskhub-cli` provides commands for sharing a screen, opening a remote shell and
connecting from a script or over SSH. On Windows and Linux, `connect` opens a window for
the remote screen; on macOS, use the desktop app to watch a screen. Run `deskhub-cli help`
for the command list. The CLI and app use the same settings, client keys, allowed clients
and trusted hosts.

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
Package managers also put the command on `PATH`:
`winget install ManhPham.DeskhubCLI` on Windows, `brew install manhpham90vn/tap/deskhub-cli`
on macOS — Homebrew installs it without the quarantine flag. On Ubuntu or Debian, run
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
runs over **QUIC/TLS**, and access works like SSH. To let a device connect:

1. On the device that will connect, open **Devices** → *When this machine is the client*
   → **My keys** and press *Copy public key* (CLI: `deskhub-cli key public --name default`).
2. On the host, open **Devices** → *When this machine is the host* → **Clients allowed to
   connect to this machine**, press *Allow* and paste that key (CLI:
   `deskhub-cli access add --stdin`).
3. Connect by address. The first time, a **New host** dialog shows the host's key
   fingerprint: compare it with **This machine's host key** on the host's Devices page,
   then press *Trust and connect*. From then on the host is under **Trusted hosts**.

[Keys and access](#-keys-and-access) walks through each step in the app and the CLI,
along with extra keys, scripts, revoking and rotating.

Nothing is approved over the network and Deskhub never scans it. If a trusted host's key
ever changes, the connection is refused outright; remove the host from *Trusted hosts*
only if you know why its key changed, then trust it again.

Use Deskhub on a **network you trust** or through a **VPN**. Do not port-forward UDP
47777. Encryption protects session contents, but the first connection to a host trusts
the key it is shown unless you compare the fingerprint. For remote
access, you can install [Tailscale](https://tailscale.com) on both machines and connect
to the `100.x.y.z` address.

[`SECURITY.md`](../SECURITY.md) has the full threat model, what is and isn't protected,
and how to report a vulnerability.

## 🔑 Keys and access

Deskhub signs in with key pairs, the way SSH does. Every device that connects has a
**client key**, and every host has a **host key**. A host lets in only the client keys it
lists, and a client connects only to hosts whose key it trusts. All of it lives on the
**Devices** page, split into *When this machine is the host* and *When this machine is
the client*. Each step below also has a `deskhub-cli` command; the app and the CLI read the
same files, so a change made in one shows up in the other.

### Your keys

Your keys are under **Devices** → *When this machine is the client* → **My keys**. The
private half never leaves the machine.

- **The `default` key.** The app creates a key called `default` the first time you open
  the Devices page, and the CLI creates it the first time it connects anywhere or prints
  it with `deskhub-cli key public --name default`. Most people never need another one.
- **More keys.** Press *New key* and give it a name (CLI:
  `deskhub-cli key generate --name NAME`). Use a separate key when you want to revoke one
  host's access without touching the others.
- **An existing key.** Press *Import key…*, pick the private key file, give it a name and,
  if the file is encrypted, its passphrase (CLI:
  `deskhub-cli key import --name NAME --file PATH`, adding `--passphrase-stdin` to read the
  passphrase from standard input). Deskhub reads OpenSSH (`ssh-keygen`) and PKCS#8 files
  holding an Ed25519 or ECDSA P-256 key; RSA is not supported. The passphrase is used only
  to unlock the file during the import — it is not stored and never sent to a host.
- **Your public key.** Press *Copy public key* beside a key (CLI:
  `deskhub-cli key public --name NAME`). You get one line such as
  `ssh-ed25519 AAAA… laptop`; the label at the end is this device's name, set under
  **Settings** → *General* → **Device name**, so the host owner can tell whose key it is.
  This line is what you send to the owner of a host — it is safe to share.
- **Deleting a key.** Press *Delete* beside it. The `default` key cannot be deleted, and
  neither can a key a trusted host still uses — point that host at another key first. A
  deleted private key cannot be recovered. From the CLI: `deskhub-cli key list` and
  `deskhub-cli key delete --name NAME`, with the same rules.

### Letting a device in

On the host:

1. Ask the person connecting for their public key line (see [Your keys](#your-keys)).
2. Open **Devices** → *When this machine is the host* → **Clients allowed to connect to
   this machine**, paste the line and press *Allow*. Only Ed25519 and ECDSA P-256 public
   keys are accepted.
3. The key now appears in the list with its label. *Remove* beside a key takes it out;
   *Remove every client* empties the list after asking you to confirm.

From the command line, pipe the line into `access add`:

```sh
deskhub-cli key public --name default                          # on the client
deskhub-cli access add --stdin                                 # on the host: paste, then Ctrl-D
deskhub-cli key public --name default | ssh me@host deskhub-cli access add --stdin
deskhub-cli access list                                        # on the host
deskhub-cli access remove --fingerprint SHA256:…               # on the host
```

`deskhub-cli access clear` does what *Remove every client* does.

### Connecting the first time

1. On the client, type the host's address (`192.168.1.10`, or `192.168.1.10:PORT` when
   the host does not use 47777) and press *Connect*.
2. A **New host** dialog shows the host-key fingerprint, `SHA256:…`.
3. On the host, open **Devices** → *When this machine is the host* → **This machine's
   host key** (*Copy* puts it on the clipboard). Compare the two fingerprints over a
   channel you already trust — in person, by phone, over chat you know is theirs.
4. If they match, press *Trust and connect*. If they don't, press *Cancel*.

The host is now listed under **Trusted hosts**, with the client key it uses, and later
connections skip the dialog.

**Pinning a host in advance.** With the CLI you can save the host key before the first
connection, so no dialog is needed:

```sh
deskhub-cli host-key public                                    # on the host
deskhub-cli host add office --address 192.168.1.10 --identity default --host-key-stdin
```

Paste the host's line into the second command (on the client), then Ctrl-D. `office` is an
alias of your choosing: `connect office`, `sources office`, `shell office` and
`send office FILE` all accept it. `deskhub-cli host list` shows every saved host, including
the ones trusted from the app — those get an alias built from their address, such as
`192-168-1-10-47777`. `host update ALIAS` changes the address (`--address`), the client
key (`--identity`) or the pinned host key (`--host-key-stdin`); `host remove ALIAS` forgets
the host. The app connects to a new host with `default`; to use another key for a host,
set it here with `--identity` — the app then uses it too.

### Scripts and the command line

- **Unknown hosts are refused.** `sources`, `connect`, `shell` and `send` will not talk to
  a host whose key is not saved. They print its fingerprint instead; compare it with the
  host, then rerun with `--accept-new-host-key` to save it. That flag only saves a key seen
  for the first time — a host whose key *changed* is always refused.
- **Choosing a key.** `--identity NAME` picks the client key for one command. Without it
  the saved host's key is used, and `default` for a host that has none.
- **A separate configuration.** `--config-dir DIR` (before or after the command) or the
  `DESKHUB_CONFIG_DIR` environment variable points the CLI at another directory of keys,
  allowed clients, trusted hosts and settings — handy for a service account or a test
  setup.
- **Exit codes.** A script can tell a refused connection or a changed host key from other
  failures by the exit code; the codes are listed under
  [The command-line client](BUILD.md#the-command-line-client) in `BUILD.md`. Listing
  commands take `--json`.

### Revoking and rotating keys

**Revoking a device.** On the host, press *Remove* beside its key, or run
`deskhub-cli access remove --fingerprint SHA256:…`. Any session that device has open ends
at once, and it cannot connect again until its key is allowed again.

**Rotating a client key.**

1. Create a new key: *New key*, or `deskhub-cli key generate --name NAME`.
2. Have each host that should accept it allow its public key.
3. Make the trusted host use it: `deskhub-cli host update ALIAS --identity NAME`, or
   remove the host and add it again with `host add … --identity NAME`. Connect once to
   check.
4. Ask the host owner to remove the old key, then *Delete* the old key here.

**When a host's key changes.** Deskhub refuses to connect and says
"This host's key has changed". That happens when the host reinstalled Deskhub or lost its
settings directory — or when a different machine is answering at that address. Only if you
know why the key changed, remove the host from **Trusted hosts** (*Remove*, or
`deskhub-cli host remove ALIAS`), connect again and compare the new fingerprint as on the
first connection. With the CLI, `host update ALIAS --host-key-stdin` pins the new key
directly.

### Coming from an older Deskhub

Nothing is migrated. Passcodes and the list of paired devices from older versions of
Deskhub are not carried over: create or copy your keys, allow them on each host and trust
each host again, as described above. Both machines need this version — an older Deskhub on
either side cannot connect and is refused with "That machine uses an incompatible
authentication version".

## 🆘 If something doesn't work

- **Nothing to connect to** — both machines must be on the same network (or the same
  Tailscale tailnet), and UDP 47777 must be open on the host.
- **"This device's key is not authorized on that machine yet"** — the host does not list this
  client key; allow its public key on the host's Devices page — see
  [Letting a device in](#letting-a-device-in). Clients allowed by an older Deskhub must be
  allowed again.
- **"This host's key has changed"** — see
  [Revoking and rotating keys](#revoking-and-rotating-keys).
- **"That machine uses an incompatible authentication version"** — one side runs an older
  Deskhub; update both machines. See [Coming from an older Deskhub](#coming-from-an-older-deskhub).
- **Linux: sharing fails immediately** — run `vainfo | grep -E 'H264.*Enc'`; an empty
  result means this machine has no usable H.264 encoder and cannot host.
- **Linux: the pointer doesn't move** — the `/dev/uinput` rule from requirement 3 is
  missing.
- **macOS: a black screen or dead input** — check Screen Recording and Accessibility on
  the Settings page.
- **Still stuck** — open an
  [issue](https://github.com/manhpham90vn/Deskhub/issues) and include your device model,
  OS version and what the status line on the Host or Client page says.
