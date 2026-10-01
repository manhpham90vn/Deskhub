**English** · [Tiếng Việt](PRIVACY.vi.md) · [中文](PRIVACY.zh.md) · [日本語](PRIVACY.ja.md)

# Deskhub Privacy Policy

_Effective date: October 1, 2026 — Version 2.13_

> Translations are available at [`PRIVACY.vi.md`](PRIVACY.vi.md),
> [`PRIVACY.zh.md`](PRIVACY.zh.md) and [`PRIVACY.ja.md`](PRIVACY.ja.md). This English
> version is the authoritative one.

## 1. Introduction

This Privacy Policy describes how **Deskhub** ("the app", "we") handles
information when you use the Deskhub mobile applications (iOS, Android) and the
Deskhub desktop applications for Windows, macOS and Linux (together, "the
Software").

Deskhub is a remote desktop application: it streams the screen of one of your
computers to another device of yours and lets you control that computer with
mouse, keyboard and touch input.

The Software is developed and published by an individual developer:

- **Developer:** Manh Pham
- **Contact:** manhpv151090@gmail.com
- **Project page:** https://github.com/manhpham90vn/Deskhub

## 2. The short version

**The developer does not receive or store your session content or usage data through
Deskhub.** The app does process and store some information on your own devices, as
described below. Deskhub has no user accounts, developer-operated servers, analytics,
crash reporting, advertising or embedded third-party SDKs that collect data.

## 3. Information the Software processes

To function, the Software must process certain data **entirely on and between
your own devices**. None of it is transmitted to the developer or to any third
party.

| Data | Purpose | Where it goes | Retention |
|---|---|---|---|
| Screen content of the shared computer (video frames) | Displaying that screen on your other device | Sent directly between your two devices, encrypted in transit (QUIC/TLS) | Never stored; exists only in memory during the session |
| Sound the shared computer is playing (only while it shares sound and a viewer asks for it) | Letting the person watching hear that computer | Sent directly between your two devices, encrypted in transit (QUIC/TLS), as compressed audio | Never stored; exists only in memory during the session |
| Mouse, keyboard, and touch input | Controlling the shared computer from your other device | Sent directly from the viewing device to the shared computer, encrypted in transit (QUIC/TLS) | Never stored, and never written to a log; discarded after injection |
| This device's key — one private key created on first run | Proving this device's identity in both directions: to the devices that connect to it when it shares, and to the hosts it connects to; people see it as one fingerprint (`SHA256:…`) | Written to `host_key.pem` in the app's own folder. The private key is never sent to another device: when it shares, a certificate built from the key is presented to connecting devices; when it connects, only the public key and a signature are sent. The certificate is not kept — the TLS library loads it only from a file, so it is written for a moment to `transport_cert.<random>.pem` in the same folder, readable only by you, and deleted once loaded; one left behind by a crash is deleted the next time this device starts sharing. It contains only the public certificate — the private key is read from `host_key.pem`. On iOS the folder is excluded from iCloud and computer backups, and on Android the app's backup is turned off; on Windows, macOS and Linux a backup tool that copies your home folder copies the key file with it. *Copy public key* puts the public key on your clipboard labelled with this device's name, for you to hand to a host's owner. The `host_cert.pem` and `client_key*.pem` files of earlier versions are no longer read | Kept until you delete the file; it is never replaced automatically. Deleting it gives the device a new identity: hosts that allowed the old one have to allow it again, and devices that trusted it are shown it as a new host |
| Trusted hosts (pinned key fingerprint, name, when this device first and last reached it, the last address and port it answered at) | Recognising a host this device has trusted, at whatever address it appears, and warning when a different key answers at an address that used to belong to a trusted host | Written to `known_hosts` in the same folder; never transmitted | Kept until you remove the host or delete the file; the address and the last-seen time are updated on each connection |
| Client public keys allowed to connect to this host, each with a label | Admitting only clients that prove they hold the matching private key | Written to `authorized_keys` in the same folder, one key and its label per line; no times are recorded. Never transmitted. A key is added when you paste it, when you approve that device's connection request, or when that device scans this host's QR code — in the last two cases the label is the name the device sent | Kept until you remove the keys or delete the file; without the file nobody can connect |
| Connection requests — the name, public key, address and time of each device that tried to connect to this host without being allowed yet | Letting this host's owner see who is asking and decide with *Approve* or *Deny* | The requesting device sent its name and public key over the encrypted connection and signed with the matching private key; only once that signature checks out does this host write them, with the address it saw and the time, to `access_requests` in the same folder. Never transmitted beyond the two devices; shown only on this host's own screen | At most 16 requests. Each is deleted after 10 minutes, or as soon as you press *Approve* (which moves the key into `authorized_keys`). *Deny* takes it off the list at once but keeps the row, marked as denied, until its 10 minutes are up, so that device cannot ask again straight away |
| QR pairing tokens — random one-time codes this host issues while it shows its QR code | Letting the one device that scans the code in without any further step | Written to `pairing_tokens` in the same folder, with each token's expiry. A token travels inside the QR code and link you show — anyone who can see that screen can read it — and, once, over the encrypted connection from the device that scanned it. The QR code also contains this device's network addresses and port, its key fingerprint and its device name | Deleted when the code is hidden, when sharing stops, when the token is used, or after 5 minutes |
| Camera frames while you scan a QR code (Android and iOS only) | Reading a host's QR code off its screen | Processed on the device only, to find and decode the code; nothing is stored, transmitted or shown to anyone | Never stored; discarded as each frame is examined |
| The IPv4 address (and port, if you give one) you type — Deskhub does not look up host names | Connecting to the other machine | Stays on the device you typed it on | Kept locally until you change it |
| The last 10 hosts you connected to — the address, the time of the last connection and the name the host reported for itself | Filling in the *Recent devices* list | Written to `recent-hosts.txt`, one host per line, in the app's own folder on your device — `%USERPROFILE%\.deskhub` on Windows, `~/.deskhub` on macOS and Linux, the app sandbox on iOS and Android; never transmitted. The `recent-devices.txt` file of earlier versions is deleted, not converted | Kept until you connect to 10 newer hosts, or you delete the file |
| Your sharing preferences (frame rate, bitrate, resolution cap, port, network address, input permission, whether this device controls the machines it views, clipboard sync, sharing sound, playing the sound of the machine you watch, keep awake, start with OS, auto share and background mode) | Restoring your settings the next time you open the app | Written to `ui-settings.txt` in the same folder. On iOS the file lives in the app group container shared by the app and its broadcast extension | Kept until you change them or delete the file |
| The launch entry *Start Deskhub when you log in* creates (desktop only) | Starting Deskhub when you log in | Created by the operating system's own mechanism, outside the app's folder: a scheduled task named *Deskhub* on Windows (run at logon with the highest privileges), `~/.config/autostart/deskhub.desktop` on Linux, a login item on macOS. It holds only the path to the app; nothing is transmitted | Removed when you turn the setting off |
| The screen-permission token the Linux desktop issues after you pick displays in its screen-sharing dialog (Linux only) | Letting later shares reuse your choice silently, so the dialog appears only the first time | Written to `portal-restore-token.txt` in the same folder; the token is meaningful only to your own desktop session on this machine and is never transmitted | Replaced after each share; removed when you press *Choose screens again* or delete the file |
| Clipboard text (only while the clipboard-sync toggle is on and a session is active) | Making text copied on one device pastable on the others | Sent directly between your devices, encrypted in transit (QUIC/TLS), capped at 32 KiB per copy; only plain text, never images or files | Never stored by Deskhub; lives only in each device's normal system clipboard |
| Whether a broadcast is currently running, how many viewers are connected and their device names, the broadcast extension's own memory use in megabytes, and the text of the last start-up error (iOS only) | Letting the app's sharing screen report the state of the broadcast extension, which iOS runs as a separate process and terminates if it uses too much memory | Written to `broadcast-status.txt` in the same app group container | Deleted when the broadcast ends |
| The device name in Settings → General → *Device name* — when left empty, this computer or device's own name is used (its hostname on Windows and Linux, its computer name on macOS, its device name on iOS, its model on Android) | Naming this device: shown to viewers when it shares, shown to the allowed clients that connect to it, shown on the host you connect to next to this device's address, shown in the connection request a host records for it, and used as the label of the public key you copy | Saved in `ui-settings.txt` in the same folder, and sent to the host when you connect — encrypted in transit, but displayed on the host's screen and written into its logs — so the default name is transmitted unless you set one of your choice. A host that has not allowed this device yet shows the name in its list of connection requests and keeps it there for up to 10 minutes. When this device shares, it also sends the name to each client that has authenticated with an allowed key — never before — that client keeps it in its own recent list, and it is written into the QR code this device shows. A device that connects using that QR code or link puts the name, as the server name, in the opening packet of the TLS handshake, which is not encrypted — anyone watching the network at that moment can read it. It is also embedded in the public key you copy, and it is the label a host stores in its `authorized_keys` when it approves this device or admits it by QR code | Kept until you change it or delete the file. Emptying the field falls back to the default rather than removing the name |
| Files you choose to send to a computer you are connected to (only when you pick them and press Send) | Putting a file from one of your devices onto another | Sent directly between your two devices, encrypted in transit (QUIC/TLS); on a phone or tablet a copy is first staged in the app's own cache so it can be read while sending | Where a file lands depends on what receives it. A computer writes it into the folder it has chosen for this — `Deskhub` in that user's home folder unless another is picked — and keeps it until that user deletes it. A phone or tablet has no such folder: a photo or video is added to that device's own photo library (`Pictures/Deskhub` and `Movies/Deskhub` on Android), and every other file is put where the system file browser can see it — the app's Documents folder on iOS, `Download/Deskhub` on Android — and stays there until you delete it. On iOS a photo the library refuses goes to Documents instead. The media-store route needs Android 10: on Android 9 and older an arriving file stays in Deskhub's own folder on the device instead, out of the gallery and out of Downloads. The staged copy on the sending phone or tablet is deleted when the sending window is closed |
| The name, size and checksum of each file offered, and the sending device's name, address and key fingerprint | Letting the receiving computer show what is arriving and refuse what it cannot store, and letting its owner see who sent what | Sent between your two devices, encrypted in transit; the receiving computer writes the offer, its verdict and the outcome to its own session log | Kept in that computer's log file until you delete it |
| The folder a computer stores received files in | Restoring the choice the next time you open the app | Written to `ui-settings.txt` in the app's own folder; never transmitted | Kept until you change it or delete the file |
| Connection statistics (bitrate, packet loss, latency) | Adapting stream quality; shown in the status bar | Exchanged only between your two devices. On Windows, macOS and Linux they are also written, as periodic `[DIAG]` summaries, to the local diagnostic log described below | Discarded when the session ends, except for what the log holds |
| Diagnostic logs (Windows, macOS and Linux) — connection statistics, the addresses and ports of peers, the device names they sent, key fingerprints, the name, size and outcome of each file sent or received and the folder it went to, and errors. Never screen content, keys pressed, pointer movement, clipboard text or terminal output | Letting you, or someone you choose to send them to, work out what went wrong | Written as plain text to one file per run, `deskhub-<date>-<time>-<pid>.log`, in the app's own folder, readable only by you (on Windows through the permissions of that folder, which the file inherits); never transmitted. On Android and iOS the same lines go only to the system log (logcat, the Xcode console), not to a file | Older logs are deleted automatically so that only the ten newest are kept; delete the folder or the files at any time |

### 3.1 Peer-to-peer by design

All communication happens **directly between your own two devices** over:

- your local network (Wi-Fi/LAN), or
- a VPN that **you** operate or subscribe to (for example Tailscale), if you
  choose to use one for access over the Internet.

We do not operate relay servers, signaling servers, or any other backend. The
Software has no technical means to send data to the developer.

### 3.2 Data we do NOT process

Apart from the device name described above, Deskhub does not ask for your name, email
address, phone number, contacts, location or advertising identifier. It does not use
your microphone. It uses the camera only while you are scanning a host's QR code on a
phone or tablet, after you tap *Scan QR code*: the frames are decoded on the device to
find the code and are neither stored nor sent anywhere. It accesses photos and files only when you select them for
transfer, when another device sends them to you, or when they appear on a screen you
choose to share. The sections above describe where those files go and how long they
remain on your devices.

### 3.3 Sharing a phone or tablet screen

Android and iOS devices can share their own screen as well as view another
machine's. The stream is **view-only**: incoming mouse and keyboard packets are
discarded, because neither operating system lets an ordinary app drive the
device. What is captured is the **whole screen**, including anything that
appears while sharing — notifications, other apps, banking apps, passwords you
type. On Android the system shows its own recording-consent dialog for every
share and a permanent notification while it runs; on iOS the system broadcast
indicator stays visible. Both are the operating system's own signals, and
either can be used to stop sharing at any time. As on desktop, the video goes
directly to your other device and is never stored or sent to us.

### 3.4 Scope of screen sharing and remote control

Sharing streams the **entire selected display**: everything that appears on
that monitor is visible to the connected viewer, including notifications,
pop-ups, and any window you open while sharing. (Sharing a single application
window was removed on 2026-07-27; the Software now shares whole displays
only.) When you allow remote control, the viewer's input is injected as if
they were sitting at the PC and can reach **any application visible on the
shared display** — it is no longer limited to one window. On any host you can
turn remote control off entirely in Settings, which makes the share view-only:
input that arrives is discarded instead of injected. Two safety
mechanisms remain active whenever control is allowed: if the person at the PC
touches the real mouse or keyboard, remote input pauses ("host wins"), and any
keys held by the remote side are automatically released when the connection
ends or the viewer switches away. Up to five viewers can watch one PC at once,
but only one of them drives the mouse and keyboard at any moment.

## 4. Permissions the apps request

| Platform | Permission | Why |
|---|---|---|
| iOS | Local Network | Required by iOS to send/receive traffic to your PC on the same network. Used only for the streaming session. |
| iOS | Screen recording (broadcast) | Only when you start sharing this device's screen, from the system broadcast picker. iOS asks every time and shows a recording indicator throughout. |
| Android | `INTERNET`, network state | Required to open the UDP connection to your PC. Used only for the streaming session. |
| Android | Screen capture consent (`MediaProjection`) | Only when you start sharing this device's screen. Android asks every time; the answer cannot be remembered. |
| Android | `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_MEDIA_PROJECTION` | Keeps the share running while the app is in the background or the screen is off. Required by Android for screen capture. |
| Android | `RECORD_AUDIO` | Android puts its playback-capture API behind this permission, and playback — what the device itself is playing — is the only thing Deskhub captures. Asked for when you start a share; decline and the share goes ahead without sound. Deskhub never opens the microphone. |
| Android | `POST_NOTIFICATIONS` | Shows the ongoing notification Android requires while a screen share is running, and names the files that arrive when another device sends you one, and tells you when a device asks to connect to this one (a *Connection request* notification with the device's name and address). No other notifications are sent. |
| iOS | Photo library, add only | Asked for the first time a photo or video someone sent you arrives on this device, so it can be put in the Photos app. Deskhub can only add items: it never reads, changes or deletes what is already in your library. Decline and the file goes to the app's Documents folder instead. |
| iOS | Notifications | Names the files that arrive when another device sends you one, and tells you when a device asks to connect to this one (a *Connection request* notification with the device's name and address). No other notifications are sent. |
| macOS | Notifications | Asked for the first time a device asks to connect to this one, to show a *Connection request* notification with the device's name and address. Nothing else is notified. |
| Android | `CAMERA` | Asked for only when you tap *Scan QR code* on the Client page, to read a host's QR code off its screen. Frames are decoded on the device and never stored or sent. Decline and you can paste the host's link into the address field instead. |
| iOS | Camera | Asked for only when you tap *Scan QR code* on the Client page, for the same purpose and with the same limits as on Android. Decline and you can paste the host's link instead. |
| macOS | Screen Recording | Needed to capture the screen (and the sound it plays) when this Mac shares. Granted once in System Settings → Privacy & Security. |
| macOS | Accessibility | Needed to inject a viewer's mouse and keyboard input when this Mac shares with control allowed, and to notice when the person at the Mac touches it ("host wins"). |
| macOS | Local Network | Asked by macOS the first time Deskhub reaches another device on your network. Used only for sessions. |
| Windows | Administrator rights (UAC) | The app runs elevated so that it can inject input into elevated windows. UAC asks on every launch, except when *Start Deskhub when you log in* starts it through its scheduled task. |
| Windows | Windows Firewall | When you share, the elevated app adds or refreshes one inbound UDP rule named *Deskhub (host)* for itself, on every network profile. It changes no other rule; a block rule you created for Deskhub stays in place and keeps it unreachable. |
| Linux | Screen-sharing consent (desktop portal) | The desktop's own dialog asks which screens to share the first time; the answer is remembered through the token described in section 3. |
| Linux | Virtual input device (`/dev/uinput`) | Needed to inject a viewer's input. The `.deb` and `.rpm` install a udev rule that opens it to the `input` group and to the user logged in at the seat. |
| Linux | Reading `/dev/input/event*` | Needed only for "host wins" — noticing that the person at the machine is typing or moving the mouse. Granted by adding your user to the `input` group yourself; the packages do not do it, and without it Deskhub works but never pauses remote input. Deskhub reads only whether input happened, and records none of it. |

Sharing sound needs no permission of its own on the desktops: it captures what the
computer itself is playing, not a microphone. Android is the exception, and only in name:
its playback-capture API is gated behind `RECORD_AUDIO`, so an Android device that shares
its sound must hold the permission the system labels *Microphone*. Deskhub uses it for
nothing else. It never records a microphone, has no two-way audio, and asks for no
microphone permission on any other platform.

The apps request no permissions beyond those listed above. If a future version needs a
new permission, it will be requested in-context and this policy will be updated.

## 5. Analytics, advertising, and third parties

- **Analytics / telemetry:** none.
- **Crash reporting:** none built into the Software. Diagnostic logs exist only on
  your own machine — in the app's console output and, on Windows, macOS and Linux, in
  plain-text files under `~/.deskhub/` (`%USERPROFILE%\.deskhub` on Windows),
  readable only by you, with older ones deleted automatically. Section 3 lists what
  they contain. They are never uploaded anywhere; they leave your device only if you
  copy and send them yourself, and you can delete that folder at any time.
- **Advertising:** none.
- **Third-party SDKs:** none that collect data. Besides its own source code
  (available at the project page) and operating-system frameworks, the Software is
  built with open-source libraries that run entirely on your device and send nothing
  anywhere of their own accord: quiche and BoringSSL (encryption and transport), Opus
  (sound), FFmpeg (video on Linux) and, on Android, AndroidX, CameraX and ZXing (the
  interface, the camera and QR decoding). They are listed with their licences in
  [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
- **App distribution:** the iOS app is distributed through Apple's TestFlight and
  the Android app through Google Play. Apple and Google may collect installation,
  usage and crash data under their own privacy policies; that collection is outside
  our control. Through TestFlight and the Google Play Console they may show the
  developer crash reports from individual devices whose owners allowed them to be
  shared — with the device model, operating-system version and app version — and,
  for TestFlight, any feedback a tester chooses to send, as well as aggregated
  statistics. None of it contains your session content.
- **Tailscale or other VPNs:** if you choose to connect through a VPN, your
  traffic is handled under that provider's privacy policy. Deskhub neither
  requires nor bundles any VPN.

## 6. Security

- Streaming traffic stays inside your own network or your own VPN tunnel.
  When you use a VPN such as Tailscale, traffic between devices is end-to-end
  encrypted by that VPN (WireGuard).
- Deskhub encrypts its session traffic — video, control, input, clipboard and
  terminal data all run over QUIC/TLS between your devices. A client must sign
  a transcript of the connection with a key the host lists in `authorized_keys`,
  and the client checks the host's pinned key before sending anything. A key is
  listed only when the host's owner approves that device's connection request,
  shows it the host's QR code, or pastes its public key. No passcode
  is stored or transmitted. Deskhub never scans your network and answers no
  unencrypted discovery requests. The device name is encrypted in transit but
  displayed on the host, shown in connection requests and embedded in the public key
  you copy, so do not put anything sensitive in it. Never
  expose Deskhub to the Internet directly. The full threat model — what is
  protected, what is not, and how to report a vulnerability — is in
  [`SECURITY.md`](https://github.com/manhpham90vn/Deskhub/blob/main/SECURITY.md).
- Data left by earlier versions is not converted: the old `paired_devices` list, the
  old activation marker and the old `auth_salt` file are deleted, and a passcode line
  in an old `ui-settings.txt` is removed the first time the file is loaded. An
  `authorized_keys` or `known_hosts` file that cannot be read denies access while
  it is unreadable and is written afresh on the next change.
- Because we hold no data about you, there is no developer-side database that
  could be breached.

## 7. Data retention and deletion

We retain nothing, so there is nothing for us to delete. All session data
disappears when the session ends. The address saved in the app is removed by
clearing the field or uninstalling the app. The recent-device list, the
saved settings, the key, the allowed clients, the trusted hosts, any waiting connection
requests and any live QR tokens are removed by deleting the app's
folder (`%USERPROFILE%\.deskhub` on Windows, `~/.deskhub` on macOS and Linux),
which the app recreates empty on the next launch; the diagnostic logs live in the
same folder and go with it. On iOS and Android, uninstalling the app removes them. The
launch entry *Start Deskhub when you log in* creates lives outside that folder: turn
the setting off before deleting the folder or uninstalling, or remove the entry by hand.

Files another device sent you are yours, not the app's. Once delivered they live in the
folder that computer chose, or in your photo library, Documents or Downloads on a phone or
tablet, and uninstalling Deskhub leaves them exactly where they are; delete them there.

## 8. Your rights (GDPR, CCPA, and similar laws)

Laws such as the EU General Data Protection Regulation (GDPR) and the
California Consumer Privacy Act (CCPA) grant rights over personal data —
access, correction, deletion, portability, objection, and non-discrimination.

Because Deskhub does not collect or hold any personal data, there is no data
on which to exercise these rights. If you believe we do hold data about you,
contact us at the address below and we will respond within 30 days.

We do not "sell" or "share" personal information as defined by the CCPA.

## 9. Children's privacy

The Software is not directed at children and, as described above, collects no
data from anyone, including children under 13 (COPPA) or under 16 (GDPR).

## 10. International data transfers

Deskhub does not send data to the developer. If you connect through a VPN, session data
travels between your devices over that network, as described in section 3.1.

## 11. Changes to this policy

If the Software's data practices ever change (for example, if a future
version adds optional crash reporting), this policy will be updated **before**
the change ships, with a new effective date and a changelog entry below. The
current version is always published at:
https://github.com/manhpham90vn/Deskhub/blob/main/PRIVACY.md

| Version | Date | Change |
|---|---|---|
| 2.13 | 2026-10-01 | **A denied connection request is kept until it expires.** Pressing *Deny* on a connection request used to delete the row from `access_requests` at once. It now takes the request off the list at once but keeps the row — the same name, public key, address and time, marked as denied — until its 10 minutes are up, so the denied device's next attempts are refused instead of filing a new request. Nothing new is collected or transmitted. |
| 2.12 | 2026-10-01 | **Corrections and tighter handling of local data.** Diagnostic log files on Windows, macOS and Linux are now readable only by you, and older ones are deleted automatically so that only the ten newest are kept; the Windows app no longer writes the keys a viewer presses into them, whether in remote control or in a terminal; this policy now lists what they contain, and none of it is the keys a viewer presses. Connection statistics are recorded in those logs, which earlier versions of this policy did not say. A connection request is recorded only after the requesting device has signed with its key, so the key it shows is proven. The TLS certificate built from this device's key — the public part only — is written briefly to a file readable only by you and deleted at once, and one left behind by a crash is deleted the next time this device starts sharing; earlier versions said it was never stored. On iOS the folder holding this device's private key is now excluded from iCloud and computer backups. The old `auth_salt` file is now deleted, and a passcode line left in an old `ui-settings.txt` is removed as soon as the file is loaded. This version also corrects earlier statements: `known_hosts` records when each host was first and last reached; the settings file also stores whether this device controls the machines it views and whether it plays their sound; the launch entry *Start Deskhub when you log in* creates is now listed; the iOS broadcast status file holds the names of connected viewers; a device that connects using a QR code or link sends the host's name unencrypted in the opening TLS handshake; the address you type is an IPv4 address, never a host name looked up; the permissions table now covers macOS, Windows and Linux; the Software contains open-source libraries but no SDK that collects data; and the iOS app is distributed through TestFlight, which, like the Google Play Console, can show the developer per-device crash reports. |
| 2.11 | 2026-09-30 | **One key per device, connection requests and QR pairing.** Each device now has a single key (`host_key.pem`) that is its identity both when it shares and when it connects; the separate client keys (`client_key*.pem`) and the stored certificate (`host_cert.pem`) are gone — the certificate is built in memory and never stored, and leftover files are ignored, not converted. Trusted hosts are remembered by key fingerprint with the last address each answered at, no longer by address. Two new files in the app's folder, both never transmitted beyond the two devices involved: `access_requests` holds the name, public key, address and time of each device that asked to connect without being allowed yet (at most 16, each deleted after 10 minutes or on *Approve* / *Deny*), and `pairing_tokens` holds the random one-time tokens behind the QR code a host can show while sharing (deleted when the code is hidden, used or expired after 5 minutes). The QR code itself contains the host's addresses, port, key fingerprint, device name and token, and is read by whoever sees the screen. A device's name is now also shown in the connection request it leaves and becomes the label of its key when a host approves it or admits it by QR code. On Android and iOS the camera is used only while you scan a QR code, behind a permission asked for at that moment; frames are decoded on the device and never stored or sent. A new connection request also raises a system notification on this device naming the requesting device and its address; it is shown by this device's own notification centre and goes nowhere else. |
| 2.10 | 2026-09-29 | A host now sends its device name to each client that has authenticated with an allowed key — nothing is sent before authentication — and the client keeps that name in its recent list. The recent list moved to a new file, `recent-hosts.txt` (address, time of the last connection, host name; at most 10). The old `recent-devices.txt` is deleted rather than converted. |
| 2.9 | 2026-09-29 | **Passcodes are gone and access works like SSH.** No passcode is stored or transmitted anywhere any more. LAN discovery was removed: Deskhub never scans your network, and a host answers no plaintext discovery requests. A host keeps the client public keys it allows in `authorized_keys`, each with a label; a client keeps the hosts it trusts in `known_hosts` — pinned host-key fingerprint, address, name and the client key to use. One device name, set in Settings, is sent to the hosts you connect to and embedded in the public keys you copy. No times are kept for allowed clients. Data files from earlier versions — passcodes, the old `paired_devices` list, the old activation marker — are deleted rather than converted. |
| 2.8 | 2026-09-28 | Hosts can store authorized public keys in `authorized_keys` and keep the activation marker locally. Saved host profiles now include a selected client identity and alias. The older fingerprint-only list is used only until the new list is activated. |
| 2.7 | 2026-09-28 | The CLI can create and import additional named client signing keys and select one for a connection. Each named private key is stored locally in its own file; listing identities exposes only public key information. |
| 2.6 | 2026-09-28 | Client admission now requires an authorized signing key and a pinned host key. Recent devices and UI settings no longer store passcodes; legacy fields are removed when their files are loaded and safely rewritten. A failed rewrite leaves the old file for a later retry. |
| 2.5 | 2026-09-07 | A correction, not a change — Deskhub behaves exactly as before. Earlier versions of this policy said a file arriving on an Android phone or tablet goes to `Pictures/Deskhub`, `Movies/Deskhub` or `Download/Deskhub` through the system media store. That holds on Android 10 and later. The media-store route Deskhub uses needs Android 10, so on Android 9 and older an arriving file stays in the app's own folder on the device and never appears in the gallery or in Downloads. Either way it reaches nobody but the two devices involved. |
| 2.4 | 2026-08-28 | **Phones and tablets now take files as well as send them**, and where a file lands on one is new. On iOS a photo or video is added to your photo library, which asks for the system's add-only Photos permission the first time — Deskhub can only add, never read or change what is already there — and anything else is put in the app's Documents folder, where the Files app can see it. On Android a photo goes to `Pictures/Deskhub`, a video to `Movies/Deskhub` and everything else to `Download/Deskhub`, all through the system media store. Both name what arrived in a notification. None of it reaches us. This version also corrects two things earlier versions of this policy stated wrongly: Android has always needed the permission the system labels *Microphone* (`RECORD_AUDIO`) to capture what the device itself is playing, which is what version 2.1 describes as sharing sound — Deskhub still never records a microphone — and the desktop apps never lost the *File transfer* tick described in 2.3, only the saved setting behind it. |
| 2.3 | 2026-08-24 | **Taking files stopped being a saved setting.** The stored *Take files viewers send* preference was removed from `ui-settings.txt`: a phone or tablet takes files whenever the app is on screen, and a computer offers *File transfer* as one of the things it shares, ticked by default each time and never remembered, so it still takes files only while it is sharing. What happens to a file that arrives is unchanged: it still needs a paired, admitted sender, still lands where that machine puts received files, still never overwrites a file already there, and is still logged locally with the sending device's name, address and key fingerprint. Sharing the screen stays a deliberate act behind its own button, and a computer sharing its screen keeps taking files at the same time rather than shutting that off for the duration. |
| 2.2 | 2026-08-21 | Deskhub can now **send files** between your own devices. A computer sharing its screen may also offer to take files, and any device connected to it can pick files and send them; on Android and iOS the files come from the system photo picker or the system file browser, and a copy is staged in the app's own cache while it is sent, then deleted. Files travel directly between your two devices over the same encrypted transport as the picture, and are never sent to us or through any server of ours. The receiving computer writes them into a folder it chooses — `Deskhub` in that user's home folder unless another is picked, saved with your other preferences in `ui-settings.txt` — never overwrites a file already there, and writes each offer, its verdict and the outcome, along with the sending device's name, address and key fingerprint, to its own local session log. Taking files is off unless the person sharing ticks it, and phones and tablets only send files: they never take them. |
| 2.1 | 2026-08-19 | Sharing a screen can now share that computer's **sound** as well. What is captured is the mix the computer's own speakers are playing — never a microphone; Deskhub has no two-way audio and asks for no microphone permission. The audio is compressed, sent directly to the people watching over the same encrypted transport as the picture, and never stored. It travels only when the machine sharing has *Share this device's sound* on **and** a viewer has *Play the sound of the device you are watching* on; either switch turns it off. Both switches are saved with your other preferences in `ui-settings.txt` and are on by default. |
| 2.0 | 2026-08-15 | Sessions now run over an encrypted transport (QUIC/TLS) — video, input, clipboard and terminal traffic alike — and machines are admitted by pairing. New data stored on your own device, all in the app's folder and never sent to us: a key pair that is this machine's identity (`host_key.pem`, `host_cert.pem`), the keys of hosts you have trusted (`known_hosts`), the machines allowed into this host (`paired_devices` — key fingerprint, the name each sent, timestamps), and a non-secret salt (`auth_salt`). The passcode is now optional and is never transmitted: the pairing handshake proves it without sending it. |
| 1.9 | 2026-08-14 | On Linux, the screen choice made in the desktop's screen-sharing dialog is now remembered: the permission token the desktop issues is saved to `portal-restore-token.txt` so later shares skip the dialog. The token only works for your own desktop session on this machine, is never transmitted, is replaced after each share, and is removed when you press *Choose screens again* or delete the file. |
| 1.8 | 2026-08-14 | New *keep awake* toggle (on by default): while you are sharing or viewing, the app asks the operating system not to sleep the machine or turn off the display, and releases that request when the session ends. Only the on/off choice is stored, in the same local settings file; nothing about it is transmitted, and no system sleep settings are changed. |
| 1.7 | 2026-08-13 | Clipboard sync now also works on Android and iOS, with the same toggle, 32 KiB cap and plain-text-only rule as on desktop. The operating systems limit it: an Android device can read its own clipboard only while Deskhub is the app in the foreground (incoming text is applied at any time); iOS may show its system paste prompt when Deskhub reads a fresh copy; an iOS device that is hosting never syncs its clipboard, because its broadcast runs in a separate process. Phones and tablets also gain the desktop's choice of which network address to share on, saved in the same local settings file and never sent anywhere. Nothing new is stored on any device beyond that choice. |
| 1.6 | 2026-08-13 | Desktop apps gain an optional clipboard sync: with its toggle on, plain text you copy during a session is sent between your devices (unencrypted, like the rest of the traffic, capped at 32 KiB) and placed in the other machine's clipboard; Deskhub never stores it. New locally saved settings: which network address to share on, start-with-OS, auto-share on launch, background/tray mode, and the clipboard toggle itself. Turning on start-with-OS creates the platform's own launch entry (an autostart file on Linux, a scheduled task named *Deskhub* on Windows, a Login Item on macOS); turning it off removes it. |
| 1.5 | 2026-08-13 | Each client can now set a device name (the *Your name* field on its connect page). The name is saved in the existing `ui-settings.txt` settings file on your own device and is sent to the host when you connect — unencrypted, like the rest of the traffic — so the host can label this viewer in its session list, status lines and logs. The host keeps the name only in memory, only while you are connected, and never stores it. The field is prefilled with your computer or device's own name, so that default is transmitted unless you replace it with a name of your choice. |
| 1.4 | 2026-08-12 | The iOS status file the broadcast extension shares with the app now also records the extension's own memory use in megabytes, so the sharing screen can show it. The value describes only the Deskhub broadcast process, stays inside the app group container on your device, and is deleted with the rest of the status file when the broadcast ends. |
| 1.3 | 2026-08-12 | Android and iOS devices can now share their own screen, view-only, so a phone or tablet screen can be streamed to another device you own. This adds the screen-capture permissions each OS requires (plus a foreground service and its notification on Android) and, on iOS, an app group container shared between the app and its broadcast extension for your passcode and port, plus a short-lived status file the extension writes there so the app can show whether the broadcast is running. The video still travels only between your own devices and is never stored. |
| 1.2 | 2026-08-07 | The passcode is now required on every host, generated on first launch instead of left blank, and every client can enter one. Sharing settings are now saved on macOS and Linux as well as Windows, and the recent-device list is saved on every platform, each inside the app's own local folder. No new data leaves your devices. |
| 1.1 | 2026-08-05 | The Windows app now saves data between launches: a list of the last 10 addresses you connected to, your sharing settings, and the passcodes used with either. All of it stays in `%USERPROFILE%\.deskhub` on your own machine; none of it is transmitted anywhere. Documented view-only sharing and the 5-viewer limit. |
| 1.0 | 2026-07-24 | First publication. |

## 12. Contact

For any question about this policy or about privacy in Deskhub:

- **Email:** manhpv151090@gmail.com
- **Issues:** https://github.com/manhpham90vn/Deskhub/issues
