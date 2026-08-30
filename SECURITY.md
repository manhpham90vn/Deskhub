**English** · [Tiếng Việt](SECURITY.vi.md) · [中文](SECURITY.zh.md) · [日本語](SECURITY.ja.md)

# Deskhub Security Policy

_Last updated: August 15, 2026_

## ⚠️ Read this first

**Deskhub encrypts its sessions. Everything a session carries — video, keystrokes,
mouse, clipboard and terminal traffic — runs over QUIC/TLS, and admission is decided by
a pairing handshake: an unknown machine must either prove it knows the host's passcode
(the code itself never travels over the network) or be approved by the person at the
host.** The only traffic left in the plain is the discovery beacon, which carries no
secrets; anything else arriving outside an encrypted connection is dropped.

Every host can also share **view-only** (input is dropped instead of injected), and can
turn off new pairings entirely so only already-paired machines get in.

Encryption is not the same as being Internet-proof: the port still answers discovery
probes, a 4-digit passcode is still a small secret, the first meeting is still a leap of
faith, and nothing here resists flooding. Deskhub remains built for networks you trust.

So the rule still stands:

> **Never port-forward UDP 47777. Never expose a sharing machine to the Internet
> directly. For remote access, use a VPN — [Tailscale](https://tailscale.com) is what
> this project is tested against — and connect to the `100.x.y.z` address.**

If you follow that rule, Deskhub is safe to use. If you break it, you are handing your
machine to the Internet.

## Threat model

### What Deskhub protects against

| | |
|---|---|
| Data reaching the developer | Nothing does. There are no servers, no accounts, no telemetry, no third-party SDKs. See [`PRIVACY.md`](PRIVACY.md). |
| Someone reading your traffic | Every session runs inside QUIC/TLS — video frames, keystrokes, clipboard text and terminal bytes are all encrypted between the two machines. A packet capture yields traffic volume and timing, not content. Unencrypted packets arriving at the port are dropped unless they are discovery probes. |
| A remote viewer fighting you for the machine | "Host wins": the moment you touch the real mouse or keyboard, remote input is paused (Windows, macOS and Linux hosts alike). |
| Keys left stuck down | Any key the remote side is holding is released automatically when the session ends or the viewer switches away. |
| A stranger connecting uninvited | Admission is a pairing handshake. An unknown machine must prove the host's passcode via SPAKE2 — the code never travels, an eavesdropper takes nothing home to crack, and each connection allows exactly one guess — or, when no passcode is set, wait for the person at the host to answer *Let this machine in?*. Three wrong guesses lock pairing for 30 seconds. Once admitted, a machine is paired: recognised by its cryptographic key, listed on the host's Devices page, and revocable there. The discovery beacon no longer confirms a guessed code — a stranger's probe gets an empty list no matter what it contains, so the old brute-force oracle is gone. |
| A machine-in-the-middle on later visits | Every machine has a key. A client remembers the key of each host it has paired with and refuses to reconnect over a changed key until the user explicitly accepts it. The passcode proof is bound to the host key the client actually saw, so a relayed proof does not verify. |
| Viewers fighting each other for the mouse | Up to 5 viewers may watch one host, but only one drives input: the earliest to have joined wins, and a later viewer's input is dropped until the earlier one has been idle for a second. A 6th viewer is rejected as `Busy`. |
| A viewer you only want to show the screen to | View-only sharing, available on every host, drops input packets at the host before anything is injected — it is not enforced by asking the client to behave. Android and iOS hosts are view-only unconditionally. |
| A phone left sharing by accident | The operating system, not Deskhub, is the backstop: Android keeps a permanent notification up and re-asks for recording consent on every single share, and iOS keeps its broadcast indicator visible. Either can stop the share without opening the app. |
| A paired machine writing files onto yours | Only an admitted machine can send files, and only while the receiving machine is offering file transfer. What arrives cannot escape the folder that machine chose: the name on the wire is cut to its last path element and scrubbed of separators, control bytes, characters the filesystem rejects and reserved device names before any file is opened; each file is written under a `.deskhub-part` name and renamed only once it has arrived whole with a matching CRC-32; and a name already present gets a number rather than overwriting anything. A batch is capped at 32 files, 8 GiB per file and 32 GiB in total. The same scrubbing runs on a phone or tablet before anything reaches its photo library or its Downloads folder. |
| Malformed packets | Every field is bounds-checked before it is read. The parsers are covered by unit tests, run under AddressSanitizer, UndefinedBehaviorSanitizer and ThreadSanitizer in CI, and fuzzed nightly with libFuzzer — seven targets covering the wire format, H.264 parsing, packet reassembly, terminal byte streams, UI text, and the host and viewer session state machines. Crashes found by fuzzing are kept in-repo as regression tests, and new coverage is folded back into the seed corpus. |

### What Deskhub does **not** protect against

This is the honest list. Nothing below is solved today:

- **The first meeting is a leap of faith.** Pairing stops a machine-in-the-middle who
  arrives *later* — the key is pinned and a change is refused loudly. It cannot stop one
  who is already in the middle at the very first contact: with no passcode set, whoever
  the client reaches is who gets paired, and a passcode raises the bar only as much as a
  4-digit secret can. Compare fingerprints out of band if that matters to you.
- **Traffic analysis still works.** Encryption hides content, not existence: an observer
  sees that a session is running, how much video is flowing, and when you type.
- **No rate limiting or DoS resistance.** Flooding the port will disrupt a session; a
  no-passcode host can also be made to show approval prompts repeatedly.
- **The discovery beacon still answers anyone.** A `LIST_SOURCES` probe or a `PING` gets
  a reply from any source address — a stranger's reply is an empty list, and no probe can
  confirm a passcode any more, but the machine is still discoverable by scanning and the
  port is still usable as a small UDP reflector. One exception: a source address that
  currently holds an encrypted connection is never answered in the plain — once a machine
  has proved itself, everything it says must arrive encrypted, so a forged plaintext
  `SOURCE_LIST` or `PONG` cannot impersonate a connected peer.
- **The device name is shown and logged.** The *Your name* a viewer sends is encrypted in
  transit now, but it is still shown on the host's screen, written into the host's logs
  and stored in the host's paired-devices list. It defaults to the machine's own
  hostname — often the owner's real name. Use a nickname; never put anything sensitive
  in it. Clearing the field does not stop a name being sent; it only restores the
  default.
- **A viewer slot frees itself after 5 seconds of silence.** If your viewer drops off,
  its slot reopens and the next `Hello` to arrive takes it — whoever sent it, subject
  only to the passcode.
- **Sharing exposes the entire display.** Not one window: every notification, popup and
  window on that monitor. See [`PRIVACY.md` §3.4](PRIVACY.md).
- **A phone or tablet host exposes the whole phone.** Android and iOS can host too, and
  what they stream is the entire screen — banking apps, one-time codes, messages, every
  password you type while sharing. The stream is encrypted like any other session, but
  every viewer you admit sees all of it. Mobile hosts are always view-only, which removes
  the remote-control risk but none of the exposure risk.

## Where it is safe to run

✅ **Safe**

- A home or personal LAN where you control every device on it.
- A Tailscale tailnet (or another WireGuard/VPN tunnel) that only your own devices have
  joined. The VPN adds a second layer of encryption and keeps strangers from reaching
  the port at all.
- A machine that is only ever a *client* (phone, tablet, laptop that never shares its
  screen). Clients accept no inbound sessions.

❌ **Unsafe — do not do this**

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
machine. On Windows the app runs elevated from the moment it starts (it asks once, so
that it can inject input into elevated windows) and opens the firewall rule for you
when you share — the rule covers the whole app on every profile, so a narrowed bind
does not narrow the firewall; that convenience is also what makes the rule above
matter.

## What an attacker on the same network can do

If someone is on the same LAN as a machine that is sharing its screen, and Deskhub is
running, they can:

1. Discover it by scanning for UDP 47777. An unpaired machine's probe gets an empty
   list, but the machine still answers, so it still gives itself away.
2. Try to get in. They can no longer read the passcode off the wire — it never travels.
   What is left is guessing it online (one guess per connection, three wrong guesses
   lock pairing for 30 seconds) or, on a host with no passcode, hoping the person at the
   host clicks **Allow** on the approval prompt.
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
- [ ] Decide how machines get in: set a 4-digit passcode in Settings, or leave it empty
      and answer the approval prompt yourself. Review the Devices page now and then and
      forget machines you no longer recognise. Untick *Viewers can control this machine*
      whenever you only need someone to watch.
- [ ] Quit Deskhub when you are not actively using it. It does not run as a background
      service — closing it closes the hole.
- [ ] On Linux, if you use `ufw`, scope the rule instead of opening it wide:
      `sudo ufw allow from 100.64.0.0/10 to any port 47777 proto udp` rather than
      `sudo ufw allow 47777/udp`.
- [ ] Do not leave a share running on a laptop that you carry onto other networks.
- [ ] Lock your machine when you walk away, so an unattended session cannot be taken
      over silently.
- [ ] With `deskhub-cli`, do not put the passcode in the command itself. `--passcode 0417`
      is visible to every process on the machine through `ps` and `/proc/*/cmdline`, and
      it lands in your shell history. Use `--passcode -` to read it from standard input,
      `--passcode @FILE` to read it from a file only you can read, or set
      `DESKHUB_PASSCODE` in the environment.

## Local artifacts

Diagnostic logs are written in plain text under `~/.deskhub/` (`%USERPROFILE%\.deskhub`
on Windows) on Windows, macOS and Linux. They contain connection statistics and peer
addresses, not screen content or keystrokes.

The desktop apps and `deskhub-cli` share those files. They keep more in that folder: `ui-settings.txt` (fps, bitrate,
resolution cap, ports, the view-only and pairing switches, your host passcode if you set
one, and the device name shown to hosts), `recent-devices.txt` (the last 10 addresses you
connected to, when, and the passcode used for each), `host_key.pem` + `host_cert.pem`
(this machine's private key and self-signed certificate — the identity behind its
fingerprint; anyone who copies the key file can impersonate this machine), `known_hosts`
(the keys of hosts this machine has trusted), `paired_devices` (the keys, names and
timestamps of machines allowed into this host), `auth_salt` (a non-secret salt for the
passcode verifier) and, on Linux, `portal-restore-token.txt` (the desktop's own token for
the screens you picked, meaningful only to your desktop session and never transmitted).
The mobile apps keep their settings inside their own sandbox — on iOS in the app group
container. Stored passcodes are obfuscated with a fixed XOR key, which
keeps them off the screen and out of a casual `type` of the file — **it is not
encryption**, and anyone with the source and the file recovers them in seconds. Treat
that folder as readable by anything running as you.

Files another machine sends land outside that folder, in the directory the receiving
machine chose for them (`Deskhub` in the user's home folder unless another is picked,
saved as `transfer_dir`). On a phone or tablet they end up in the device's photo library
or its Documents / Downloads folder, where they survive uninstalling the app. Treat
anything delivered there as a file a paired machine put on your device.

Nothing uploads any of this; delete the folder at any time.

## Planned mitigations

Tracked, in the order they are intended to land:

1. **Storing the passcode and the host key in the OS keychain** instead of files.
2. **Silencing the discovery beacon** so it does not reply at all to an unsolicited
   probe, rather than replying with an empty list.

Shipped since the last revision of this list: an encrypted transport (QUIC/TLS) for the
whole session — video, input, clipboard and terminal alike — with unencrypted arrivals
dropped unless they are discovery probes; SPAKE2 pairing so the passcode never travels
and cannot be harvested or brute-forced offline; the approval prompt on the host; a
paired-machines list with revocation; machine keys with a key-change warning on the
client; and a 3-strikes / 30-second lockout on wrong passcode guesses.

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

**Already documented above is not a vulnerability.** The limits listed above — the
first-meeting leap of faith, traffic analysis, the beacon answering probes, the lack of
DoS resistance — are known and listed; a report restating one of them tells us nothing
new. What *is* worth reporting: memory
corruption or crashes reachable from a malformed packet, a way to escape the documented
threat model, anything that leaks data off the machine, or a flaw in a mitigation once
one ships.

## Supported versions

Only the most recent release on the [Releases page](https://github.com/manhpham90vn/Deskhub/releases)
is supported. Fixes ship in a new release; there are no backports to older versions.

## Scope

This policy covers the Deskhub source in this repository and the binaries published on
the Releases page, TestFlight and Google Play. It does not cover Tailscale, your
operating system, your router, or any other software you run alongside it.
