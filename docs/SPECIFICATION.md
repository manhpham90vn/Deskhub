**English** · [Tiếng Việt](SPECIFICATION.vi.md)

# Deskhub — Functional Specification

This document describes **what** Deskhub does, as experienced by a person using it. It is
a product specification, not a design document: it contains no implementation detail, no
protocol description and no build instructions. Those live in
[`INSTALL.md`](INSTALL.md), [`BUILD.md`](BUILD.md), [`SECURITY.md`](../SECURITY.md) and
the source tree.

- **Status:** describes the behaviour of the current code.
- **Audience:** anyone who needs to know what the product is supposed to do — testers,
  reviewers, contributors, store listings.

---

## 1. Product summary

Deskhub lets one machine show its screen to other machines on the same network, and lets
those machines drive its mouse and keyboard. It is a single application: the same app
both shares a screen and views someone else's. A desktop machine can also share a
**terminal**: a real shell on the host that other machines open in a window of their own
(sections 4 and 5).

There is no installer requirement, no account, no sign-in, no background service and no
cloud component. Two machines find each other by IP address on a network both can reach.

## 2. Vocabulary

| Term | Meaning |
| --- | --- |
| **Host** | The machine whose screen (or terminal) is being shared. |
| **Client** / **Viewer** | The machine watching a host, and optionally controlling it. |
| **Source** | One shareable thing on the host: a display, or the terminal. A host may share several at once. |
| **Session** | One viewer watching one source. Each source opens in its own window. |
| **Key** | The cryptographic identity a machine creates on first run, shown to people as a fingerprint (`SHA256:…`). |
| **Pairing** | The host letting a machine in for good. A paired machine is recognised by its key and connects without a passcode, until it is forgotten (section 9). |
| **Passcode** | The optional 4-digit code a host may require before an unknown machine can pair. |

A single machine can be host and client at the same time.

## 3. Roles by platform

| Platform | Can host | Can view | Sound |
| --- | :--: | :--: | :--: |
| Windows | ✅ | ✅ | ✅ |
| macOS | ✅ | ✅ | ✅ |
| Linux | ✅ | ✅ | ✅ |
| Android | ✅ view-only | ✅ | ⚠️ Android 10+ |
| iOS | ✅ view-only | ✅ | ⚠️ app audio only |

Every platform offers the same client feature set unless stated otherwise in section 12.
Phones and tablets host in **view-only** mode: they stream their screen but never accept
remote input, because no mobile OS lets an ordinary app drive the device.

The app is organised into the same named sections everywhere: **Host**, **Client** and
**Settings** — plus a **Devices** page listing the machines paired with this one
(section 9).

The three desktops also have a command-line client. It offers the same behaviour with no
pages at all: it hosts, connects, opens a remote shell, finds machines, and reads and
writes the very same settings, paired machines and trusted host keys the app does. It is
an alternative face on the behaviour in this document, never a different behaviour.

---

## 4. Hosting — sharing this machine's screen

| ID | Feature | Description |
| --- | --- | --- |
| H-1 | Display picker | Before sharing, the user ticks which of this machine's displays to expose. At least one must be ticked. |
| H-2 | Multi-display sharing | Several displays can be shared simultaneously; each becomes a separate source that viewers pick between. |
| H-3 | Source limit | A maximum of **8** displays can be shared at once. If the machine has more, the user is warned that only the first 8 will be shared. |
| H-4 | Start / stop sharing | One action starts sharing, one stops it. The current state is always shown (*Not sharing* / *Starting share…* / *Sharing*). |
| H-5 | Stop one display | An individual shared display can be stopped without ending the whole share. |
| H-6 | Connection details | While sharing, the app lists this machine's network addresses and the port viewers must use, so they can be read out or copied. On desktop the *Share on network* choice (T-9) sits on the hosting screen beside this list, and the list shows only the chosen network's address — *All networks* shows every address. While sharing (or starting to share) the choice is locked; stop sharing to change it. |
| H-7 | Live session table | For each shared display the host sees: display name, resolution, number of viewers, capture rate, send rate, bandwidth in use, and round-trip time. Each connected viewer appears as its own row under its display, identified by its display name and address — "Name (ip:port)" — when the viewer has set a name (C-7), or by the bare address otherwise. |
| H-8 | Disconnect a viewer | The host can drop any individual viewer from the session table. |
| H-9 | Viewer limit | At most **5** viewers may watch one host at a time. Further attempts are rejected as busy. |
| H-10 | Failure reporting | If sharing cannot start, the reason is shown to the user rather than failing silently. A terminal that cannot start because its port is taken says exactly that. |
| H-11 | Terminal source (desktop) | The source list also offers **Terminal — a shell on this machine**. It is ticked afresh every time the list is shown and never saved; sharing screen without terminal, terminal without screen, or both, are all valid. Everything shares the app's one UDP port (T-4), passcode and network choice. |
| H-12 | Shell sessions in the table | While the terminal is shared, the live table shows a *Terminal* row (with its port), and each open shell as a row beneath it identified like a viewer (C-7), with *Disconnect*; the *Terminal* row's *Stop* ends terminal sharing alone. At most **8** shells are open at once. Every shell opened, closed, reattached or expired is written to the session log (G-3) with the client's address, name and key. |
| H-13 | Shell survives a drop | A shell whose connection is lost is kept alive for **2 minutes** so the same machine can reattach with the session contents intact; after that it is discarded. The client reattaches on its own: it retries with a widening delay for as long as the host would still hold the shell, says it is reattaching while it does, and picks the same shell back up with its contents and scrollback. Only when the 2 minutes are up does it report the connection as lost, and offer to start over. |
| H-15 | An idle shell is not a dropped shell | A terminal connection carrying no traffic is kept alive by the client, so a shell left sitting at a prompt is not mistaken for a broken link and closed. |
| H-16 | File transfer source (desktop) | The source list also offers **File transfer — files viewers send**, ticked afresh every time the list is shown and never saved; it shares the app's one UDP port (T-4), passcode and network choice like the terminal (H-11). Files land in the folder named under the picker and in the sharing status (T-25). A batch carries at most **32** files, **8 GiB** per file and **32 GiB** in total; anything larger, or a name that cannot be stored, is refused with the reason. Each file is written beside its final name with a `.deskhub-part` suffix and renamed only once it has arrived whole and its checksum matches; a damaged file is thrown away and stops the batch. Nothing is ever overwritten: a name already in the folder gets a number appended. If the folder cannot be written to, no files are taken and the host says so rather than failing silently. |
| H-17 | Transfers in the table | While file transfer is shared, the live table shows a *File transfer* row naming the folder, and each machine currently sending as a row beneath it, identified like a viewer (C-7), with the file being received, its place in the batch and the share done — or the reason the batch stopped. The *File transfer* row's *Stop* ends file transfer alone. Every batch offered, accepted, refused or finished is written to the session log (G-3) with the machine's address, name and key. |
| H-14 | Stop & attach (desktop) | Every shell row — live or waiting for a reattach — also offers **Stop & attach**: the remote client is disconnected (its window reports the shell ended) and the very same shell opens in a terminal window on the host, contents and scrollback intact. From then on the shell belongs to the host machine: the old client cannot reattach, the 2-minute limit (H-13) no longer applies, the table marks the row *attached on this machine*, and closing the host's window — or *Stop* on its row — ends the shell. The takeover is written to the session log (G-3). |

## 5. Connecting — viewing another machine

| ID | Feature | Description |
| --- | --- | --- |
| C-1 | Connect by address | The user types the host's IP address in one field and the UDP port in another, prefilled with the default `47777`. Pasting `192.168.1.10:47777` into the address field still works — its explicit port wins over the port field. Invalid input produces an explanatory hint, not a failure. |
| C-2 | Passcode entry | The passcode field may be left empty. A typed code must be exactly 4 digits, or the connect is refused before anything is sent. What the field shows is exactly what is used — nothing is filled in behind the user's back. With an empty field, the host decides: a paired machine is let straight in; an unpaired one waits (about a minute) while the person at the host is asked to let it in (S-2). A typed code the host rejects fails with a message naming the passcode. The prompt that opens from the device lists shows the device's UDP port and its remembered passcode (D-7), both prefilled and editable. |
| C-3 | Control opt-out | Before connecting, the viewer can untick *control the remote machine* to watch without sending any input. A host that accepts no input at all — a phone or tablet (P-4), or a desktop sharing with input off — says so when asked what it is sharing, and the desktop clients carry a standing note that control and terminal do nothing against such a host. |
| C-4 | Source picker | If the host is sharing more than one display, the viewer is asked which to view. Picking several opens several windows. If the host shares exactly one display, it opens immediately. |
| C-5 | Clear failures | If the host cannot be reached, is not sharing, or refuses the passcode, the viewer is told which — with the address named in the message. |
| C-6 | Session end notice | When a session ends, from either side, the viewer sees why. |
| C-7 | Viewer name | A *Your name* field on the connect page names this device. Until the user first sets a name, it is prefilled with a platform default: the computer's hostname on Windows and Linux (the login username if no hostname is available), the computer's name on macOS, the device name on iOS, and the device model on Android. The field can be edited, and whatever it contains when connecting is what is saved and sent. It can never end up unset: connecting with a cleared field falls back to the platform default above, which refills the field and is what is saved and sent, so a name always accompanies a connection. Hosts show the name next to this machine's address so viewers can be told apart. The name is remembered on this device, holds at most **64** bytes of text, and control characters are removed from it. A host running an older version simply does not show it. |
| C-8 | Open a shell | *Terminal — open a shell* is a button that appears once a host has answered (C-10), on every client. The shell opens in its own window — grid, scrollback, status line, and on phones an extra key row (Esc, Tab, latching Ctrl/Alt, arrows, ^C) — and that window states why a shell could not open (wrong passcode, refused, unreachable). Watching the screen and running a shell at the same time is normal. Every client learns what the host shares before opening anything: against a host with no terminal — a phone or tablet, or a desktop not sharing one — the button stays disabled, so no terminal window is opened at all. |
| C-9 | Send files | Every client can send files to a host that is taking them. *File transfer — send files to it* is a button that appears once a host has answered (C-10), on every client, and opens a **Send files** screen. On Android and iOS files are picked from the system photo picker or the system file browser, and a copy is staged in the app's own cache before it is sent. One batch at a time: while one is running the pickers are disabled and a second offer is refused as busy. Progress names the file being sent, its place in the batch and the share done, and the transfer can be stopped at any point. When it settles, every file in the batch is listed as sent or not sent, with the reason. Every client learns what the host shares before opening anything: against a host that is not taking files the button stays disabled, so no window opens. |
| C-10 | Connect, then choose | Connect only authenticates: it dials the host, settles pairing or the passcode (S-2), and asks what the host shares. What a host that has answered offers is the same everywhere: its address, a **Disconnect** control, the live line of V-7, and one button each for *Remote desktop — view its screen*, *Terminal — open a shell* and *File transfer — send files to it*, each enabled only if the host shares that capability. Opening a session reuses the pairing just settled, so nobody at the host is asked a second time. Where those land differs by platform (C-11). |
| C-11 | One window per host (desktop) | On Windows, Linux and macOS a host that answers opens a **connection window** of its own, titled with its address and holding everything C-10 lists. The connect page itself never changes state: the address, port, passcode and name fields, Connect and the device list stay put, so the next host can be dialled while the first is still open, and a machine can be connected to several hosts at once. Connecting again to a host that already has a window raises that window instead of opening a second. Closing a connection window, or its **Disconnect**, drops that host alone and leaves the others untouched; quitting the app closes them all. Sessions already opened from a window (V-1, C-8, C-9) are separate windows of their own and outlive it. On Android and iOS there is one connection at a time and it stays on the connect page: until Connect succeeds the page is the fields, Connect and the device list — nothing else; once the host answers those give way to what C-10 lists, and Disconnect, or editing the address, port or passcode, returns the page to the first state. |

## 6. Finding machines

| ID | Feature | Description |
| --- | --- | --- |
| D-1 | Network scan | The client scans the local network for machines that are currently sharing and lists them, with progress shown while scanning ("*n* of *m* addresses checked"). When the scan finds nothing, the user is told why a machine may be absent: it appears only while it is sharing. |
| D-2 | Scan bounds | A scan covers at most **512** addresses on the local subnet. If the machine has no local network address, the user is told scanning is not possible. |
| D-3 | Automatic re-scan | The scan repeats periodically, and can be re-run on demand via *Refresh now*. |
| D-4 | Click to connect | Clicking a discovered device starts a connection to it. |
| D-5 | Recent devices | Machines connected to before are kept in the *Devices* list — up to **10** — marked *Recent* in the *Where* column and showing address, status, ping and when they were last connected. |
| D-6 | Live status | Each recent device shows **Online**, **Offline** or **Checking…** with a round-trip time, refreshed automatically every **30 seconds** and on demand. |
| D-7 | Remembered passcode | The passcode used for a device is remembered with it and prefills the prompt when connecting from the device list — visibly, in the editable field, never silently. Connecting without typing a code does not erase the remembered one; typing a new code replaces it. It is stored obscured, which is convenience — not protection (see section 9). Once machines are paired the code stops mattering: they are recognised by key. |
| D-8 | Forget a device | A recent device can be removed from the list. |

## 7. Viewing a session

| ID | Feature | Description |
| --- | --- | --- |
| V-1 | Fit to window | The remote screen is scaled to fit the window, preserving aspect ratio, with the window sized to the source on open. On desktop, when the stream's shape genuinely changes mid-session — a phone or tablet host rotating, or a switch to a differently shaped display — the window re-fits itself to the new shape; quality changes at the same shape leave the window alone. |
| V-2 | Zoom and pan | The view can be zoomed up to **5×** and panned. Zoom level is displayed and can be reset in one action. |
| V-3 | Session status | The window shows a live status line: frame rate, bandwidth, round-trip time and end-to-end latency. |
| V-4 | Titled windows | Each viewer window is titled with the source it is showing plus its current status, so multiple sessions are distinguishable. |
| V-5 | Disconnect | The viewer can end the session at any time. |
| V-6 | Sound | Where both machines support it (section 3), the viewer hears what the shared machine is playing, in step with the picture to within about a frame. Sound is carried on its own channel: losing a packet costs a fraction of a second of audio and never disturbs the picture, and a machine playing nothing costs almost no bandwidth. It is off for a viewer that turns it off (T-23) and never sent by a host that turns it off (T-22). |
| V-7 | Connection health | The reading sits where the host was answered — the connection window on desktop, the connect page on Android and iOS (C-11) — not in the session window: it shows the host's address, a **Disconnect** control (V-5) and a live line saying it is connected, with the ping beside it, going red the moment that host stops answering. The reading comes from the same once-a-second probe that fills the device list, so it is there before a session is opened and stays there while several are, and on desktop each open host carries its own. A session window that loses its host still says it is reattaching (V-8). |
| V-8 | Auto-reconnect | A session that loses its host — the network drops, the stream goes silent — does not end. The window keeps its last picture, states that it is reattaching, and redials with backoff for up to a minute; when the host answers again the picture resumes on its own. Only after that minute, or when the host deliberately ends or refuses the session, does the window close with the reason. A shell window keeps its own two-minute reattach grace as before. |

## 8. Controlling the remote machine

| ID | Feature | Description |
| --- | --- | --- |
| I-1 | Mouse | Movement, left / right / middle / back / forward buttons, and the scroll wheel are sent to the host. |
| I-2 | Keyboard | Key presses and releases are sent, including modifier combinations. |
| I-3 | Pointer lock (desktop) | `F9` locks the mouse to the remote screen for games and other software that expects raw movement; `F9` or `Esc` releases it. The current state is shown in the window title. |
| I-4 | Focus safety | Losing focus releases the pointer lock and any keys still held down, so no key can be left stuck on the host. |
| I-5 | Touch trackpad (mobile) | On phones and tablets the video acts as a trackpad: drag moves the pointer, tap clicks, double-tap right-clicks, hold-and-drag drags, and a vertical two-finger drag scrolls. |
| I-6 | Pointer / pan mode (mobile) | A toggle switches between moving the remote pointer and panning a zoomed view. |
| I-7 | On-screen keyboard (mobile) | The device keyboard can be shown or hidden on demand and types into the remote machine. |
| I-8 | Hotkey bar (mobile) | Shortcut buttons for keys a touch keyboard makes awkward: `Esc`, `Tab`, `Enter`, the four arrows, `Del`, `Ctrl+C`, `Ctrl+V`. |
| I-9 | Host always wins | Input from the person physically at the host machine takes precedence over every remote viewer. |
| I-10 | One driver at a time | Only one viewer controls the mouse and keyboard at a time. The earliest to have joined wins contention; other viewers' input is ignored until the current driver has been idle for **1 second**. |
| I-11 | View-only enforcement | When the host has disabled control, or the viewer chose to watch only, no input reaches the host and the viewer window states that it is view-only. |

## 9. Access control and safety

| ID | Feature | Description |
| --- | --- | --- |
| S-1 | Encryption | Sessions run over an encrypted transport (QUIC/TLS). Everything a session carries — video, control, input, clipboard and terminal traffic — travels encrypted between the two machines. Discovery beacons are plain by design and carry no secrets; any other unencrypted packet arriving at the port is dropped. [`SECURITY.md`](../SECURITY.md) has the full picture. |
| S-2 | Pairing gates admission | The first time a machine connects, the host decides whether to let it in. A machine that offers the host's passcode proves it knows the code cryptographically — the code itself never travels over the network. A machine that offers no code — or a host that has none to check — goes to the person at the host instead: *Let this machine in?*, with **Allow** and **Deny**, and one answer covers everything that machine is opening (screen and shell alike). Letting it in **pairs** the two machines: from then on it is recognised by its key and connects without a passcode, until it is forgotten. A typed code is always checked, though — even a paired machine offering a wrong code is turned away. |
| S-3 | Optional passcode, paired list | The passcode is optional and empty by default; with it empty, nothing gets in without a person at the host approving it. The paired machines are listed on the **Devices** page — name, key, when paired, last seen — with *Forget* and *Forget every machine*, an *allow new pairings* switch that, when off, admits already-paired machines only, and this machine's own key for reading out. A host reveals what it is sharing only to machines it has admitted. |
| S-4 | Lockout on repeated failure | **3** wrong passcode attempts lock the host's pairing against further attempts for **30 seconds**, and the machine trying is told to wait. Already-paired machines are unaffected. |
| S-5 | Control switch | The host can share with *viewers can control this machine* turned off, making every session view-only regardless of what viewers request. |
| S-6 | Consent to capture | On platforms that require it, the operating system's own permission prompts and screen-picker dialogs are used; Deskhub cannot capture without the user granting it. |
| S-7 | Explicit sharing only | Nothing is shared until the user starts a share. Closing or stopping ends all sessions. |
| S-8 | Key change warning | A client remembers the key of every host it has trusted. If that key ever changes — the shape a machine-in-the-middle has — a loud warning shows the new fingerprint and the connection is refused until the user explicitly accepts it. A key never seen before is settled by the pairing handshake itself and asks nothing. |

## 10. Settings

Settings are per machine, persist across restarts, and apply the next time sharing
starts. Phones and tablets expose only the network port (T-4) — which also decides which
port the network scan knocks on — clipboard sync (T-17) and keep awake (T-19), plus the
passcode (T-5) and the network to share on (T-9) on their sharing screen; they host with
the built-in defaults for everything else.

| ID | Setting | Range | Default |
| --- | --- | --- | --- |
| T-1 | Frame rate | 1 – 240 fps | 60 |
| T-2 | Bitrate | 1 – 1000 Mbps | 20 |
| T-3 | Quality | 720p · 1080p · 1440p · Native | 1080p |
| T-4 | Network port | 1 – 65535 | 47777 |
| T-5 | Passcode | empty, or exactly 4 digits | empty (see S-2, S-3) |
| T-6 | Viewers can control this machine | on / off | on |
| T-9 | Share on network | All networks · one of this machine's addresses | All networks |
| T-11 | Start sharing when the app opens | on / off | off |
| T-13 | Start Deskhub when you log in | on / off | off |
| T-15 | Keep running in the background | on / off | off |
| T-17 | Sync clipboard text | on / off | off |
| T-19 | Keep this device awake during sessions | on / off | on |
| T-21 | Allow new machines to pair (Devices page) | on / off | on |
| T-22 | Share this device's sound with viewers | on / off | on |
| T-23 | Play the sound of the device you are watching | on / off | on |

| ID | Feature | Description |
| --- | --- | --- |
| T-7 | Automatic quality | Stream quality adapts on its own to the available network capacity within the configured limits; no user action is required when conditions change. |
| T-8 | Validation | Out-of-range or non-numeric values are rejected and the previous value kept, rather than applied. |
| T-10 | Network fallback | When a specific network is chosen (T-9), the host is reachable only through that address. If that address no longer exists when sharing starts, the host shares on all networks instead and says so in the sharing status. A saved address that is currently unavailable is still listed, marked *not connected*. |
| T-12 | Auto-share on launch | Desktop only. With T-11 on, opening the app goes straight to the Host page and starts sharing with the saved settings, exactly as if the user had pressed Share. When the app is launched at login (T-13) the desktop may not have any display yet; sharing then waits, re-checking every half second for up to 30 seconds, and starts the moment a display appears. Until then the Host page says it is waiting. If no display ever appears, the app shares whatever else is ticked (the terminal) or stays idle with the reason on the Host page — an automatic share never opens a dialog box, because at login the window may be hidden in the tray where nobody would see it. The platform rules still apply: Linux shows the desktop's screen-sharing dialog the first time and reuses the remembered choice after that (P-3), and macOS still requires its permissions (P-2). |
| T-14 | Launch at login | Desktop only. With T-13 on: Linux writes an autostart entry into `~/.config/autostart`; Windows registers a scheduled task named *Deskhub* that starts the app elevated at logon, so no UAC prompt appears; macOS registers a Login Item the user can also see in System Settings. Turning it off removes that artifact again. The checkbox always shows what the operating system reports, not merely what was last saved. |
| T-16 | Background mode | Desktop only. With T-15 on, a tray / menu-bar icon appears with *Show/Hide window*, *Start/Stop sharing* and *Quit*; closing the window hides the app instead of quitting it, and sharing continues in the background. The window always appears on launch and hides only when the user closes it, so T-13 + T-11 + T-15 together start sharing at login with the window shown until it is closed. On Windows, left-clicking the tray icon shows or hides the window. On macOS the Dock icon disappears while the window is hidden. On Linux the tray needs a StatusNotifier host (standard on KDE; GNOME needs the AppIndicator extension) — without one, closing the window still quits, so the app can never become unreachable. On Windows and Linux, while sharing is active, closing the window always hides to the tray even with T-15 off (when a tray is available), so connected viewers are not dropped; on macOS closing the window never quits the app, so sharing continues either way. |
| T-18 | Clipboard sync | With T-17 on, plain text copied on any machine in the session appears on the others within a couple of seconds, in both directions; the host relays a viewer's copy to the other viewers. Text is capped at 32 KiB (longer copies are cut at a whole character); images, files and formatting are never transferred. The host's toggle governs the session: with it off, the host ignores and never sends clipboard data. Each machine also needs its own toggle on to read or write its local clipboard. On Android and iOS the operating system constrains this: an Android device picks up its own copies only while Deskhub is the app in the foreground, though incoming text is applied at any time; an iOS viewer may show the system paste prompt when Deskhub reads a fresh copy; and an iOS device that is hosting does not take part at all, because its broadcast runs in a separate process without clipboard access. |
| T-20 | Keep awake | With T-19 on, the machine does not go to sleep and the display does not turn off while it is sharing or viewing; the block is released the moment the session ends, and no sleep settings are changed. On Windows, macOS and Linux this covers both display and system sleep for hosts and viewers alike (on Linux it needs systemd-logind and a desktop that honours the freedesktop screensaver interface — standard on KDE and GNOME). The operating system still wins where it insists: closing a laptop lid, pressing the power button, or macOS on battery power may still sleep the machine. On Android and iOS the toggle keeps the screen on while viewing a stream; sharing from a phone already survives the screen turning off (P-5), so hosting there does not hold the screen. |
| T-25 | Where files land | Desktop only. Files viewers send are written to a folder this machine chooses — `Deskhub` in the user's home folder unless another is picked. The chosen folder is shown beside the file-transfer tick before sharing and in the sharing status while sharing, is created if it does not exist, and is saved with the other settings. Nothing outside that folder is ever written: a sender's name is stripped to its last path element and scrubbed of anything the local filesystem cannot store. |
| T-24 | What sound is shared | With T-22 on, the host shares what its own speakers are playing — the mix every application on it produces. It never captures a microphone: Deskhub has no two-way audio, and asks for no microphone permission on any platform. A viewer only receives sound if it asked for it (T-23), so a host with T-22 on sends nothing to a viewer that is not listening, and both toggles take effect the next time a session starts. |

## 11. Status and troubleshooting

| ID | Feature | Description |
| --- | --- | --- |
| G-1 | Live host statistics | Per-display and per-viewer figures for capture rate, send rate, bandwidth and round-trip time. |
| G-2 | Live client statistics | Per-session frame rate, bandwidth, round-trip time and end-to-end latency. |
| G-3 | Session logs | On Windows, macOS and Linux each run writes a log file to the user's Deskhub folder, for attaching to bug reports. Android and iOS write their diagnostics to the operating system's own log stream instead and leave no file behind. |
| G-4 | Version and project link | The app displays its version and links to the project page. |

## 12. Platform-specific behaviour

| ID | Platform | Behaviour |
| --- | --- | --- |
| P-1 | Windows | The app asks for administrator rights once at start, which is what allows it to type into elevated windows. It adds its own firewall rule when sharing begins. |
| P-2 | macOS | Shows a **Permissions** panel with the live grant state of *Screen Recording* (needed to share) and *Accessibility* (needed to accept remote input), a button to request each, and a shortcut into System Settings. Some keystrokes are silently blocked by macOS unless Accessibility is granted. |
| P-3 | Linux | The Host page lists this machine's displays and the terminal as tick boxes, like the other desktops, and only the ticked displays are shared. The desktop still confirms the screen capture in its own screen-sharing dialog after pressing Share; that confirmation is remembered where the desktop supports it (ScreenCast portal version 4+): later shares reuse it silently, including across restarts, so the dialog appears only the first time. If none of what the desktop granted matches the ticked displays, the remembered confirmation is forgotten and the dialog is shown once more, so the user can grant the right displays; if the desktop rejects or has expired the confirmation — after a compositor upgrade or a monitor change — the dialog simply appears again, and cancelling it never retries. If the desktop grants displays the app cannot match to the ticked list, everything the desktop granted is shared rather than nothing. Ticking only the terminal skips the desktop dialog entirely. Sharing additionally requires the system to permit input injection. |
| P-4 | Android / iOS | Hosting is **view-only**: the device streams its screen and silently drops every control packet, because neither OS lets an app inject input system-wide. It shares no terminal — and says so when asked, so no client opens a terminal window against a phone (C-8) — and the desktop clients can say the control tick will do nothing (C-3). It takes files from the moment the app is on screen, under the batch rules of H-16, with no switch to find, and keeps taking them while the screen is being shared, so a viewer can watch the screen and send files to it at the same time; on iOS the broadcast extension holds the one port for the length of a broadcast and serves both from it. Photos and videos it receives are added to the device's photo library; every other file lands where the system file browser can see it (the app's Documents folder on iOS, Downloads on Android), and a notification names what arrived. The whole screen is shared as a single source, so the display picker, multi-display sharing and per-display stop (H-1, H-2, H-3, H-5) do not apply. Turning the device turns the stream with it: what viewers see stays the right way up, and their window re-fits to the new shape (V-1). The session UI is touch-first: trackpad gestures, zoom controls, hotkey bar, on-screen keyboard, display switcher, and a close button in the corner shared with the shell and file-transfer screens. |
| P-5 | Android | Sharing needs the system screen-recording consent dialog, which is granted per share and cannot be remembered. While sharing, an ongoing notification is shown and the stream survives the app going to the background or the screen turning off. Stopping the share from the system notification ends the session. |
| P-6 | iOS | Sharing is started from an in-app **Start sharing** button which opens the system broadcast sheet, because iOS requires that sheet to confirm every broadcast, and runs in a separate broadcast process so it continues after the app is closed. The sharing screen reports the number of connected viewers — listing the names of those that have set one (C-7) — and the broadcast process's current memory use — iOS ends a broadcast that grows past its memory limit — without the per-viewer table of H-7, and viewers cannot be dropped individually (H-8). A system event that ends the broadcast — an incoming call, for instance — ends the session. |

## 13. Explicitly out of scope

Deskhub does **not** provide, and this specification does not cover:

- Microphone capture, two-way audio, or any voice channel. Sound travels one way only,
  from the shared machine to the people watching it (V-6).
- Remote printing.
- Clipboard sync beyond plain text (images, files, rich text).
- Any account, directory, presence or invitation system.
- Relay, rendezvous or NAT-traversal service — reaching a host over the internet is the
  user's responsibility (for example via a VPN).
- Session recording.
- Unattended access, wake-on-LAN, or remote power control.
- Multi-user administration, roles, or audit trails.
