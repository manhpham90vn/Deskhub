**English** · [Tiếng Việt](SECURITY.vi.md) · [中文](SECURITY.zh.md) · [日本語](SECURITY.ja.md)

# Deskhub Security Policy

_Last updated: October 1, 2026_

## ⚠️ Read this first

**Use Deskhub on a network you trust or through a VPN. Never port-forward UDP 47777 or
expose a sharing machine directly to the Internet.**

Sessions run over QUIC/TLS, including video, keystrokes, mouse, clipboard and terminal
traffic. Access works like SSH: a client gets in only if its public key is listed in the
host's `authorized_keys`, and it proves it holds the matching private key. A key gets
onto that list only by an act of the host's owner — pressing **Approve** on the
device's connection request, showing the device a **QR code** while sharing, or pasting
the device's public key. There is no passcode and no switch that lets unknown machines
in. Every machine has one key, and a client pins the host's key — on first connection
after comparing the fingerprint, or from the QR code — and checks it before sending
anything; a host that changes address stays trusted, and a different key at a known
address is treated as a machine never met before, with a warning. The host answers no
plaintext packet: anything outside an encrypted connection is dropped.

Every host can also share **view-only** (input is dropped instead of injected).

Encryption does not remove every network risk. The first connection to a host trusts
whatever key it is shown unless you compare the fingerprint, and the app does not resist
flooding.

For remote access, use a VPN. The project has been tested with
[Tailscale](https://tailscale.com); connect to the host's `100.x.y.z` address.

Keep the host on a trusted network, and review the limits below before sharing a screen
or terminal.

## Threat model

### What Deskhub protects against

| | |
|---|---|
| Data reaching the developer | Nothing does. There are no servers, no accounts, no telemetry, no third-party SDKs that collect data. See [`PRIVACY.md`](PRIVACY.md). |
| Someone reading your traffic | Every session runs inside QUIC/TLS — video frames, keystrokes, clipboard text and terminal bytes are all encrypted between the two machines. A packet capture yields traffic volume and timing, not content. Unencrypted packets arriving at the port are dropped, and the host sends nothing at the application level — not even what it shares — before a client has authenticated. |
| A remote viewer fighting you for the machine | "Host wins": the moment you touch the real mouse or keyboard, remote input is paused. Windows and macOS hosts do this out of the box. A Linux host does it only when the user running Deskhub can read `/dev/input/event*` — in practice, membership of the `input` group, which the packages do not grant; without it the log says so once and remote input is never paused. |
| Keys left stuck down | Any key the remote side is holding is released automatically when the session ends or the viewer switches away. |
| A stranger connecting uninvited | Only a client whose public key is in the host's `authorized_keys` gets in, and it must sign a transcript of this very connection with the matching private key. A key reaches that list in three ways only, each in the owner's hands: the owner presses **Approve** on the device's connection request — a row showing the device's name, the fingerprint of the key it proved it holds and its address — the device scans the QR code the owner shows while sharing, whose random 32-byte token works once, for 5 minutes, and dies when the code is hidden or sharing stops, or the owner pastes the device's public key. A stranger who connects is not let in: only after it has signed with the key it presented does the host record a request the owner can ignore, and about two seconds after answering the host closes the connection itself, as it does for every connection it refuses. There is no passcode, a missing `authorized_keys` file means nobody gets in, and nothing lets the client talk its way past the owner. A host keeps at most 8 connections waiting to authenticate — every connection that has not finished authenticating counts, whatever key it offers, and a ninth is not accepted — and drops each one 10 seconds after accepting it unless it has authenticated by then; it keeps at most 16 requests, at most one per source address — a new request from an address that already has one replaces it — each for 10 minutes; 3 bad signatures from one key and address within a minute block that pair for 10 seconds, and a wrong QR token, from a device that has signed, counts against the same limit for its source address. Waiting for approval is not counted as a failure. Removing a key on the host's Devices page also closes that device's running sessions at once. Admission lasts only as long as the connection that earned it. |
| A machine-in-the-middle | Every machine has one key. A client pins the host's key — on first connection after the user compares the fingerprint, or from the QR code, which carries it — and checks it before sending anything on every later one. Trust follows the key, so a host at a new address is still the host; a *different* key at an address the client knows is refused as trusted and shown as a **New host** with a warning naming the machine that used to answer there — it cannot be accepted as a "change", only trusted afresh with its fingerprint on show. A client that scans a QR code sends the token only after the machine answering has proved, through the TLS handshake, that it holds the key printed in the code; anything else at that address gets nothing. The client's signature covers a session identifier exported from the TLS session and the host key fingerprint the client saw, so a signature relayed to another host, or replayed on another connection, does not verify. |
| Viewers fighting each other for the mouse | Up to 5 viewers may watch one host, but only one drives input: the earliest to have joined wins, and a later viewer's input is dropped until the earlier one has been idle for a second. A 6th viewer is rejected as `Busy`. |
| A viewer you only want to show the screen to | View-only sharing, available on every host, drops input packets at the host before anything is injected — it is not enforced by asking the client to behave. Android and iOS hosts are view-only unconditionally. |
| A phone left sharing by accident | The operating system, not Deskhub, is the backstop: Android keeps a permanent notification up and re-asks for recording consent on every single share, and iOS keeps its broadcast indicator visible. Either can stop the share without opening the app. |
| An allowed client writing files onto yours | Only an admitted machine can send files, and only while the receiving machine is offering file transfer. What arrives cannot escape the folder that machine chose: the name on the wire is cut to its last path element and scrubbed of separators, control bytes, characters the filesystem rejects and reserved device names before any file is opened; each file is written under a `.deskhub-part` name and renamed only once it has arrived whole with a matching CRC-32; and a name already present gets a number rather than overwriting anything. A batch is capped at 32 files, 8 GiB per file and 32 GiB in total. The same scrubbing runs on a phone or tablet before anything reaches its photo library or its Downloads folder. |
| Malformed packets | Every field is bounds-checked before it is read. The parsers are covered by unit tests, run under AddressSanitizer, UndefinedBehaviorSanitizer and ThreadSanitizer in CI, and fuzzed with libFuzzer — for 30 seconds per target on every pull request and 15 minutes per target nightly. There are nine targets, covering the wire format, H.264 parsing (Annex B and SPS), packet reassembly, the host and viewer session state machines, UI text, terminal byte streams, the stored key and trust files with the pairing link, and the QR code encoder. Crashes found by fuzzing are kept in-repo as regression tests, and new coverage is folded back into the seed corpus. |

### What Deskhub does **not** protect against

This is the honest list. Nothing below is solved today:

- **A kept shell belongs to every allowed client, not to the machine that opened it.**
  A shell left behind on a host outlives the connection that opened it, with no time
  limit, and every admitted machine can list the shells a host is keeping, reattach a
  detached one, and close any of them. The id, size and device name of each shell are
  part of that listing. So a second client you allow — or one whose key you have not
  removed on the Devices page — can read back what an earlier shell was doing and
  carry on in it. Remove a key you no longer trust, and close the shells you are
  finished with rather than leaving them.
- **The first meeting is a leap of faith.** Pinning the host key stops a
  machine-in-the-middle who arrives *later* — a different key is a different host, and
  the dialog says so. It cannot stop one who is already in the middle at the very first
  contact unless you compare the fingerprint the *New host* dialog shows with the one on
  the host's Devices page, as the dialog asks. Scanning the host's QR code does that
  comparison for you, because the code carries the fingerprint. `deskhub-cli` refuses an
  unknown host unless told `--accept-new-host-key`, or you can pin the key in advance
  with `host add … --host-key-stdin`.
- **Approving the wrong request is the owner's mistake, and Deskhub cannot catch it.**
  A connection request shows a name the device chose for itself, the fingerprint of the
  key it holds, and the address it came from. The device has proved it holds that key —
  a request is recorded only after a valid signature — but the name proves nothing, and
  keys cost nothing to make. Check the fingerprint against the device's own Devices page,
  and the address against where you expect it to be, before pressing *Approve*. Anyone on
  the network can leave a request; only you can turn one into access.
- **Requests can be crowded out, but not from one address.** A host keeps at most one
  request per source address, so a request from an address that already has one
  replaces it, and at most 16 in all, dropping the oldest to make room. Fresh keys from
  one machine therefore overwrite only that machine's own row; pushing a genuine request
  off the list takes 16 different addresses. A device that shares an address with
  someone else — behind the same NAT, say — can still have its request replaced by
  theirs. If a request you expect is missing, ask the device to connect again.
- **A QR code is a secret for five minutes, to anyone who can see the screen.** It lets
  one device in without any click on the host. Someone who photographs it — over your
  shoulder, from a screenshot, from a shared screen — can use it in your place until it
  expires, is used, or is hidden. Show it only to the person you mean to let in, and
  hide it as soon as they are connected; the device it admitted then appears under
  *Devices allowed to connect to this machine*, where you can remove it if it is not the one you expected.
- **A `deskhub://` link is only as trustworthy as whoever sent it.** The link a host
  shows beside its QR code can be opened from any web page, message or app on a phone
  or tablet (on Android, only `deskhub://pair` links reach the app). Opened that way,
  Deskhub does not trust it silently: unless the host it names is already trusted, it
  shows the same *New host* confirmation as a first connection, with the fingerprint the
  link names, and connects only after you accept. But that fingerprint comes from the link itself.
  Accept a link someone else made without comparing the fingerprint with the host's
  Devices page, and you may be trusting their machine instead: it gets the pairing token
  meant for the real host, shows you whatever screen it likes, and receives what you
  type and send in that session. Scanning a QR code with the in-app scanner trusts the
  code without asking, because you are pointing the camera at the host's own screen.
- **On Linux, the package widens who may inject input.** Remote control needs
  `/dev/uinput`, so the `.deb` and `.rpm` install a udev rule that gives it to the
  `input` group and to the user logged in at the seat (`MODE="0660", GROUP="input",
  TAG+="uaccess"`). Any program running as that user can then create a virtual keyboard
  or mouse, not only Deskhub. And the `input` group that "host wins" needs also reads
  every keyboard on the machine — anything running as a member can log keystrokes. Add
  a user to it only if you accept that.
- **Traffic analysis still works.** Encryption hides content, not existence: an observer
  sees that a session is running, how much video is flowing, and when you type.
- **No rate limiting or DoS resistance.** Flooding the port will disrupt a session.
  The limits on pending authentications, waiting requests and bad signatures stop
  guessing, not flooding.
- **The QUIC handshake still answers.** The host no longer replies to any plaintext
  packet, but a QUIC/TLS handshake to the port completes before the client has proved
  anything, so a stranger who knows the address can still learn that something is
  listening, and see the host's certificate.
- **The device name is shown and logged.** The device name a client sends is encrypted in
  transit, but it is shown on the host's screen and written into the host's logs, it
  appears in every connection request the machine leaves, and it is the label of the
  public key the machine copies and of the key a host stores when it approves the
  machine or admits it by QR code — so it ends up in the `authorized_keys` of each host
  that allows that key. A host also sends its own device
  name to every client that has authenticated with an allowed key — never before — and
  that client keeps it in its recent list. It defaults to the machine's own
  hostname — often the owner's real name. Set a nickname in Settings → General →
  *Device name*; never put anything sensitive in it. Clearing it does not stop a name being sent;
  it only restores the default.
- **A viewer slot frees itself after 5 seconds of silence.** If your viewer drops off,
  its slot reopens and the next `Hello` to arrive takes it — from any machine that has
  passed admission with an allowed key.
- **Sharing exposes the entire display.** Not one window: every notification, popup and
  window on that monitor. See [`PRIVACY.md` §3.4](PRIVACY.md).
- **A phone or tablet host exposes the whole phone.** Android and iOS can host too, and
  what they stream is the entire screen — banking apps, one-time codes, messages, every
  password you type while sharing. The stream is encrypted like any other session, but
  every viewer you admit sees all of it. Mobile hosts are always view-only, which removes
  the remote-control risk but none of the exposure risk.

## Cryptography

What each piece is built from, for anyone checking the design:

- **Device key.** One ECDSA key on the NIST P-256 curve per device, generated by
  BoringSSL on first run and kept in `host_key.pem`. The same key identifies the device
  as a host and signs for it as a client. Its fingerprint is the SHA-256 of the key's
  DER-encoded SubjectPublicKeyInfo, written `SHA256:` followed by unpadded base64.
- **Certificate.** A host presents a self-signed X.509 v3 certificate for that key:
  subject and issuer `CN=deskhub`, a random 63-bit serial number, valid for 20 years
  from the moment the port opens, signed with ECDSA-SHA256. It is made afresh each time
  and carries no name or address.
- **Transport.** QUIC through Cloudflare's quiche, whose TLS 1.3 comes from BoringSSL,
  with the ALPN `deskhub`. Neither side checks certificate chains — there is no
  certificate authority, so peer verification is off — and the client instead compares
  the SHA-256 of the certificate's public key with the fingerprint it has pinned, before
  it sends anything.
- **Client authentication.** Both ends derive a 32-byte session identifier with the TLS
  exporter, label `EXPORTER-Deskhub-Auth-v5`. The client signs a transcript made of a
  fixed domain string (`Deskhub/auth/signature`), the protocol version, its role, that
  session identifier, its own public key and the host's key fingerprint; the host
  verifies it against `authorized_keys`. Deskhub's own keys sign with ECDSA P-256 and
  SHA-256. A host also accepts a pasted `ssh-ed25519` public key and verifies Ed25519
  signatures made with it; Deskhub itself never creates one, because TLS needs the
  device key to be P-256.
- **Pairing tokens.** 32 bytes from the operating system's random generator
  (`BCryptGenRandom` on Windows, `arc4random_buf` on Apple platforms, `getrandom` on
  Linux and Android, falling back to `/dev/urandom`), compared in constant time, used
  once, and alive for 5 minutes. A host keeps at most 4 live at a time and drops the
  one closest to expiring to make room for a new one.

## Where to run Deskhub

✅ **Recommended environments**

- A home or personal LAN where you control every device on it.
- A Tailscale tailnet (or another WireGuard/VPN tunnel) that only your own devices have
  joined. The VPN adds a second layer of encryption and keeps strangers from reaching
  the port at all.
- A machine that is only ever a *client* (phone, tablet, laptop that never shares its
  screen). Clients accept no inbound sessions.

❌ **Avoid these setups**

- Port-forwarding UDP 47777 through your router, or putting a sharing machine in a DMZ.
- Sharing your screen on café, hotel, airport, campus, coworking or conference Wi-Fi.
- Sharing on an office or shared-house LAN where you do not trust every other device.
- Any network with guest devices, IoT devices you did not configure, or roommates'
  machines you do not administer.
- Exposing the port through a cloud VM's public interface or a public tunnel service.

By default the socket binds to all interfaces (`INADDR_ANY`), so it is reachable on
every network the machine is attached to — including one you forgot it was joined to.
The **Share on network** setting narrows this: pick one of the machine's addresses and
the host binds only that interface, so machines on the other networks cannot even
reach the port. Two caveats: if the chosen address no longer exists when you start
sharing (cable unplugged, DHCP gave you a new address), Deskhub falls back to all
interfaces and says so in the sharing status — check the banner if you rely on this;
and binding one interface also stops loopback (`127.0.0.1`) viewers on the same
machine. On Windows the app runs elevated from the moment it starts, so that it can
inject input into elevated windows: UAC asks every time you launch it, except when
*Start Deskhub when you log in* launches it through its scheduled task, which runs it elevated at
logon without asking. It also opens the firewall for you when you share, with an
inbound UDP rule named *Deskhub (host)* — the rule covers the whole app on every
profile, so a narrowed bind does not narrow the firewall; that convenience is also what
makes the rule above matter. Deskhub replaces only that rule of its own. A block rule
you created for the app yourself is left alone, and because Windows lets a block rule
win over an allow rule, the host then stays unreachable until you remove it.

## What an attacker on the same network can do

If someone is on the same LAN as a machine that is sharing its screen, and Deskhub is
running, they can:

1. Find it by trying a QUIC handshake against UDP 47777 on each address. No plaintext
   packet gets an answer, but the handshake itself does, so the machine gives itself away
   to a targeted scan.
2. Try to get in — which takes a private key whose public half is in `authorized_keys`,
   and only the owner's *Approve*, a live QR token or a paste puts one there. What a
   stranger *can* do is leave a connection request that the owner sees on the Host page,
   under a name of their choosing and with a key they have just made; it expires after
   10 minutes, each address holds one request at a time — so fresh keys from one machine
   only overwrite its own row — and only an *Approve* turns it into access. They can guess at a QR token, signing each guess with a key of
   their own, but a wrong token is charged to their address like a bad signature — 3 in a minute and they are blocked for 10
   seconds — and a token is 32 random bytes that lives 5 minutes and works once.
   Beyond that, the most they can do is try to sit in the middle of a client's *first*
   connection to a host, which the fingerprint comparison — or the fingerprint inside
   the QR code — catches.
3. Watch the traffic without getting in — and learn only volume and timing. The
   session's content, video included, is encrypted; a capture no longer reconstructs
   the screen or the keystrokes.
4. Flood the port and disrupt the session. Nothing rate-limits an attacker who can
   reach the machine.

The "host wins" behaviour limits mischief while you are *sitting at* the machine. It
does nothing while you are away from it, which is when it matters.

## Hardening checklist

If you want to keep using Deskhub as it is today, these are worth doing:

- [ ] Run Tailscale on both machines and connect only over the `100.x.y.z` address.
- [ ] Confirm your router has **no** port-forward or UPnP mapping for UDP 47777.
- [ ] Allow only the client keys you need on the host, and compare the host key
      fingerprint on the first connection from each client. Review the Devices page and
      remove keys you no longer recognize. Untick *Viewers can control this machine* when only viewing is needed.
- [ ] Approve only the connection requests you were expecting, and check the fingerprint
      and address in the row before you do. Deny or ignore the rest — they expire on
      their own.
- [ ] Hide the QR code as soon as the device you showed it to is connected, and never
      show it on a screen you are sharing or presenting.
- [ ] Quit Deskhub when you are not actively using it. It does not run as a background
      service — quitting it closes the hole. Three settings under *Launch & background*
      change that: *Start Deskhub when you log in* launches it at every logon (on Windows
      through a scheduled task that runs it elevated without a UAC prompt),
      *Start sharing when Deskhub opens* then shares straight away, and
      *Keep running in the background (tray icon) when the window is closed* means
      closing the window no longer quits. Together they leave a host listening from the moment you log in with
      no window on screen. Leave them off unless you mean exactly that, and quit from the
      tray icon when one is showing.
- [ ] On Linux, think twice before adding yourself to the `input` group for "host
      wins": it also lets every program you run read every keyboard.
- [ ] On Linux, if you use `ufw`, scope the rule instead of opening it wide:
      `sudo ufw allow from 100.64.0.0/10 to any port 47777 proto udp` rather than
      `sudo ufw allow 47777/udp`.
- [ ] Do not leave a share running on a laptop that you carry onto other networks.
- [ ] Lock your machine when you walk away, so an unattended session cannot be taken
      over silently.
- [ ] With `deskhub-cli`, use `key public` to display this machine's public key
      and `host-key public` to display its key as a host — they are the same key.
      Transfer it over a channel you trust, then use `access add --stdin` on the host
      and `host add … --host-key-stdin` on the client. To approve a request from a
      terminal, read `access requests` and answer with
      `access approve --fingerprint SHA256:…` only for the fingerprint you expected.

## Local artifacts

The desktop apps write diagnostic logs in plain text under `~/.deskhub/`
(`%USERPROFILE%\.deskhub` on Windows), one file per run named
`deskhub-<date>-<time>-<pid>.log`; on macOS and Linux a `deskhub-latest.log` symlink
points at the newest. On macOS and Linux each log is created readable only by you
(`0600`) and is never written through a symlink; on Windows it takes the restricted
permissions of the folder it sits in. Older logs are deleted so that only the ten newest
are kept. They contain connection
statistics (`[DIAG]` lines: bitrate, loss, latency, frame timings), peer addresses and
ports, the device names peers sent, key fingerprints, the name, size and outcome of each
file sent or received and the folder it went to, and errors. They do not contain screen
content, the keys a viewer presses — in a remote-control session or in a terminal — or
where the pointer moves, clipboard text or terminal bytes. Android and iOS write the same lines only to the system log (logcat, the
Xcode console), never to a file.

The desktop apps and `deskhub-cli` share the rest of that folder; `DESKHUB_CONFIG_DIR`
or the CLI's `--config-dir` points both at another folder for these files:
`ui-settings.txt` (fps, bitrate, resolution cap, port, the view-only switch, the device
name, the bind address and the other toggles), `recent-hosts.txt` (the last 10
hosts you connected to — address, when, and the name each host reported), `host_key.pem`
(this machine's one private key — the identity behind its fingerprint in both roles;
anyone who copies it can impersonate this machine as a host *and* sign in wherever this
machine is allowed), `authorized_keys` (the client public keys allowed into this host,
each with its label), `known_hosts` (the hosts this machine trusts — pinned fingerprint,
when it was first and last seen, the last address and port it answered at, and its
name), `access_requests` (the devices waiting for your *Approve* — name, public key,
address and time; at most 16, each dropped after 10 minutes), `pairing_tokens` (the QR
tokens currently live and when each expires — a copy of this file is as good as the
code on screen until they expire) and, on Linux, `portal-restore-token.txt` (the
desktop's own token for the screens you picked, meaningful only to your desktop session
and never transmitted). Beside them you may see a `.lock` file for each of
`authorized_keys`, `known_hosts`, `access_requests` and `pairing_tokens`, which lets
the app and the CLI take turns writing, and short-lived `.tmp-…` files, the halves of
an atomic write.

No certificate is kept. The TLS certificate is built from `host_key.pem` in memory each
time the port opens, but the TLS library loads certificates only from a file, so it is
written for a moment to `transport_cert.<random>.pem` in the same folder, created
`0600`, and deleted as soon as it has been loaded; any such file older than a minute,
which only a crash leaves behind, is swept away the next time this machine starts
hosting. It holds only the public certificate: the TLS library reads the private key
from `host_key.pem`.

No passcode is stored anywhere. On POSIX systems the folder is created `0700` and every
file in it `0600`, written atomically; on Windows the folder is restricted to your user,
SYSTEM and Administrators, and the files in it inherit that restriction. If the home folder cannot be used, the POSIX builds fall
back to `$TMPDIR/.deskhub`, then to `.deskhub` in the current directory. Debug builds
use `.deskhub-dev` instead of `.deskhub`, so they never touch a release build's keys.
The mobile apps keep the same files inside their own sandbox — on iOS in a `.deskhub`
folder inside the app group container, shared by the app and its broadcast extension,
and excluded from iCloud and computer backups; on Android in the app's internal
storage, with backup turned off for the whole app. The desktop folder has no such
exclusion: a backup tool that copies your home folder copies `host_key.pem` with it.
Treat that folder as readable by anything running as you.

An `authorized_keys` or `known_hosts` file that cannot be read is not guessed at: while it
is unreadable, the host lets nobody in and the client refuses every host, and the next
change writes it afresh. Data from older versions is not converted: the old
paired-machines list, the old activation marker and the old `auth_salt` file are
deleted, and a `passcode=` line in an old `ui-settings.txt` is removed the first time
the file is loaded. The `client_key*.pem` and `host_cert.pem` that 7.0.x wrote are
ignored, never read; delete them if you like.

Files another machine sends land outside that folder, in the directory the receiving
machine chose for them (`Deskhub` in the user's home folder unless another is picked,
saved as `transfer_dir`). On a phone or tablet they end up in the device's photo library
or its Documents / Downloads folder, where they survive uninstalling the app. Treat
anything delivered there as a file an allowed client put on your device.

Nothing uploads any of this; delete the folder at any time.

## Build and release hardening

- **Compiler and linker.** GCC and Clang builds of the C and C++ code use
  `-fstack-protector-strong`, and optimised ones `-D_FORTIFY_SOURCE=3` — except on
  Android, which keeps the NDK's own level 2; Linux binaries, and only those, are linked
  with full RELRO (`-z relro -z now`). MSVC builds use `/sdl`. The flags live in
  `cmake/DeskhubHardening.cmake`.
- **macOS.** The app is signed with the hardened runtime but runs
  **outside the App Sandbox** (`com.apple.security.app-sandbox` is `false`): a sandboxed app cannot post
  keyboard and mouse events into other apps, which is what remote control is. It
  therefore runs with everything your user account can reach, like any unsandboxed app.
- **Windows.** The app asks for administrator rights on every launch (see above), so a
  flaw in it is a flaw in an elevated process.
- **Continuous integration.** Every pull request runs the three test suites under
  AddressSanitizer, UndefinedBehaviorSanitizer and ThreadSanitizer, clang-tidy, the
  fuzzers described above, CodeQL over the C++, Kotlin and Swift code, and a gitleaks
  scan of the whole Git history for committed secrets; CodeQL and gitleaks also run on
  every push to `main` and weekly. A pull request that adds a dependency with a known
  high-severity vulnerability fails its dependency review. Third-party GitHub Actions
  are pinned to commit hashes, and the linters and scanners the workflows download are
  checked against pinned SHA-256 sums.

## Planned mitigations

Tracked, in the order they are intended to land:

1. **Storing the machine key in the OS keychain** instead of a file.

Shipped since the last revision of this list: SSH-style access — clients admitted only by
public keys listed in the host's `authorized_keys`, each connection signed afresh; the
passcode and pairing switch removed; LAN discovery removed, so the host answers no
plaintext packet at all; limits on pending authentications and bad signatures; then, in
8.0, one key per machine with the certificate no longer stored, trust that follows the
host's key rather than its address (the hard refusal on a changed key became a *New host*
dialog that names the previous owner), connection requests the owner approves by
fingerprint over the authenticated channel, and QR pairing with a one-time token the
client sends only to the machine whose fingerprint the code names; and in 9.0,
connection requests recorded only once the requester has signed and kept to one per
source address, a host that closes a
refused connection itself, a deadline for every connection that has not authenticated,
a *New host* confirmation before a link opened from outside the app is trusted, a device
key kept out of iOS backups, no key codes in the Windows logs from remote control or the
terminal, log files readable only by their owner and pruned to the ten newest, a
firewall rule that no longer removes block rules you made, and hardened compiler and
linker flags.

This list is a statement of intent, not a schedule. Deskhub is maintained by one person
in their spare time. Treat the current state as the state, not the plan.

## Reporting a vulnerability

Please report security issues **privately** — not as a public GitHub issue.

- **Email:** manhpv151090@gmail.com — put `[Deskhub security]` in the subject.
- **Or:** open a [private security advisory](https://github.com/manhpham90vn/Deskhub/security/advisories/new)
  on GitHub.

Please include what you were running (OS, Deskhub version from the title bar or
[`VERSION`](VERSION)), what you did, and what happened. A proof of concept helps a lot.

**What to expect:** an acknowledgement within 7 days and an assessment within 30. This
is a spare-time project run by one developer, so please be patient with the timeline —
you will get a straight answer either way. If a fix ships, you will be credited in the
release notes unless you would rather not be.

There is no bug bounty; nothing is paid out.

The limits above — first-connection trust, traffic analysis, the visible QUIC handshake
and the lack of DoS resistance — are already documented. Please report new evidence about
their impact, or other security issues such as memory corruption, crashes caused by
malformed packets, data leaving a device unexpectedly, or a flaw in a released
mitigation.

## Supported versions

Only the most recent release on the [Releases page](https://github.com/manhpham90vn/Deskhub/releases)
is supported. Fixes ship in a new release; there are no backports to older versions.

## Scope

This policy covers the Deskhub source in this repository and the binaries published on
the Releases page, TestFlight and Google Play. It does not cover Tailscale, your
operating system, your router, or any other software you run alongside it.
