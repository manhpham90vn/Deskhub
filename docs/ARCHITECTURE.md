**English** · [Tiếng Việt](ARCHITECTURE.vi.md) · [中文](ARCHITECTURE.zh.md) · [日本語](ARCHITECTURE.ja.md)

# Deskhub — Architecture

This document describes **how** Deskhub is built: the layers, the processes and
threads, the wire protocol, and the design decisions behind them. What the product
does, as a user sees it, lives in [`SPECIFICATION.md`](SPECIFICATION.md); the threat
model lives in [`SECURITY.md`](../SECURITY.md).

- **Status:** describes the current code.
- **Audience:** anyone changing the code.

---

## 1. Layers

One rule drives the whole layout: logic is written once and shared by every client.

```
core/       pure C++20, no OS headers, no third-party code, unit-tested offline
platform/   thin OS abstractions behind one identical API per header (depends on core)
client/     per-OS apps: windows, linux, macos, ios, android (depend on platform + core)
            plus client/cli, one command-line client for all three desktops
```

| Layer | Contents |
| --- | --- |
| `core/protocol` | Wire format (`Wire.h`), record framing for streams (`RecordStream.h`), packet classifier that tells QUIC from Deskhub beacon datagrams |
| `core/transport` | Packetizer/Reassembler for video, FEC, retransmit cache, send pacer |
| `core/session` | Session state machines, split by role: `session/host` (per-viewer sessions, viewer table, beacon, file receiver, auth throttle), `session/client` (screen client, file sender, terminal client, connect flow), and shared pieces beside them (transfer types, terminal session table, clipboard sync, link recovery) |
| `core/control` | Bitrate controller, quality ladder, stream sizing, clock offset |
| `core/terminal` | The VT emulator every client shares: `VtParser`, `Screen`, `KeyEncoder`, `Palette` |
| `core/net` | Trust store (client side), paired devices (host side), bind-address selection, LAN scan logic |
| `core/ui` | Every user-visible string, settings parsing, table-row builders — so all five clients say the same things |
| `platform/net` | `UdpSocket` (per-OS), `QuicEndpoint` (quiche behind a pimpl), `SessionTransport` |
| `platform/auth` | `AuthNegotiation` — the one pairing/passcode handshake both sides speak |
| `platform/client` | `HostLink` (dial + trust + auth + channels, shared by every surface), `ScreenViewer`, `TerminalViewer`, `FileTransferClient`, `SourceQuery`, host probe, LAN scanner |
| `platform/host` | `HostEngine`, `HostNetLoop`, `SharingHost`, `TerminalHost`, `FileHost`, `ViewerBroadcast` |
| `platform/system` | Clock, random, PTY (ConPTY / forkpty), host identity (keys), trust/paired-device files, autostart, keep-awake |
| `core/cli` | The command-line grammar and its JSON writer — pure text in, validated command out |
| `client/<os>` | Capture, encode, decode, render, windowing, dialogs — nothing protocol-shaped |
| `client/cli` | Flags to sessions: one binary that hosts, connects and opens shells with no GUI toolkit. It links the same per-OS media library the desktop app does |

`core/` must stay testable offline with no network and no GPU. `platform/` may touch
the OS but must expose one identical API everywhere. If the same code appears in two
clients, it belongs in a lower layer.

## 2. One port, one transport

Everything a host offers rides **one UDP port** (default 47777) through one
`SessionTransport`, which wraps a single `QuicEndpoint`:

```
                      UDP port 47777
                            |
                 ClassifyPacket (first byte)
                   /                    \
            QUIC packets          Deskhub datagrams
                 |                        |
   +-------------+------------+       beacon only:
   |             |            |       LIST_SOURCES / PING answered
 streams     datagrams     (TLS)      in the plain; every other
   |             |                    raw packet is dropped
 control      video
 input        audio       Streams carry framed records (RecordStream):
 clipboard                length-prefixed messages up to 16 KiB.
 terminal                 Datagrams carry one video or audio packet
 files                    each (≤ 1200 B).
```

- **Streams** (reliable, ordered): control, input, clipboard, terminal, files — each
  connection uses one bidirectional stream, opened by the client. A stuck stream on
  one connection cannot stall another connection. Inbound stream data is drained
  under a 64 KiB budget per service pass: whatever consumes it (the terminal's VT
  emulation above all) hands the loop back to ACKs, keepalives and timeout
  processing between slices, so a `cat` storm can no longer starve the connection
  into its own idle timeout.
- **Datagrams** (unreliable, unordered, still encrypted): video and audio packets.
  Lost ones are never retransmitted by QUIC; for video the app's own FEC/NACK
  machinery handles loss, and for audio nothing does — see section 9.
- **Raw UDP** exists only for discovery: the beacon answers scanners that speak no
  QUIC, and probes it did not invite get an empty source list. Inbound raw packets
  that are not discovery types are discarded before they reach any session code.

`QuicEndpoint` hides quiche completely (pimpl; `QuicEndpointNone.cpp` stubs it out,
but only when a build opts out with `-DDESKHUB_QUIC=OFF` — a missing quiche fails the
configure, because a stub binary cannot share or connect). Connections are identified by peer address; there is no
connection migration. A quiche connection is single-threaded by contract, so every
touch of the endpoint happens under the transport's send mutex — and the transport
never holds that mutex across a blocking socket wait (`WaitReadable` first, unlocked;
then a brief locked `Poll`). Holding it across the wait starves every sender.

## 3. Admission: pairing

Every machine creates an ECDSA P-256 key on first run (`HostIdentity`); its SHA-256
SPKI hash is the fingerprint people see. TLS uses a self-signed certificate over that
key. On top of TLS, an application-level handshake (`AuthNegotiation`) decides
admission per connection. The transport runs it and drops every message from a
connection whose auth has not settled:

| Client offers | Host knows the machine | Result |
| --- | --- | --- |
| nothing | paired | **Signature**: client signs a nonce+host-fingerprint transcript with its key. In silently. |
| nothing | unknown | **Approval**: the person at the host is asked (*Let this machine in?*). |
| a passcode | host has one | **Passcode**: SPAKE2 over a salted verifier — the code never travels, one guess per connection, both sides prove it, MACs are bound to the host key the client actually saw (kills relays). A typed code is always checked, paired or not. |
| a passcode | host has none | nothing to check against → Signature if paired, Approval otherwise. |
| anything | pairing switched off | **Denied** (paired machines still get Signature). |

Success writes the client into the host's `paired_devices`; pairing is by key, not
address. Three wrong passcode guesses lock the passcode path for 30 seconds
(`AuthThrottle`, shared constants with the legacy session lockout); the approval path
needs no throttle — a human is the gate.

Client side, `known_hosts` (`TrustStore`) pins host keys. A **changed** key blocks the
connection behind a loud warning; an unknown key is settled by the handshake itself
(a host that proved the passcode is remembered without a prompt).

The wire carries the public key itself, never a bare fingerprint — the host hashes
what it receives, so wearing someone else's identity would mean signing with a key
the impostor does not hold. And because admission settles once per connection,
nothing above the transport ever asks again: a machine that proved itself carries no
passcode in any later message, and session code treats the whole connection as
authenticated.

## 4. Host side

```
HostEngine (one per app, owns SessionTransport)
 ├─ net-loop thread: RunHostNetLoop
 │    recv → beacon replies | video-path ingest | Chan::Terminal → TerminalHost
 │    per-source session Tick, clipboard flush, reconfig, stats
 ├─ capture/encode: per-source, driven by the OS capture callbacks (client layer)
 │    frame → encoder (per-source mutex) → Packetizer → FEC → SendTo (datagrams)
 ├─ audio worker: capture callback → lock-free frame ring → Opus encode →
 │    per-viewer datagrams (AudioBroadcaster)
 └─ TerminalHost (tenant, when the terminal is shared)
      ├─ HandleMessage on the net-loop thread: TERM_OPEN/DATA/RESIZE/CLOSE → PTY
      └─ pump thread: PTY output → host-side Screen mirror + TERM_DATA records,
           expiry, kicks
```

- The engine runs whenever anything is shared. With zero screen sources and the
  terminal ticked it runs source-less; the loop stays alive while the terminal does.
- Each screen source is a `SourcePipelineState`: its own `ScreenHostSession` (viewer table,
  negotiation, input arbitration), encoder, quality ladder and diagnostics. One
  encode feeds every viewer of that source.
- The feedback loop: viewers send `Feedback` (loss/RTT) once a second, and the host adds
  one signal of its own — the age of a frame when it reaches the sender, the same
  quantity `enc_lat_ms` reports. `BitrateController` (AIMD) and `QualityLadder` adjust
  encoder bitrate, resolution and fps from all three; FEC is armed from the first frame
  and only stood down after a long clean run, because the loss it protects against shows
  up before the first report does — a backlog never arms it, since parity would only
  deepen the queue. quiche's CUBIC congestion control sits underneath the
  datagram path; the two act in series — quiche bounds what leaves the machine, the
  app adapts the encoder to the loss that results.
- Input: "host wins" — `LocalInputMonitor` pauses remote input while the person at
  the machine moves their own mouse; one viewer drives at a time.
- Shells: one PTY per shell (`ConPTY` on Windows, `forkpty` elsewhere), at most 8; a
  dropped connection detaches the shell and keeps the PTY alive for 2 minutes so the
  same machine can reattach. Every open/close/detach/reattach is audit-logged with
  address, name and key.
- Every shell's output also feeds a host-side `core/terminal` Screen from the moment
  it starts. *Stop & attach* disconnects the remote client and opens that mirror —
  scrollback intact — in a terminal window on the host; a shell taken over this way
  belongs to the host, never expires, and ends when the host's window closes.

## 5. Client side

Every client surface reaches a host through the same piece, `HostLink`
(`platform/client/HostLink`): it dials the QUIC connection, checks the trust store,
runs the auth handshake, keeps the link alive, and — for surfaces that ask for it —
redials with backoff when the link drops. No service dials or authenticates on its
own any more; a service opens a channel per wire `Chan`, gets its own inbox queue,
and drains it on its own thread:

```
HostLink (one per open surface)
 ├─ link thread: dial → trust check → auth → pump
 │   (routes inbound records and datagrams by Chan into per-channel
 │    queues; link pulse; redial with backoff where recovery is on)
 ├─ Chan::Control/Video/Audio ─> ScreenViewer
 │    ├─ net thread: HELLO/negotiation, video ingest (Reassembler+FEC),
 │    │   NACKs, feedback, clipboard
 │    └─ decode thread: decoder + render queue
 ├─ Chan::Terminal ─> TerminalViewer service thread
 │    ├─ core/terminal Screen holds the grid
 │    └─ UI polls Snapshot(), posts keys into a command queue
 └─ Chan::File ─> FileTransferClient service thread (FileUpload ring)
```

Once admitted, the link takes its own pulse (`core/session/LinkPulse`): a
`Ping` datagram with session id 0 goes out once a second, the host's beacon answers
it over the same connection with no session required, and the echoed timestamp
becomes a smoothed RTT while the ids of pongs that never came back become a loss
percentage. `ClassifyLinkQuality` folds the two into Good / Fair / Poor for the
device list and the panel that answered the host — a window of its own on desktop, the
connect page on Android and iOS — the session windows no longer carry it —
`HostLink` hands the reading out through `onPulse` and `Pulse()`, and
because a ping is ack-eliciting it doubles as the keepalive; the plain keepalive
timer only still matters while the link is parked in `Deciding`. A host too old to
answer session-0 pings simply leaves the reading at Unknown — nothing regresses.
On a recovering link the pulse is also the liveness check: five seconds without a
pong (only ever after a first pong proved the host answers) drops the connection
into the existing redial path.

The screen viewer now opts into that recovery like the terminal always has: a
dropped or silent link, or a session that stops receiving for five seconds, parks
the window in `Reattaching` (the last frame stays up, the status line flips to the
reattaching text) instead of ending it. `HostLink::RequestRedial` forces the
redial when the session noticed first, and once the link is readmitted the viewer
re-runs `HELLO` with the same client id — the host rebinds the viewer slot — and
streaming resumes off the fresh keyframe. After sixty seconds
(`kViewerReattachGraceUs`) without getting back in, the window ends with the usual
reason.

The source query (`QuerySources`) rides the same link in a one-shot, blocking form.
The UI still posts intents (keys, resize, accept-fingerprint) into command queues;
a changed host key parks the link in `Deciding` until the person accepts or rejects
it. The terminal window never parses escape sequences — `core/terminal` turns the
byte stream into a cell grid, and the window only draws cells and forwards key
events. Today each window still holds its own link; sharing one admitted link across
every window aimed at the same host is the intended next step, and it slots in at
`HostLink` — a registry and observer fan-out — not as another handshake.

## 6. Discovery

The beacon answers `LIST_SOURCES` and `PING` as plain UDP so a scanner can sweep a
subnet without 254 TLS handshakes. A stranger's reply is an empty list; the real
source list is revealed only over an admitted connection. That answer also carries
what the host can do — whether it takes input, whether it shares a terminal — in the
`SOURCE_LIST` header flags, so a client knows before it opens any window that a phone
can only be watched. A host from before the flags existed sets none of them. Recent devices, their
online state (ping/pong probes) and the LAN scan feed one merged device list, built
by `core/ui/DeviceRows` and shown by all five clients.

## 7. Data on disk

Everything lives in the user's Deskhub folder (`~/.deskhub`,
`%USERPROFILE%\.deskhub`): `host_key.pem` + `host_cert.pem` (identity),
`known_hosts` (hosts this machine trusts), `paired_devices` (machines this host
admits), `auth_salt` (non-secret verifier salt), `ui-settings.txt`,
`recent-devices.txt` (addresses + obscured passcodes), `portal-restore-token.txt` on
Linux (the desktop's own token for the screens picked in its screen-sharing dialog),
and per-run logs. File I/O
stays in `platform/`; the parsing and the data structures live in `core/` and are
unit-tested.

Files a viewer sends land somewhere else entirely: a folder the host picks
(`ui-settings.txt`'s `transfer_dir`, defaulting to `Deskhub` in the user's home
folder). `FileStore` writes each one as `<name>.deskhub-part` and renames it only
after the whole file has arrived with a matching CRC-32, so a half-written file never
appears under its real name, and `UniqueFileName` guarantees nothing is overwritten.
The name on the wire is scrubbed by `core/`'s `SafeFileName` — path separators,
control bytes, characters Windows rejects and reserved device names all go — before
`platform/` ever touches the filesystem.

## 8. Testing

| Suite | Runs | Covers |
| --- | --- | --- |
| `make test` | offline, no sockets | all of `core/`: wire, framing, FEC, sessions, VT emulator, settings, strings, deterministic structured fuzzing |
| `make test-platform` | loopback sockets | real QUIC handshakes, SPAKE2 end-to-end, terminal host + viewer over the wire, PTY against a real shell, lockout, approval |
| `make test-integration` | loopback, fake capture/encode | full host↔client sessions: negotiation, video across the wire, input, passcode/approval gating, junk resistance, and lag under cross-load — a file transfer, a flooded terminal and keystrokes beside a live stream, each gated on its worst observed stall |
| fuzz targets | 30 s per target on every PR, 15 min per target nightly | parsers for wire, H.264, reassembly, terminal bytes and UI text, plus the host and viewer session state machines |
| `make test-perf` | release build, offline + loopback | the hot paths measured rather than only exercised: `core_perf` covers the pure-C++ paths, `platform_perf` covers real QUIC over loopback; both fail on allocations per unit, on the cost at 4× the input, and on drift against a baseline recorded on that machine |

`platform_tests` and `integration_tests` each keep their own app data directory on every
operating system — the host key, the trusted-host list and the pairing file are single
shared files, so a suite that read the developer's home would be racing the installed app
and every other Deskhub process on the machine.

CI additionally enforces clang-format and clang-tidy (both pinned), SwiftLint
`--strict`, Android Lint, actionlint + shellcheck, ASan/TSan runs of all three suites,
CodeQL over C++/Kotlin/Swift, a gitleaks sweep of the whole history, and ≥ 90 % line /
80 % branch coverage on `core/`. The three suites are additionally cross-built and run
on arm64 Linux, an Android emulator and the iOS Simulator, and a Windows job runs the
integration suite three more times per round, hunting an intermittent stack corruption
in `DrainStreams` that shows up in about one run in three. The Linux and macOS release
jobs also run `core_perf` and `platform_perf` with their allocation and scaling gates
(no time baseline exists on a shared runner), and each pull request additionally gets
a perf-and-lag report posted as one self-updating comment: both perf suites A/B'd
against the base commit on the same runner (drift as warnings, never a failure), the
under-load integration numbers from the pull-request build, and the core coverage
line.

## 9. Decisions worth remembering

The A1 bake-off behind several of the FEC decisions below is written up for readers outside
the project in [`docs/posts/fec-under-burst-loss.md`](posts/fec-under-burst-loss.md), with the
raw CSVs it quotes checked in beside it under [`docs/data/bake-off/`](data/bake-off/).

- **A capability probe that returns false can switch off a whole control loop**: the
  Media Foundation encoder answered `SetBitrate` with `false` whenever the MFT did not
  expose `CODECAPI_AVEncCommonMeanBitRate`, and `ApplyFeedback` correctly treats a refusal
  as "nothing committed". On an Intel Quick Sync MFT that reports `MeanBitRate: NOT
  SUPPORTED`, the result was a host that never changed bitrate at all: measured on this
  hardware, 30 s of sustained 29-40 % loss produced zero `Bitrate` decisions, so the
  quality ladder never moved either. The startup log said `NOT SUPPORTED` the whole time
  and nobody read it as "adaptation is dead". `SetFps` and `RequestKeyFrame` in the same
  file already fell back to `ReinitTransform()`; `SetBitrate` was the one that gave up,
  and it now falls back the same way — `ConfigureTransform` writes `MF_MT_AVG_BITRATE`
  from `cfg`, so a rebuild applies the new rate. The rebuild costs an IDR, which is why
  the live `codecapi` path is still tried first. When a per-device capability gates a
  control input, make the fallback mandatory: degrading to "slower" is a choice, silently
  degrading to "never" is not.

- **A sender that cannot keep up looks exactly like a clean link**: every input
  `BitrateController` had — loss, RTT, receive rate — comes from the viewer, so nothing
  in the loop could say "I am the one falling behind". Measured on a Pixel 4 hosting for
  two viewers: frames left the encoder 15 s stale while the viewers reported 0 % loss and
  15 ms RTT, and the controller read that as headroom and walked the bitrate back up to
  its 20 Mbps ceiling — bufferbloat inside the sender, where the cleaner the link looks
  the harder it pumps. The host now measures frame age at the send step and feeds it in
  beside the viewer's numbers: past `kBacklogMs` it backs off like 2 % loss, past
  `kSevereBacklogMs` like 5 % loss, and either one blocks the ramp-up for the usual two
  seconds. Bitrate is still the only control variable, so the `QualityLadder` steps down
  behind it and the fps cap follows. Any control loop fed only by the far end is blind to
  the half of the pipeline it actually owns.

- **Capping fps only helps where something drops the frame**: the ladder's fps rung is a
  request, and each platform has to honour it somewhere frames can be thrown away.
  Windows and Linux gate at capture with `FrameGate`; Android caps MediaCodec's input
  with `max-fps-to-encoder`; macOS reconfigures ScreenCaptureKit's frame interval. iOS
  had nowhere: ReplayKit delivers at screen rate and `VtEncoder::SetFps` only sets
  `kVTCompressionPropertyKey_ExpectedFrameRate`, a rate-control hint that does not drop
  anything. A rung change there re-tuned the encoder and changed nothing about how many
  frames it had to swallow. `OfferVtFrame` now runs the same `FrameGate` for both Apple
  apps, after the idle-flush cache is refreshed so a still screen still has a frame to
  re-send. When a knob exists on every platform, check what each one does with it before
  trusting the ladder.

- **The send pacer must stay well above the encoder's own output rate**: `Pacer::Gate`
  sleeps on whichever thread `SendEncodedFrame` runs on, and on Android that is
  MediaCodec's drain loop — the same loop that must call `releaseOutputBuffer` before the
  encoder can hand over the next frame. Pacing therefore sets the drain rate, not just
  the wire rate, while the VirtualDisplay keeps pushing new frames in at screen rate.
  Narrowing `kPacingRateMultiple` from 2 to 1.2 to smooth send bursts was measured on a
  Pixel 4: burst per frame went from 20 ms to 63 ms median, and the encoder backlog grew
  without bound — `enc_lat_ms` climbed past 46 s in 100 s, and the viewer sat 4.6 s
  behind. At 2 the same run held `enc_lat_ms` at 0. The headroom is not slack to reclaim;
  it is what keeps the encode pipeline draining faster than it fills. Attack send bursts
  with socket buffers or by moving pacing off the drain thread, never by tightening this
  number.

- **The perf suite gates on cost, so a second gate has to watch outcome**: `core_perf`
  measures allocations per packet and how time scales with input, and every one of its
  reassembler workloads passed while a single lost packet was costing 22 % of the intact
  frames on a real link. It could not have caught it: discarding good video is *cheaper*
  than decoding it, so the broken policy scored better on every number the suite watches.
  `LossGoodputTests` is the companion that fails when the code does less work than it
  should — a simulated tail-loss link with a real round trip, gating on the fraction of
  frames whose packets all arrived that actually reach the decoder, and on the longest
  gap between two delivered frames. Both are machine-independent, so they hold on a
  laptop, a CI runner and a phone alike. Reach for a goodput gate whenever a policy can
  "succeed" by throwing work away.

- **A lost packet costs one frame, not the whole picture until the next keyframe**: the
  reassembler used to arm `waitingForIdr_` on every loss, so a single missing packet
  threw away every *complete* frame that followed until a fresh IDR arrived. Measured on
  a phone host over Wi-Fi, that turned 64 genuinely incomplete frames into 381 discarded
  ones — 6.4 MB of decodable video binned, and a picture frozen for a median of 146 ms
  and up to 1.4 s at a time. Now only the incomplete frame is dropped; the frames behind
  it go straight to the decoder, which conceals the missing reference while
  `InvalidateRef` names the bad frame to the host and the keyframe request repairs it.
  Brief macroblock artifacts are the deliberate price of not freezing. `waitingForIdr_`
  survives for the one case it was right about: a viewer joining mid-stream has no
  reference at all and must wait for the first IDR.

- **The stall window has to outlast a retransmit, or NACK is decoration**: a frame used
  to be given two frame intervals (33 ms at 60 fps) before it was declared lost, while
  the measured RTT on the same link was 24-49 ms. The NACK went out and its answer
  arrived after the frame had already been binned — visible as `late_ms_avg=24` with 87
  packets per second landing on frames that no longer existed. `StallTimeoutUs` now takes
  the larger of the paced window and one-and-a-half round trips, still capped by the hard
  timeout, so retransmission is worth asking for on exactly the links that need it.

- **The performance suite gates on allocations and shape, not on milliseconds**: the
  three test suites build debug, and CI runs them again under ASan, TSan and coverage,
  where a wall-clock budget measures the sanitizer rather than the code. So `core_perf`
  (release preset, `make test-perf`) fails on two machine-independent things —
  allocations per packet, frame or KB, counted by replacing the global `operator new`,
  and a `-scaling` row whose time grows far faster than its input — and keeps the timing
  half as a comparison against `out/perf/baseline.txt`, recorded per machine by
  `make perf-baseline` and never committed. That split is what lets the suite fail a
  regression like "the reassembler now copies every piece twice" on a laptop, a CI
  runner or a phone alike, while still printing ns per unit and MB/s for the paths where
  the number itself is the point. CI runs those two machine-independent gates on the
  Linux and macOS release jobs; Windows only builds the binary, because MSVC's deque
  allocates a block per element for anything larger than 16 bytes, so the same code has
  a different allocation count there. Pull requests also get a timing comparison the
  shared-runner noise cannot invalidate — base commit and pull request measured on the
  same runner, 50 % tolerance, warnings only. `platform_perf` extends the same gates to
  real QUIC over loopback, where wall time measures the service-loop cadence — the 64 KiB
  stream-drain budget times the 1 ms poll tick — so a shrunken budget, a drain that stops
  scaling linearly, or a new allocation in the poll loop all show up as a jump even
  though the CPU cost of the same work would barely move.

- **`FileHost` never sends while holding its own lock**: the QUIC service loop runs
  `QuicEndpoint::Poll` under `SessionTransport::sendMutex_`, and a connection that closes
  there calls straight back into `FileHost::OnPeerGone`, which takes `FileHost::mutex_`.
  So `sendMutex_ -> mutex_` is fixed by the transport. Any path that took `mutex_` first
  and then sent — `FileReceiver` emitting an accept, an ack or a cancel through
  `hooks.send` — closed the cycle, and TSan caught it as a lock-order inversion between
  the receive loop and a UI thread flipping `SetAccepting(false)` on a live transfer.
  Records the receiver emits are therefore queued into `outbox_` under `mutex_` and sent
  only after it is released, with `outboxMutex_` held across both halves so a peer still
  sees them in the order they were produced. `OnPeerGone` cannot send at all: it already
  runs under `sendMutex_`, so it drops whatever it queued.

- **The command-line client is a fourth front-end, not a second implementation**: it
  parses flags in `core/cli`, then drives exactly the pieces the desktop apps drive —
  `SharingHost` to host, `ScreenViewer` to watch, `TerminalViewer` to open a shell. The
  only thing it owns is a window: X11 + EGL on Linux, the desktop app's own `RunViewer`
  on Windows. That is why each client's `cpp/` tree is a static library
  (`deskhub_linux_core`, `deskhub_win_core`, `deskhub_win_view`, `deskhub_mac_core`) and
  the GUI code sits above it — the split exists so the CLI can link the media pipeline
  without linking GTK or wxWidgets.

- **`preflight` runs only when there is a screen to capture**: every client uses it to
  check the capture path — the xdg portal on Linux, the Screen Recording grant on macOS,
  a D3D11 device on Windows. A share that carries only a shell needs none of that, so
  asking anyway turned `share --terminal` on a headless box into "the screen-capture
  permission is gone". `HostEngine::Start` now skips it when the source list is empty.

- **A host with a shell and no screen stays alive**: the net loop ends a session once no
  source is alive, and a terminal-only share has none by definition. `keepAlive` answers
  from the caller's intent (`ShareOptions::terminal`), not from a `TerminalHost` pointer
  that is only attached after the loop is already running.

- **The frame gate counts to a deadline, not from the last frame it kept**: a compositor
  that hands over 40 fps against a 30 fps target has no frame at all on most of the
  33 ms boundaries, so a gate that only asks "is this far enough after the one I kept?"
  rejects every second frame and settles at 20 fps — under target, and ragged, which is
  judder rather than a slower stream. `FrameGate` carries a running due time instead:
  admitting advances it by exactly one interval, so the remainder is kept and 40 in
  gives 30 out. A capture slower than the target is never decimated, and a due time
  that has fallen behind real time resyncs rather than banking a burst, so a quiet
  spell cannot buy one later.

- **The Linux host encodes on its own thread, and hands that thread the small frame,
  not the big one**: encoding inside PipeWire's `process` callback throttled capture to
  `1000 / enc_ms` fps and turned every encode-time wobble into frame-cadence jitter on
  the client. The encode now runs on its own thread, fed through `FrameMailbox`, a
  latest-wins single-slot queue — when the encoder falls behind, the freshest frame
  wins and the stale one is counted, not queued. What crosses the queue is the frame
  already downscaled to encode size, roughly a seventh of the bytes. Copying the
  full-resolution frame across instead cost far more than the copy itself: 20 MB of
  cache lines left dirty in the capture core, which the encode core then had to pull
  across, measured at 16 ms against 3.4 ms for the same read of memory it did not own.
  The capture thread has to touch every source pixel once regardless, so it is the
  right place to spend that single pass. Dma-buf frames still encode inline: the
  compositor reuses their backing memory the moment the callback returns, so they
  cannot outlive it, and VA-API scales them on the GPU anyway.
- **The Linux host picks its encoder by where the frame lives, not by what is
  installed**: a dma-buf frame goes to VA-API, which can import it zero-copy on the
  GPU that produced it; a mapped (CPU) frame goes to NVENC when an NVIDIA driver is
  present, because on a desktop rendered by an NVIDIA GPU the compositor renegotiates
  screencast to shared memory, and the encode then belongs on the card that can take
  the pixels straight from system memory. `HwEncoder` makes that call per encoder
  rebuild, and a frame of the other kind arriving later returns `false`, which is the
  signal to rebuild.
- **The downscale ahead of NVENC is ours, not swscale's**: NVENC takes packed 32-bit
  pixels but will not resize them, and the capture is a full-resolution desktop.
  `libswscale` measured 9.2 ms for 3440x1440 → 1280x534 — roughly 2 GB/s, an order of
  magnitude off this machine's memory bandwidth, because a packed-RGB rescale falls off
  its optimised paths. `RgbDownscale` in `core/` is an area average written for exactly
  this shape: one 32-bit load per source pixel, integer accumulation, 4.0 ms for the
  same frame, and correct antialiasing rather than the bilinear tap swscale was giving.
  Whole-frame NVENC cost lands at ~5 ms, so 60 fps has headroom.
- **Performance numbers only mean anything from a release build**: `make build-linux`
  and `make run-linux` configure the `x64-debug` preset, which is `-O0`, and the encode
  path is now pixel arithmetic in `core/`. The same frame costs ~19 ms there against
  ~5 ms from `make release-linux`. A judder report measured against a debug binary is
  measuring the build type.

- **Apple viewers pace video by PTS on a control timebase, and the pacer never trusts
  itself**: displaying every frame the moment it arrived made Wi-Fi arrival jitter
  visible as judder while every latency number stayed excellent — cadence is not
  latency. `VideoPacer` (core, tested offline) maps host PTS to local display time the
  same way the e2e metric does — a windowed minimum of `arrival − pts` — plus one
  ~33 ms lead that arrival jitter is paid from, and `VtDecoder` drives an
  `AVSampleBufferDisplayLayer` control timebase from it, resyncing only past a 250 ms
  divergence. A pts jump over 2 s reads as a new stream, not as jitter, so the mapping
  reprimes instead of freezing for a window. Because the renderer honoring an external
  timebase cannot be proven on every OS version from here, the decoder watches its own
  back: a run of paced frames swallowed by a full renderer queue flips it back to
  display-immediately and flushes, trading the smoothing away rather than the picture.

- **Audio is one frame per datagram, and a lost one is never chased**: a 20 ms Opus
  frame at 64 kbps measures about 160 bytes, 209 at its widest, against the 1180 bytes
  a datagram has room for — so the audio path has no packetizer, no FEC, no
  reassembler and no NACK, which is most of what the video path is. Loss is absorbed
  where it costs least: Opus carries in-band FEC in the following frame, and the
  receiver asks its decoder to conceal a hole the jitter buffer reports. Retransmitting
  would be worse than useless, because a frame that arrives 200 ms late is unplayable
  yet still delays the ten behind it. `make opus-smoke` measures those numbers on any
  machine that builds the library.
- **The jitter buffer has no timer in it**: `AudioJitterBuffer` is pure state, and
  the target delay is simply how many frames it fills before starting — 60 ms is
  three. That makes the whole thing testable offline with no sleeping, and it makes
  the failure modes explicit: a burst is capped rather than queued, an empty buffer
  rebuffers rather than stuttering, and a sequence jump is read as a new stream
  rather than as thousands of lost frames. The pacing lives in `AudioPlayer`, which
  pops one frame per 20 ms of wall clock into a PCM ring that the sink's render
  callback drains.
- **The capture callback never encodes**: PipeWire and ScreenCaptureKit deliver
  audio on real-time threads with a deadline of a few milliseconds, and a blown
  deadline there xruns the host's own playback, not just Deskhub's. Opus encode is
  0.3–1.5 ms with spikes, and one `sendto` per viewer used to ride behind it on that
  same thread. `AudioBroadcaster::Offer` now only copies the 20 ms frame into a
  preallocated lock-free slot ring and stamps the capture time; a worker thread does
  the encode, the diagnostics and the per-viewer sends. A worker that falls behind
  costs a counted drop (`framesRefused`), never a glitch in the host's audio.
- **Sound needs both ends to say yes, and old clients never hear it**: a viewer sets
  bit 0 of `Hello.features`, a host advertises `kHostSharesAudio` in its capabilities,
  and the host sends a packet only to viewers whose bit is set. That is what keeps
  `kProtocolVersion` at 2: a 5.0.x viewer sends `features = 0`, so a 5.1 host never
  puts a message on its wire that it cannot parse.

- **The terminal link keeps itself alive and dials itself back**: a terminal viewer
  owns a QUIC connection of its own, separate from the video session, so none of the
  video path's keepalives reach it. Left alone at a prompt it carried no traffic at
  all and died on the 30 s QUIC idle timeout, and the viewer then stopped its thread
  in `Reattaching` without ever redialling — the shell was still waiting on the host
  for the full 2 minutes, and nothing went back for it. `TerminalViewer` now sends an
  ack-eliciting packet on a timer and redials with backoff, reusing
  `TerminalClient::Reattach()` (which was already written and tested in core, and
  simply never called) so the same shell comes back with its scrollback.
  `deskhub::KeepaliveIntervalUs` / `ReconnectDelayUs` hold the timings in core: the
  keepalive is at most half the idle timeout so one lost packet is survivable, and
  retrying stops exactly at `kTerminalReattachGraceUs`, because past that the host
  has already dropped the shell and reconnecting would silently open a new one.
- **A record goes onto the stream whole or not at all, and a client that falls behind
  is repainted rather than fed every byte**: everything reliable — control, auth,
  terminal output — is length-prefixed records sharing one QUIC stream, so half a
  record on the wire desynchronises the framing on the far side permanently;
  `RecordStream` has no way to resynchronise and the peer closes the connection.
  `QuicEndpoint::SendStream` used to write whatever fit and drop the rest, which held
  until a command like `make test` outran the link: the 1 MiB stream window filled,
  the tail of a `TermData` record was dropped, the viewer's framer failed and the
  shell "disconnected" a minute after it opened. It now refuses a record the stream
  has no room for, and closes the connection if a partial write ever happens anyway,
  because a torn stream cannot be repaired in place. Above it `TerminalHost` holds
  unsent output in a per-shell queue and retries it on every tick, so a burst that
  merely outruns the link for a moment — a build's output, say — still reaches the
  client byte for byte. Past `kMaxPendingBytes` the queue is dropped instead of grown: every
  byte has already reached the host-side mirror `Screen`, so the client is caught up
  with `deskhub::term::RenderScreen` — one repaint of the current grid, at most every
  `kRepaintIntervalUs`. Output nobody could have read is skipped rather than buffered,
  which lets a flooding command run at its own speed and still leaves the right screen
  behind. A reattaching client gets that same repaint, since its position in the byte
  stream means nothing after a gap.
- **An automatic share waits for the desktop instead of enumerating it once**:
  Windows registers autostart as a `ONLOGON` scheduled task, which fires before the
  session has a monitor to enumerate, so a single `ListDisplays()` at construction
  used to come back empty and the app reported that there was nothing to share.
  `deskhub::ui::AutoShareGate` (core, unit-tested) owns the retry rule — probe every
  `kAutoShareProbeMs`, give up after `kAutoShareGiveUpMs` — and every client drives it
  from its own timer, so the policy exists once. `NextAutoShareStep` is the same rule
  without the state, which is what the Swift client reaches through
  `dh_auto_share_step`. An automatic share never opens a modal: at login the window
  may be hidden in the tray, where a dialog is invisible and blocks the share
  forever, so refusals go to the Host banner and the log. The desktop clients also
  refresh their picker on the OS display-change signal, which is what keeps the list
  right when a monitor is plugged in later.
- **quiche over msquic/ngtcp2**: the only QUIC library with production evidence on
  both Android and iOS. It brings BoringSSL, which also serves SPAKE2 and the host
  identity — no second crypto library.
- **No connection migration**: no usable client-side support in any candidate
  library. Reconnect-and-reattach (tmux-style, already required for mobile
  backgrounding) covers it.
- **ECDSA P-256, not Ed25519**: BoringSSL's server side will not sign a TLS
  handshake with Ed25519 through quiche. Do not switch back. A stored Ed25519
  identity is replaced on load — it would fail every handshake as
  `QUICHE_ERR_TLS_FAIL` with nothing on screen to explain it.
- **The passcode verifier is one SHA-256, not an expensive KDF**: SPAKE2 already
  limits an attacker to one online guess per connection and leaves no transcript
  worth cracking offline, which is the whole job KDF hardness exists to do.
- **quiche is prebuilt, not FetchContent**: `scripts/build-quiche.sh` writes one
  directory per rust target under `third_party/quiche/` plus a shared `include/` —
  quiche.h and the BoringSSL headers boring-sys vendors, copied out because Deskhub
  calls BoringSSL directly for the host identity and wants one include path and no
  second TLS library. `DeskhubQuiche.cmake` turns that into `deskhub::quiche`; a
  missing library fails the configure.
- **Apple links `libplatform_bundled.a`**: the Xcode apps consume the platform
  archive from outside CMake, where a PRIVATE link to quiche never reaches their
  link line — so a `libtool` step fuses platform + quiche into the one archive the
  `.pbxproj` links.
- **The Windows toolchain traps are already cleared — keep them cleared**: quiche
  builds against the static CRT via `CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_RUSTFLAGS`
  for the Rust objects plus `/MT` in `CFLAGS_x86_64_pc_windows_msvc` for the
  BoringSSL objects (the msvc default is the DLL runtime, and forcing the flag
  through a blanket `RUSTFLAGS` broke the cargo build outright), and the whole tree
  pins `MultiThreaded` to match so the exe ships without the VC++ Redistributable;
  wxWidgets re-pins `wxBUILD_USE_STATIC_RUNTIME` on every configure because
  `wx_option()` caches it forever. BoringSSL must build under the default Visual
  Studio generator — the cmake crate only communicates /MT through per-config flags
  there, so forcing `CMAKE_GENERATOR=Ninja` silently reverts BoringSSL to /MD and
  the final link dies in LNK2038; if MSBuild trips MSB6003 on long paths, enable
  Windows long paths instead. Git Bash's `/usr/bin/link.exe` shadows the MSVC
  linker (put `cl.exe`'s directory first), its path rewriting mangles `/`-style
  arguments (`MSYS2_ARG_CONV_EXCL`), and NASM's installer does not touch PATH.
- **Android quiche skips cargo-ndk on Windows hosts**: cargo-ndk hands boring-sys an
  extension-less `clang` path, which CMake refuses on Windows, so `build-quiche.sh`
  sets `CC_*`/`CXX_*`/`AR_*`, the cargo linker and `--target=` for the ABI itself and
  calls plain cargo. BoringSSL still needs Ninja there (the Visual Studio generator
  cannot target the NDK), and bindgen picks up Visual Studio's libclang, which looks
  for `stddef.h` beside its own binary — `BINDGEN_EXTRA_CLANG_ARGS` points it at the
  NDK's resource headers with forward slashes, because bindgen splits that variable
  with shell rules and eats backslashes.
- **Every cross-compiled app builds its own quiche first**: `build-android`,
  `build-ios`, `build-macos` and `build-linux` depend on a quiche target for their
  ABIs, the way `debug`/`release` do for the host. quiche is per-ABI and the CMake
  configure fails without it, so a build that skipped this step looks like a broken
  toolchain rather than a missing library — and an app left behind at the last
  successful build speaks a protocol its peers no longer answer.
- **iOS quiche pins `IPHONEOS_DEPLOYMENT_TARGET=17.0`**: boring-sys's clang floats
  to the SDK default while rustc links for its own minimum, and the mismatch
  surfaces as an undefined `___chkstk_darwin` at link time.
- **Two clocks, deliberately**: `NowUs()` is monotonic (seconds of uptime) for
  intervals; `NowUnixSeconds()` is the only one that renders as a date. Mixing them
  is not loud — a stored monotonic stamp shows up as some time on 1 January 1970.
- **A Windows PTY child gets no standard handles**: with the host's own stdout
  redirected, Windows hands that redirection down past the pseudo-console attribute
  and the shell talks to the pipe; no handles at all sends it back to the attached
  ConPTY.
- **`wxWANTS_CHARS` on the Windows terminal grid**: without it the frame's dialog
  navigation eats Enter, Tab and the arrow keys before the terminal sees them.
- **macOS TCC pairs a grant with the code signature**: a locally built app.app
  (ad-hoc, re-signed every build) and the Developer ID dmg fight over the same
  `com.deskhub.macos` row — System Settings shows the permission granted while the
  copy just launched is denied, silently for Accessibility.
  `make reset-macos-permissions` clears every grant so the next launch asks again.
- **macOS is a desktop build in CI and a signed one at release, never both at once**:
  `build-desktop` compiles the app ad-hoc-signed on every push, so a Cocoa change that
  no longer builds fails its own pull request; `deploy` reaches the same app through
  `release-macos`, the fastlane path — Developer ID, notarization, dmg — that produces
  something a user can actually open. The reusable workflow therefore skips its macOS
  job when `for_release` is set, or a tag would pay for a second macOS runner to make a
  bundle nobody ships. `build-mobile` carries iOS and Android only, for the same reason
  and with the same split.
- **Every workflow gets quiche and opus from one action, and the cache key is the whole
  contract**: `.github/actions/third-party` builds both libraries for whatever targets a
  job names, which is why nineteen copies of the same cache-then-build block are down to
  one line per job. Its `cache-key` input is not decoration — it is the only thing
  keeping two jobs from restoring each other's libraries. Two target sets differ,
  and so do two runner images building the same triple: a `libquiche.a` compiled on
  ubuntu-latest and restored on ubuntu-22.04 links a glibc the release exists to avoid.
  Anything that changes what the build produces belongs in that key.
- **One static release CRT on Windows, every configuration**: cargo builds quiche
  against the static release CRT (the msvc default — never force it through
  `RUSTFLAGS`, that leaks into proc-macros and kills cargo), and the whole CMake
  tree pins `MultiThreaded` to match, which is also what keeps the app a single
  exe with no VC++ Redistributable. Rust offers no debug-CRT build, so Debug
  matches too: `_ITERATOR_DEBUG_LEVEL=0`, `/U_DEBUG`, `/RTC1` stripped — the
  release CRT has no `_CrtDbgReport` and no run-time check support. Any mismatch
  ends in a wall of LNK2038.
- **Passcode = self-service admission, approval = the fallback**: a typed code is
  always verified; no code means a human decides. The passcode never crosses the
  network in any form an attacker can take home.
- **The VT emulator is ours**: no platform terminal widget is available on all five
  clients under a usable licence, and owning it makes terminal behaviour testable
  offline and identical everywhere.
- **The host's shell mirror is fed from byte one**: PTY output is a destructive
  single-consumer stream — bytes read and shipped to the viewer cannot be replayed
  later — so the grid *Stop & attach* opens must be built as the bytes pass, not
  when the button is pressed. While a remote viewer is attached the mirror's own
  terminal-query responses are discarded: the viewer's screen already answers them,
  and the shell must not hear two answers.
- **One port**: beacon, screen and terminal share a single listener; QUIC
  multiplexes connections and streams. The old second port existed only because the
  pre-QUIC screen path monopolised the socket.
- **One `HostLink`, four former handshakes**: dial + trust check + auth + recovery
  used to be written four times on the client side — source query, viewer, file
  sender, and the terminal on a raw `QuicEndpoint` of its own — which is how the
  send window learned about a changed host key three fixes later than the viewer
  did. `HostLink` is now the only client-side code that dials or authenticates; a
  service opens its `Chan`, gets its own inbox queue, and drains it on its own
  thread. The terminal's redial-with-backoff moved into the link so every surface
  that asks for recovery inherits it, and the trust rules live in one place: a
  changed key parks the link in `Deciding` until a person answers (only the source
  query passes it through, `trustGate=false`, remembering nothing — its callers
  have no prompt to show), and only a passcode the host cryptographically proved
  pins a key automatically.
- **`HostLink` sends through `Send`, not `SendMessage`**: on Windows the OS headers
  behind the platform layer define `SendMessage` as a macro for `SendMessageA`, and
  in `HostLink.cpp` they landed after the class declaration but before the method
  definition — MSVC then required a definition for a `SendMessageA` member no header
  had declared. Win32 API names (`SendMessage`, `PostMessage`, `CreateWindow`,
  `GetObject`, …) are never safe as method names in any translation unit an OS
  header can reach; the rename is the fix, not an `#undef`.
- **The portal ScreenCast session lives and dies with a D-Bus connection**: GLib caches
  the shared session bus by weak reference, so `g_object_unref` on the last handle
  disposes the connection outright. `xdg-desktop-portal` then drops the session, the
  compositor destroys the PipeWire node, and the node id the portal just handed over
  points at nothing — the stream reaches `paused` and fails with *no target node
  available*. `PortalScreenCast` therefore owns its `GDBusConnection` for as long as
  the session is open rather than borrowing one per call. The desktop app masked this
  for a long time because GTK keeps a reference on the session bus for the life of the
  process; `deskhub-cli` links no GTK and had none.
- **Every icon is derived, and only some of them are rounded**: `make icons` rebuilds
  the whole set from the single master `assets/icon_1024.png`. macOS, iOS, the Play
  Store listing and Android's adaptive-icon pipeline mask artwork into their own
  shapes, so those assets stay full-bleed squares; Windows, Linux and pre-API-26
  Android launchers draw whatever they are given, so their icons carry the rounded
  corners and the transparency baked in — otherwise the app shows up as a hard blue
  square next to every other rounded icon. `scripts/make-icons.py` is pure standard
  library on purpose: bootstrap installs no image tooling.
- **A desktop client holds many hosts at once; a phone holds one**: the connect page on
  Windows, Linux and macOS keeps no connected state of its own. A host that answers gets
  a connection window — `ConnectionFrame` in `client/windows/win32/MainFrame.cpp`,
  `ConnectionWindow` in `client/linux/gtk/MainWindow.cpp`, the `connection` `WindowGroup`
  in `client/macos/app/swift/App.swift` — owning that host's address, passcode, caps,
  sources and control tick, so the page stays free to dial the next one. The main window
  keeps only a list of the open ones, to raise a window when the same host is dialled
  twice, to push each status probe at the window whose address matches, and to close them
  all on quit. Android and iOS deliberately stay single-connection: a phone screen has no
  room for a second panel, and the session it opens is full-screen anyway.
  `ui::SameDeviceAddr` is what "the same host" means everywhere — see the entry below.
- **One host, two spellings, one comparison**: `ScanAddressText` drops the port when it is
  the default, so a scanned row reads `192.168.1.60` while the address the user typed and
  connected with reads `192.168.1.60:47777`. Comparing those as strings silently fails,
  and every place that did lost something real: the connected panel found no matching
  device row and so showed no ping, and `PasscodeForDevice` did not find the code saved
  for a host that was picked out of the scan list. Address equality therefore goes through
  `ui::NormalizedDeviceAddr` / `ui::SameDeviceAddr` (`core/ui/Strings.h`), exposed to the
  Swift and Kotlin clients as `dh_same_device_addr`. Never compare two device addresses
  with `==`.
- **Parity sized for the longest packet in its group, not for the MTU**: every parity
  packet went out at the full `Packetizer::kParityStride`
  (`kFecLenPrefix + kMaxVideoPayload`, 1176 B) however short the data packets it protected
  were, so a frame that fits in a single packet — the ordinary shape of a low-motion delta
  frame — bought a whole full-MTU parity packet to protect a few hundred bytes, 100 %
  overhead. Parity is now cut to `kFecLenPrefix` plus the longest data packet in its group;
  `BuildFecPacket` already accepted a variable-length span and `Reassembler::TryRecover`
  already needed no more than that and refuses anything shorter, so the wire format did not
  move. Note what this does *not* fix, because the packetizer interleaves (`i % numGroups`)
  and only the last packet of a frame is short: from two packets up, every group still holds
  a full-MTU packet and still gets full-stride parity. Above that size the cost is set by the
  parity *count*, `ceil(count / kFecGroupSize)` — a 9-packet frame pays 22 %, not 12.5 %. FEC
  overhead is a curve over frame size; it is never the single 1/8 the group size suggests.
- **A datagram the transport refuses is indistinguishable from one the network dropped**:
  `quiche_conn_dgram_send` fails when quiche's own congestion control or its
  `kDatagramQueue` will not take the packet, and `SendDatagram` branched on that result only
  to return it — the signal itself was thrown away. Such a packet never leaves the machine,
  yet the viewer's `Reassembler` sees an ordinary hole and reports it as loss, so the host
  can spend FEC parity on, and cut its own encoder bitrate for, packets it dropped itself.
  `QuicSendStats::datagramsRefused` counts them, and the host's `evt=sum` line carries
  `dgram_tx` and `dgram_refused` per window beside the loss figures. Read those two before
  believing any FEC or congestion-control measurement: quiche's congestion control sits
  under the datagram path in series with `BitrateController`, and neither one knows the
  other exists.
- **The FEC group count now travels in the header byte that used to be zero**: both ends
  derived it as `ceil(pktCount / kFecGroupSize)`, which tied two separate things together —
  how many packets share a parity packet, and how far apart consecutive packets land in
  different groups. Interleave depth was a consequence of frame size rather than a choice:
  a 40-packet keyframe got depth 5, while an 8-packet delta frame, the common shape at
  60 fps, got a single group and no interleaving at all, so the strongest burst protection
  sat exactly where it was least needed. `FecHeader::groups` now carries the count in the
  byte `BuildFecPacket` used to write as zero; zero still means "derive", so the header did
  not change size and `kProtocolVersion` did not move. `FecGroupCount` is the one function
  both ends call, and it clamps a signalled count to the packet count so the two can never
  disagree about a group index. What it costs: a data packet can no longer be mapped to a
  group before that frame's first parity packet arrives — harmless, because recovery needs
  the parity anyway. Measured in `LossGoodputTests` at 5 % Gilbert-Elliott loss with a mean
  burst of 4 packets, keyframe requests fell from 171/min at the derived depth to 81/min at
  depth 8, and the share of damaged frames rescued rose from 34 % to 79 %.
- **A FEC scheme is configured at both ends, never negotiated**: `FecScheme` is an
  interface with one registered implementation, `xor`, and `Packetizer` and `Reassembler`
  each hold one. Nothing on the wire says which scheme wrote a parity packet — the payload
  format is the scheme's own business — so a host and a viewer given different schemes
  recover nothing, and the mismatch surfaces only as recovery that never fires. That is
  deliberate while there is one implementation: `--fec NAME` on `deskhub-cli share` and
  `connect` exists to measure alternatives against each other, is refused at parse time if
  the build has no scheme by that name, and is not a setting for anyone to pick. Signalling
  the scheme on the wire is the job of whichever implementation wins the bake-off and
  becomes the single production path; until then `--fec` is an instrument that must be set
  identically at both ends of one session.
- **A burst loss model is the only one that can rank interleave depths**: the link
  simulator in `LossGoodputTests` drops packets from a seeded two-state Markov chain
  parameterised by loss rate and mean burst length, and runs a uniform-random model beside
  it as a control. The control is not decoration — it is the check that the burst model is
  really bursting. At the same 5 % loss with one parity packet per group, uniform loss is
  rescued 71 % of the time and clustered loss only 34 %, because scattered single losses
  are exactly what one parity packet absorbs and a burst inside one group is exactly what
  it cannot. If the two models ever score the same, the chain has degenerated to
  independent loss and every depth and scheme ranking taken from it is meaningless. Each
  sweep point prints a `[csv]` row, so the numbers behind any FEC decision can be
  regenerated by running the test.
- **The FEC arming policy, not the FEC scheme, decides whether FEC does anything**: the host
  arms parity when the viewer reports loss and stands it down after
  `kCleanSecondsBeforeDroppingFec`. Two details make that policy blind to the loss real links
  actually show. `MakeFeedback` rounds loss to whole percent
  (`fb.lossPct = uint8_t(std::lround(w.lossPct))`), so anything under 0.5 % arrives at
  `BitrateController` as a flat 0 %; and the arm test is `fb.lossPct >= 1`, so a link losing
  one packet a second — 0.1 % of a 540-packet second — never arms at all. Measured against a
  real host over home WiFi: 5 single-packet losses in 196 s, `fec_rx` zero in every one of 118
  windows, and each lost packet of at most 1174 B paid for a dropped frame and a full IDR of
  ~150 KB. Against a second host the whole cycle was visible: clean for 10 windows, one window
  at 0.5 %, parity for exactly 10 more windows, then off again — 1224 parity packets spent to
  repair 1. `LossGoodputTests` now drives the real `BitrateController` from the real
  `MakeFeedback`, so the sim reproduces this: at the measured operating point (0.1 % loss,
  bursts of 1) always-on FEC rescues 16 of 16 damaged frames and requests no keyframes, while
  the shipping policy is armed 30 % of the time, rescues 6 of 16, and requests 20 IDRs a
  minute. Any comparison between FEC schemes that assumes parity is on the wire is measuring a
  configuration this policy almost never produces.
- **An objective function counted by cause, or it counts the wrong thing**: A1 scores FEC by
  keyframe requests per minute, but only `KeyframeReason::Loss` is caused by the network. In
  148 windows with two viewers on a link reporting 0 % loss and no reconfiguration, the host
  still emitted 10 full IDRs of ~72 KB — every one of them requested by a viewer whose own
  decode queue had overflowed. `q_overflow`, `dec_fail` and `display_congested` come from the
  client's pipeline and `wait_idr` comes from startup; lumping them together inflates the
  score of whatever transport change is being tested. The reason was already in the log line
  and nowhere in a counter, so `KeyframeRequestLog` takes a typed `KeyframeReason` and prints
  `evt=kf_sum` with a count per reason each window. That fixed the viewer and left the host
  blind: the host is the side that spends the IDR, and `RequestKeyframe` was an empty
  datagram, so every request looked alike to the machine whose bitrate the score belongs to.
  The message now carries the reason as one payload byte, `ScreenHostSession` hands it to
  `onKeyframeRequest`, and the host prints `evt=kf_req_sum` split the same way. Two details
  make it usable: a peer built before the byte existed sends no payload and lands in
  `unknown` rather than in the first enum slot, and the host's own mid-stream re-join is
  labelled `viewer_join` rather than borrowing a viewer's reason. Split the number before
  scoring anything with it.
- **Whether retransmission can rescue anything is set by how long a frame is held, not by the
  retransmit path**: `PlanNack` and `RetransmitCache` work exactly as written — in the sim the
  host serves every index the viewer asks for, and every served packet arrives. It rescues
  nothing. `PopReady` drops an incomplete non-IDR frame once two newer frames are complete,
  which at 60 fps is about 33 ms, while a NACK costs a full round trip on top of the 2 ms
  `kNackHoldUs`: at the 40 ms round trip of a home link the repair lands roughly 24 ms after
  the frame it repairs has already been thrown away. Shorten the round trip to 4 ms in the
  same sim and the same path starts rescuing frames. So A1's "NACK-only" option is not a
  question about retransmission at all — it is a question about `kStallTimeoutMultiple` and
  the overtaken rule against RTT, and any hybrid that switches between FEC and NACK by RTT is
  really switching on whether the frame will still be there when the repair arrives. Note also
  that with parity armed no frame stays incomplete long enough to be nacked: FEC and NACK
  never compete for the same repair, so their contributions add rather than overlap.
- **The hold rule was the whole answer, and reading it at one operating point hid that**: the
  paragraph above concluded NACK-only was a dead end. It was measured at 0.1 % loss with the
  overtaken rule fixed at two newer frames — so it measured the rule, not retransmission. A
  72-point sweep over repair mode x round trip x hold length says something different. At 5 %
  loss with bursts of 4 and a 40 ms round trip, NACK-only rescues 11 damaged frames and asks
  for 171 keyframes a minute under the shipping rule; let the frame wait until its own repair
  could plausibly have arrived and the same code rescues 30 and asks for 72 — better than
  `fec-only` at the same point (21 rescued, 171 requests) while sending **no parity at all**.
  At 80 ms the pattern holds, and `fec+nack` reaches 63 requests a minute, the best number in
  the grid. At a 4 ms round trip nothing changes, because the repair already lands inside two
  frames: the win is purely a function of RTT against the frame interval, which is why
  `OvertakenLimit()` derives the count from the repair window rather than raising a constant.
  What it does not buy is free delay — the longest gap between two delivered frames moves both
  ways across the grid, up at some points and down at others, because fewer keyframe requests
  can more than pay for a longer wait. A negative result taken at a single operating point is a statement about
  that point, and this one was hiding a factor of three.
- **That derivation is now the shipping default, on the strength of the sweep alone**: for a
  while the gate stayed shut — `OvertakenLimit()` returns the old two-frame hold unless a caller
  raises the ceiling above it, so the sweep could exercise the derivation while production kept
  the measured behaviour. `ScreenViewer::Config::overtakenLimit` now defaults to
  `kDefaultOvertakenLimit` (8 frames, 133 ms at 60 fps), which opens it. Eight is not tuning: the
  sweep found 8 and 30 indistinguishable at every point, because the derived value is what binds
  below roughly a 130 ms round trip and the ceiling only caps the tail — without one, a 300 ms
  link would hold 29 frames, close to half a second of latency, to save a keyframe. The gain is
  conditional and worth stating in full: at 40 ms and above it halves to quarters the keyframe
  rate, and at high loss it *shortens* the longest stall too, because not spending an IDR saves
  the 120 ms that keyframe would have cost. At a 20 ms round trip and 1 % loss it is a small win;
  at 20 ms and 5 % loss it buys nothing and adds about 34 ms to the longest stall. LAN viewers
  therefore pay a little for what WAN viewers gain. ⚠️ **This rests on simulation only.** Every
  number above comes from the seeded model in `core/tests`, which has a fixed one-way delay and
  no jitter, no reordering and no congestion, and carries random bytes rather than video. It has
  never met a NIC. The `netem` half of the Phase 3 validation is the thing that would confirm or
  refute it, and it has not been run.
- **Reed-Solomon needs more than one parity packet per group, and that is a wire change, not
  an implementation detail**: `FecHeader` is exactly 16 bytes with every one spoken for
  (frameId 4, timestampUs 8, pktCount 2, groupIndex 1, groups 1), `Packetizer` emitted one FEC
  packet per group, and the receiver stored one parity payload per group. RS(k,n) with a single
  parity row is XOR with more arithmetic, so none of that could carry it. The parity index now
  rides in the common header's flags byte, which used only bit 0 (`kVideoFlagIdr`) and bit 1
  (`kVideoFlagFrameEnd`): bits 2-7 give 64 parity packets per group without growing the header
  or moving `kProtocolVersion`. A receiver that does not read those bits keeps the first parity
  packet of each group and ignores the rest, degrading to XOR rather than corrupting anything.
  Receiver-side parity is keyed by group and index packed into one `uint16_t` rather than
  nested vectors — the nested form cost an extra allocation per group and showed up immediately
  as `video/reassemble-fec-recovery` going from 1.30 to 1.43 allocations a packet. Measured
  cost of the scheme itself, at two parity packets a group: encode 143 µs a frame against
  22.7 µs for XOR, a factor of 6.3, while recovery is only 1.4x because it runs on one group
  rather than the whole frame. That is what buying recovery of two losses per group costs.
- **Four congestion controls behind one interface, and what each can actually see**: A2's
  options are now `aimd` (the shipping AIMD, unchanged), `delay-trend`, `scream` and `hybrid`,
  built by `MakeCongestionControl` and held by `SourcePipelineState` as a pointer rather than a
  value. What matters when reading them: the protocol's `Feedback` carries loss percent, RTT
  and receive rate once a second, and nothing else — there are no per-packet arrival timestamps,
  so a literal WebRTC delay-gradient filter over inter-group delay variation cannot be built
  here. `delay-trend` therefore works the queue delay implied by RTT above its running minimum,
  and `scream` follows the reported receive rate bounded by that same queue delay. They are
  adaptations to the signals this wire carries, not reimplementations of the papers, and
  comparing them against published GCC or RFC 8298 numbers would be comparing different
  algorithms. `hybrid` takes the lower of the two rates and the union of their FEC decisions.
  All four share the same FEC arming rule, so switching control does not silently change when
  parity goes out.
- **Reconnect backoff without jitter brings every viewer back at the same instant**:
  `ReconnectDelayUs` doubled from 500 ms to a 5 s cap as a pure function of the attempt count,
  so viewers that lost the same host at the same moment retried in lockstep and hit it together
  on every round. It now takes a caller-supplied `jitter` word and returns a delay drawn from
  the top half of the nominal backoff, keeping the retry rate bounded while spreading arrivals.
  `core/` cannot reach `deskhubp/system/Random.h`, so the randomness has to come in as an
  argument — the signature change to every caller is the point, not an inconvenience.
- **Reed-Solomon at one parity row is XOR, measured to the digit**: the Phase 3 sweep runs 180
  points over scheme x parity rows x interleave depth x loss rate x burst length x round trip,
  with FEC forced on so it measures the scheme rather than the arming policy. At 5 % loss with
  bursts of 4, `rs` with one parity row and `xor` produce identical numbers at every depth —
  33.9 % of damaged frames rescued and 171 keyframe requests a minute at the derived depth,
  78.7 % and 81 at depth 8 — which is what the algebra says must happen, and a useful check
  that the implementation is right. It also means RS earns nothing below two parity rows while
  costing 6.3x the encode CPU, so one row is never the configuration to ship it in. **No point
  in the grid beats the shipping default once overhead is counted, and reading the sweep
  without that column is how you conclude otherwise.** Decoupling interleave depth adds no
  CPU, and it is tempting to call that free: it takes rescue from 33.9 % to 78.7 % and keyframe
  requests from 171 to 81 a minute. It also takes parity overhead from 15 % to **100 %** — at one
  group per packet every packet carries its own parity, which is duplication, not coding. On a
  20 Mbps budget that spends roughly half the picture to halve the keyframe requests. `rs` with
  three rows at depth 4 reaches 36 requests a minute at **150 %** overhead. The sweep therefore
  produces no winner to promote; its result is that the defaults stand and the contestants stay
  as reference behind `--fec`. `overhead_pct` is now a column of its own in the CSV, and a test
  asserts depth costs more than three times the parity, because the first write-up of this same
  sweep called depth the cheapest lever in the grid and was wrong.
- **A kept implementation earns its place by being reachable and tested, not by being swept
  under every sanitizer**: the Phase 3 sweep promoted nobody, so `rs` stays in the tree with no
  way for production to select it — `Packetizer` and `Reassembler` both start on
  `kDefaultFecScheme`, `ShareOptions` and `ScreenClientConfig` name no scheme until something
  asks, and only `deskhub-cli --fec=NAME` ever does. That reachability is the entire reason a
  losing implementation is worth keeping, so a test now asserts it instead of leaving it to
  habit. Keeping them is not free either: `rs` encodes a frame in 143.9 µs against XOR's
  22.8 µs, and the sweep that exercises both dominates `core_tests` — 8.6 s for the full matrix
  against 2.9 s for the shipping scheme alone, before ASan or TSan multiply it.
  `DESKHUB_FEC_MATRIX=shipping` selects the smaller matrix, and the sanitizer jobs set it
  unless the diff touched `FecScheme` or its tests, so a reference implementation is still
  swept under a sanitizer on exactly the changes that could break it, while the eight ordinary
  unit jobs keep covering it on every commit. An unrecognised value fails the run instead of
  quietly choosing one, because "which implementations did this run actually cover" is not a
  question a typo should get to answer.
- **A knob with no caller outside its own tests is not a knob**: the Phase 3 sweep moves three
  axes — scheme, parity rows and interleave depth — and only the first of them could be reached
  from a built binary. `--fec` picked the scheme, which the sweep itself shows is the axis that
  matters least: `rs` at one parity row reproduces `xor` to the digit. The two that do move the
  numbers were unreachable. `Packetizer::SetFecGroups` had no caller outside `core/tests`, the
  same shape of mistake Phase 1 hit with `SetVideoPath`. Worse, the parity ratio was reachable
  but not holdable: `ViewerBroadcast` calls `SetFecParityPerGroup` on every broadcast from
  `wantFecParity`, which `ApplyFeedback` rewrites each second from `FecParityRowsFor(lossPct)` —
  1 row below 3 % loss, 2 below 6 %, 3 above. So on a good link the policy answers 1, and
  `--fec=rs` there is `xor` with 6.3x the encode CPU. Anyone measuring on home Wi-Fi would have
  concluded Reed-Solomon changes nothing, having never once run it in the configuration where
  it differs. `--fec-parity` now pins the ratio against the policy, `--fec-depth` reaches
  `SetFecGroups`, and `--fec-arm always` holds parity on the wire so the scheme is measured
  rather than the arming policy — which is what the simulated sweep does, and the point of the
  flags is that a real link can now be asked the same question. Before trusting a flag to
  reproduce a measurement, find the line that reads it in production; a setter and a test are
  not that line. Each source also logs the configuration it ended up with, read back from the
  packetizer rather than from the options — `FEC measurement: scheme=xor parity=1 depth=4
  arm=policy` after being asked for three parity rows is the honest answer, because `xor`
  carries one, and a sweep row labelled by what was requested rather than by what ran is worse
  than no row at all.
- **The same missing caller was in four more places, and one of them still is**: running that
  check across the rest of Tier A found `SetCongestionControl`, `SetAdaptiveTarget`,
  `SetAdaptiveLead`, `SetDisplayIntervalUs` and `MakeClockOffsetEstimator` with **zero** callers
  outside their own tests. Three of the four congestion controls could not be selected, the
  adaptive audio target could not be switched on, and all three clock estimators existed only
  in a sweep. `--cc` now reaches the host's control loop and `--audio-delay` / `--audio-adaptive`
  reach the viewer's jitter buffer, both logged back from the live object. Two are deliberately
  left alone: `VideoPacer` is the only user of `ClockOffset`, and `VtDecoder` is the only user of
  `VideoPacer` — so on Windows and Linux there is no pacer to configure, and adding `--clock` or
  `--vsync` there would manufacture exactly the fake knob this entry is about. `RollingMinEstimator`
  is a pure wrapper around `ClockOffset`, so swapping `VideoPacer` onto the contract is a
  behaviour-preserving change whenever a non-Apple viewer starts using the pacer — but not before.
  A contract with no production caller is a sweep fixture wearing an interface, and writing more
  implementations behind it does not change that.
- **The pacer never consumed the method the three clock estimators disagree on**: with
  `VideoPacer` moved onto the `ClockOffsetEstimator` contract — behaviour-preserving, since
  `rolling-min` is a pure wrapper around the `ClockOffset` it used to embed — an 18-point sweep
  runs all three under 0/5/20 ms of arrival wobble, with and without a 30 ms transit step.
  They are indistinguishable: phase spread sits at 6898-6937 µs for every estimator at every
  point, and `kalman` reproduces `rolling-min` to the microsecond. The reason is in the
  interface, not the algorithms. The pacer calls `AddSample`, `ready`, `Reset` and `floorUs` —
  never `LatencyUs`, and `LatencyUs` is the only method the three implement differently:
  `KalmanEstimator::floorUs()` returns `lowest_`, which *is* the rolling minimum. So A5 cannot
  be scored on judder at all; the axis it moves is the published `e2e_abs_ms`, which is where
  its own tests already measure it. Before wiring a contract into a consumer to make a bake-off
  run, check that the consumer calls the method the contestants differ on — otherwise the sweep
  produces a tidy table of the same number.
- **What to ask the encoder for after a lost reference is a policy, and it was a constant**:
  the viewer's `InvalidateRef` message travelled the whole wire already, and the host answered
  it with `forceIdr.store(true)` — so "reference invalidation" was, in every case, a full IDR.
  `media::RecoveryPolicy` now decides between three answers from what the backend says it can
  do: fall back to the newest long-term reference older than the lost frame, start a rolling
  intra refresh, or send the keyframe. Two rules matter. A reference newer than the lost frame
  is never usable, because the viewer may never have decoded it — only an older one is safe. And
  a second loss report arriving before any frame has been encoded means the cheap answer did not
  work, so it escalates to a keyframe rather than looping. `ReferenceInvalidatingEncoder` and
  `IntraRefreshEncoder` join the optional concepts in `VideoContract.h`. The two Windows backends
  execute them; the other four declare nothing, so their capability set stays empty and their
  behaviour is unchanged — a policy is only ever as good as the encoder that can carry it out.
- **Codec negotiation was a single bit test, and the mask was already 16 bits wide**: `Hello`
  carried a `codecMask` and `HelloAck` a `Codec`, but the host only ever checked
  `codecMask & kCodecMaskH264` and answered `Codec::H264`. The mask now names H264 4:2:0,
  H264 4:4:4, HEVC and AV1, and `NegotiateCodec(hostMask, clientMask)` picks the first entry of
  an explicit preference list present on both sides, falling back to the 4:2:0 baseline that
  sits last in that list precisely so it is the floor and never the first choice. Old peers
  advertise bit 0 alone and still settle on H264 receiving the value 0, so nothing on the wire
  had to move. No encoder in the tree produces anything but H264 4:2:0, so the mechanism is
  inert today — which is what C3 asked for, a capability table and a negotiation rather than a
  race. The order itself is provisional: whether 4:4:4 for text sharpness should outrank AV1 for
  bitrate is the question C3 exists to settle, and it needs measurement this order does not have.
- **A one-way stream can never yield an absolute latency, however good the estimator**: all
  three offset estimators — rolling minimum, trendline, Kalman — take the same input, the
  difference between a frame's host timestamp and its local arrival, and that difference is the
  clock offset plus the one-way delay welded together. Every one of them subtracts a floor and
  reports the excess, so `e2e_ms` was a number about queueing, not about latency, and putting it
  beside another tool's figure would have been meaningless. The fix is a second timestamp, not a
  better filter: `PingPong` now carries `hostTimeUs` beside the client's `sendTimeUs`, and
  `ClockSync` keeps the exchange with the smallest round trip in a ten-second window — the one
  with the least queueing in it — to estimate the clock offset. Absolute end-to-end latency is
  then `(arrival - offset) - hostPts`, reported as `e2e_abs_ms` beside the relative `e2e_ms`.
  The payload grew from 12 to 20 bytes, which is safe because the common header carries no
  payload length and `ParsePingPong` only ever required the first twelve: an older peer reads
  its three original fields and ignores the rest, and a newer peer reading an older 12-byte
  message sees `hostTimeUs == 0` and falls back to the relative number. This rests on the paths
  being symmetric, which is NTP's assumption and a limit of the method, not of the code — an
  asymmetric route puts half the asymmetry straight into the offset.
- **An unsigned exponential filter walks the wrong way the first time its input drops**: both
  the audio jitter estimate and the pacer's were written as
  `jitterUs_ += (spread - jitterUs_) >> shift` with `spread` and `jitterUs_` both `uint64_t`. On
  any sample quieter than the running average the subtraction wraps to something near 2^64, the
  shift keeps almost all of it, and the estimate explodes instead of decaying. It stayed
  invisible until the audio delay/gap curve was actually plotted and the adaptive target sat at
  its 500 ms ceiling on a link wobbling by 15 ms. Both now do the arithmetic in `int64_t`. The
  curve found it; neither the unit tests around it nor the shape of the code did.
- **The delay-versus-gaps curve says the fixed audio target is still the right default**: with
  the filter fixed and playout driven by a clock rather than by arrivals, the sweep runs six
  target delays against three jitter levels. The adaptive target wins outright on a steady link
  — 20 ms held instead of 60 ms for the same single startup gap — and loses under jitter: at
  40 ms of wobble it settles on the same 60 ms the fixed target uses but pays four concealments
  instead of one. The cost is the adaptation itself. Raising the target mid-stream means waiting
  to refill to it, and that wait is an underrun; restricting the raise to moments when the queue
  already holds enough removes most of it but leaves the target under-provisioned. Shipping this
  on by default would trade a measured gap increase for a latency win the sweep only confirms on
  links that were never the problem.
- **Vsync matching is not a latency trade, which is what the sweep proved**: A6 framed judder as
  something to plot against added delay, so the sweep varies the pacer lead from 8 ms to 66 ms
  with and without snapping. Without snapping the phase a frame lands on inside a 6944 µs
  refresh interval spans about 6000 µs at every single lead — eight times the delay narrows it
  by nothing, because the spread comes from 60 fps content meeting a 144 Hz panel, not from
  arrival wobble. Snapping puts it at exactly zero and costs no delay at all. There is no curve
  here to trade along; there is a defect and a fix.
- **An HRESULT is not a bool, and `ICodecAPI::IsSupported` returns `S_OK` for yes**: every rate
  control property the Media Foundation encoder sets went through
  `if (!codecApi->IsSupported(&api)) { report("NOT SUPPORTED"); return; }`. `S_OK` is 0, so that
  branch fired on exactly the properties the MFT *did* support, and `SetValue` was attempted only
  on the ones it did not. Measured on this machine after putting the raw HRESULT in the log:
  `MeanBitRate`, `RateControlMode=CBR`, `GOPSize` and `BufferSize(VBV)` all answer
  `hr=0x00000000` and had all been skipped, so the MF backend has been encoding on MFT defaults
  for its whole life — no CBR, no target bitrate, no infinite GOP, no VBV — while NVENC got the
  full rate plan. Any bake-off between the two before this fix compared a configured encoder
  against an unconfigured one. It also inverts the evidence in the first entry of this section:
  the `MeanBitRate: NOT SUPPORTED` line quoted there meant the Intel MFT *did* expose the
  property. The mandatory fallback that entry argues for is still right; the reason given for it
  was a log read backwards. `SetBitrate` and `RequestKeyFrame` in the same file used the opposite
  polarity, which is what a bool-shaped read of a tri-state return buys: two call sites that
  cannot both be right, and no test that can tell them apart.
- **C1's knob was missing the same caller A1's was**: `CreateEncoder` tried NVENC, then Media
  Foundation, and kept whichever started first, so on any machine with an NVIDIA driver the MF
  path could not be measured at all. `--encoder auto|nvenc|mf|vaapi|videotoolbox` now names the
  backend, and naming one that will not start stops the source rather than quietly measuring the
  other — the failure a fallback would hide is the measurement. Scoring it needed a new counter
  too: `enc_ms_avg`/`enc_ms_max` cannot see a tail, so `evt=sum` now carries `enc_us_p50` and
  `enc_us_p99` from a 512 µs histogram. Each backend also reports recovery capabilities read from
  the driver instead of assumed: NVENC through `nvEncGetEncodeCaps` (`max_ltr_frames=8`,
  `ref_pic_invalidation=1`, `intra_refresh=1` on an RTX 5070 Ti), Media Foundation through
  `IsSupported` on the three LTR properties and `GradualIntraRefresh`, all supported. That answers
  what A4 was waiting for — both Windows backends can hold long-term references — but the caps were
  logged rather than handed to `RecoveryPolicy` until an encoder could act on them: declaring the
  capability while nothing consumed `invalidateBeforeFrame` or `wantIntraRefresh` would have
  turned loss recovery into a no-op. First numbers, on an idle desktop rather than a fixed clip: NVENC
  encodes at p50 2.5-5.6 ms and p99 2.7-5.7 ms, Media Foundation at p50 0.5-13.8 ms and p99
  12.3-17.6 ms. Both reach the same silicon here — `mf` resolves to "NVIDIA H.264 Encoder MFT" on
  this machine — so this is not yet the Intel-versus-NVIDIA question C1 asks; it is what going
  through Media Foundation costs to reach the same hardware. A second Windows machine separates
  them: an Intel UHD 750 with no NVIDIA driver installed at all, where `mf` resolves to "Intel
  Quick Sync Video H.264 Encoder MFT", reports the same three LTR properties and
  `GradualIntraRefresh` as supported, and encodes 1920 × 802 at p50 1.5-2.6 ms and p99 2.5-8.4 ms,
  with occasional windows reaching 27 ms. That is the Intel column C1 asked for, and it says Quick
  Sync also holds long-term references — but it is **not** yet a fair race against the NVENC row
  above: different machine, different capture size, different desktop content, and neither side is
  a fixed clip. The same run confirms the naming rule end to end: `--encoder nvenc` there logs
  "Failed to load nvEncodeAPI64.dll" and stops the source rather than quietly encoding through
  Quick Sync under NVENC's name, while `--encoder auto` falls through to Media Foundation and logs
  why it moved on.
- **Every `QuicEndpoint::Poll` ended with a sleep, and batching is what made that visible**: the
  read loop called `RecvFrom` until one returned nothing, and "nothing" only comes back once
  `SO_RCVTIMEO` expires — so a poll that had already drained the socket still paid the timeout
  floor, 1 ms in the common case, on every single call. `SessionTransport::RecvFrom` gates that
  same `Poll` behind its own `WaitReadable(10 ms)`, so the sleep was pure addition on every
  receive. Reading in batches through `recvmmsg` removes the probing read entirely: a batch that
  comes back short means the socket is empty, so the loop stops without asking again. That left
  `SetRecvTimeout(0)`, which POSIX reads as "wait forever" and the old code therefore coerced to
  1 ms; it now means "do not wait" (`O_NONBLOCK` on POSIX, `FIONBIO` on Windows), which is what
  `Poll(now, 0)` always claimed to be. Safe because every caller that passes 0 already has its own
  wait around it — `WaitEstablished` polls then waits, `RecvFrom` waits then polls — so nothing
  turns into a spin. Measured by `platform_perf` over loopback: an idle poll 1 978 692 ns → 732 ns,
  a 512-byte terminal record 4 244 632 ns → 9 201 ns, 64 KB of stream 16.7 MB/s → 500.7 MB/s, a
  QUIC datagram 248 515 ns → 3 986 ns, and the 64 KB→256 KB drain stayed linear (3.78x → 3.84x for
  4x the work). Loopback says nothing about a real link's throughput, but the 1 ms floor it removes
  is wall-clock time on any link.
- **The batching bought less than the sleep it exposed, and the send side bought more than the
  receive side**: `sendmmsg` had already collapsed a burst into one syscall, so `UDP_SEGMENT`
  (GSO) buys kernel-side work rather than syscall count — one pass through the UDP/IP stack
  instead of sixteen. Over loopback at 16 × 1200 bytes: one `sendto` plus one `recvfrom` per
  datagram costs 2036 ns/datagram, batching only the send side 636 ns, batching both 623 ns. So
  GSO is worth 3.2x here, and the last step is inside run-to-run noise — the two batched rows
  swapped order between runs — because on loopback the kernel already holds every packet and a
  receive is nearly free. `recvmmsg` still earns its place by removing the reason the loop had to
  probe at all: measured across one `platform_tests` run under `strace`, 17 317 datagrams arrived
  in 1631 productive calls, 10.6 per syscall. GSO applies only
  to a run of equal-sized datagrams with an optionally shorter last one, which is the shape
  `quiche_conn_send` produces, and any kernel that refuses it (`EIO`, `EINVAL`, `ENOPROTOOPT`,
  `EOPNOTSUPP`, `EMSGSIZE`) switches the socket back to `sendmmsg` for good rather than per burst.
- **A hot loop's allocations can hide behind its own sleep**: `Service()` built a `std::vector` of
  connection ids on every call, `DrainStreams` a fresh 16 KB chunk buffer, `DrainDatagrams` a
  1350-byte one — about 1.5 allocations per `Poll`, on a path that runs per packet. None of it
  showed up while every poll also slept 1 ms; the moment the sleep went, `quic/terminal-record-
  delivery` jumped from 9 to 27 allocations per record, because the same record now costs three
  times as many poll rounds and each round allocated. The fix is stack arrays bounded by
  `kMaxConnections` for the id lists and buffers owned by the endpoint for the two drains, and
  `DrainStreams` returns before touching its buffer when no stream is readable. `quic/poll-idle`
  went from 3.00 allocations per poll to 0.00. The general shape is worth keeping: a per-packet
  path that sleeps hides its own cost, and the allocation budget that passed for years was
  measuring the sleep.
- **The loss counter measured what survived repair, not what the wire did**: `packetsLost` and the
  `lossRuns` histogram are both tallied inside `Drop()`, so they only ever counted packets missing
  from frames that were *thrown away*. A packet FEC rebuilt, or one a NACK fetched back, left no
  trace anywhere. A loaded five-minute session over Tailscale, 5.7 Mbps median,
  measured the blind spot directly: 48 packets went absent, 41 of them never arrived, and 7 were
  fetched back by a NACK in time for the frame to complete. Those 7 — and one two-packet run that
  never reached the histogram — are exactly what the old counter cannot see, because it only ever
  looks at frames that were thrown away. Every Gilbert-Elliott parameter drawn from it would
  therefore have been a parameter of *unrepairable* loss, biased further the better FEC and NACK
  worked. (The `latePackets` counter is a different thing and not evidence here: it counts repairs
  that arrived *after* their frame was already dropped, and those losses were counted at drop
  time.) The fix does not
  scan for gaps; it marks a hole at the three moments a hole is actually visible — a packet filling
  an index below the highest one seen, `TryRecover` rebuilding a piece, and a frame leaving the
  queue with a piece still empty (which is the only way a lost tail is ever noticed, since no later
  index arrives to reveal it). Each absent packet then lands in exactly one of four bins: never
  arrived, repaired by FEC, repaired after a NACK, or merely reordered. Splitting that last bin out
  is load-bearing rather than decorative — reordering is not loss, and folding it in would inflate
  the burst model — so `wire_loss%` is `(everAbsent − reordered) / (received + neverArrived)`. The
  known bias: `nacked[i]` is set when `PlanNack` picks the index, so a packet both asked for and
  merely late is filed as a NACK repair, which errs toward calling it loss. `evt=sum` carries
  `wire_loss` / `absent` / `gone` / `nack_fix` / `reorder`, and a `wire runs` histogram sits beside
  the old `loss runs` one. Neither replaces the other: the old line is what the viewer suffered,
  the new line is what the link did, and a bake-off needs the second while a user report needs the
  first.
- **Give the encoder the power to act before you let the policy name the act**: `RecoveryPolicy`
  had been complete and inert since A4, its capability set empty on purpose. A host that declares
  long-term references without an encoder that keeps any answers a lost reference by setting
  `invalidateBeforeFrame`, having nobody read it, and never asking for the IDR it used to ask
  for — loss recovery would go from expensive to absent. So the execution landed first and
  `SetCaps` last. `IVideoEncoder` grew `MarkLongTermReference`, `InvalidateReference` and
  `BeginIntraRefresh`, with `static_assert`s binding it to `ReferenceInvalidatingEncoder` and
  `IntraRefreshEncoder` — the use those optional concepts were written for. NVENC runs LTR Per
  Picture (`enableLTR=1`, `ltrTrustMode=0`) over a four-slot ring with `maxNumRefFrames` raised to
  match: marks through `ltrMarkFrame`/`ltrMarkFrameIdx`, repairs through
  `ltrUseFrames`/`ltrUseFrameBitmap`, refreshes through `forceIntraRefreshWithFrameCnt`.
  `nvEncInvalidateRefFrames` is deliberately unused — the bitmap is the deterministic road,
  because it states what the next picture may reference instead of naming what broke and leaving
  the rest to inference. A driver that refuses LTR at `InitializeEncoder` gets one retry without
  it rather than taking the whole share down. Media Foundation takes `AVEncVideoLTRBufferControl`
  at init and then `MarkLTRFrame`/`UseLTRFrame`/`GradualIntraRefresh` per picture, and
  `RequestKeyFrame` now forgets the ring, because an IDR clears the DPB and keeping the record
  would be lying to ourselves. `deskhubp/host/EncoderRecovery.h` is the seam: `PrepareRecovery()`
  consumes `invalidateBeforeFrame` and `wantIntraRefresh`, falls back to an IDR whenever the
  encoder will not execute, and marks exactly the frames the policy will name later — the IDR
  included, since skipping it leaves `core` believing in a long-term reference the encoder does
  not hold. It is wrapped in `if constexpr` on the two concepts, so Linux, Apple and Android keep
  today's behaviour until their encoders grow the three calls. Windows calls
  `recovery.SetCaps(encoder->RecoveryCaps())` after *every* encoder creation, because `SetCaps`
  resets the policy as well — exactly what a rebuilt encoder needs, having lost every reference it
  held. The preamble lives in `EncodeTimed`, so the frame path and the flush path both go through
  it instead of carrying a copy each.
- **A class nothing calls is a data race waiting for its first caller**: `RecoveryPolicy` is
  touched from two threads — `OnReferenceLost` on the host net loop, `NoteEncoded` and
  `ShouldMarkLongTerm` on the encode thread under `encMutex` — and it carried no lock, which cost
  nothing for as long as no backend could execute a recovery and the path never ran. Switching
  Windows on made TSan report it on the first loss. The class now holds its own mutex and is no
  longer copyable; nothing copied it. A component parked behind an empty capability set is not
  shown to be thread-safe by a green test run, only left unexercised.
- **Half of a cross-platform optimisation is the platform that never got it**: P2 said the send
  path batches, and it did — on Linux. `UdpSocketWin::SendBatch` was a `sendto` loop, one syscall
  per datagram, so the Windows host paid the full per-packet cost while the note above described a
  batched sender. It now calls `WSASendMsg` with a `UDP_SEND_MSG_SIZE` control message, the
  Windows counterpart of `UDP_SEGMENT`, resolved once through `WSAID_WSASENDMSG` at `Open` and
  stood down for good — not per burst — when the stack refuses it, falling back to the same
  one-`sendto`-per-datagram loop. It is the cheap half of what P2 listed, which is why it comes
  before RIO. `LeadingRunOfEqualSegments` moved out of `UdpSocketPosix.cpp` and into
  `deskhubp/net/UdpSocket.h` so the two operating systems share one rule rather than two copies of
  it, and the rule that a short datagram may only be the last segment of a run is now held by a
  unit test instead of only by a loopback round trip. The loopback table above is Linux:
  `platform_perf` has not been run either side of this change on Windows, so those numbers do not
  transfer.
- **`enc_lat_ms` was never a capture number, and the capture clock had no reader**: C2 asks what
  Windows Graphics Capture costs in latency against DXGI Desktop Duplication, and the first
  finding was that the number did not exist rather than that it was not printed.
  `ScreenCapture.cpp` filled `fi.meta.timestampUs` from WGC's `SystemRelativeTime` and nobody
  read it: `SharingHost` took `width` and `height` off the frame and handed `Encode` a fresh
  `NowUs()`, so `enc_lat_ms` measures the encoder and says nothing about capture→texture. The two
  clocks already agree — `SystemRelativeTime` is QPC in 100 ns units and `NowUs()` on Windows is
  QPC too — so the difference was usable without an epoch conversion, which is why the whole
  question costs one call. `SourceDiag::NoteCapture` takes the frame's timestamp and the time it
  reached the host, adds the age to a `cap_us` percentile and counts a frame handed over a second
  time as `cap_repeat` instead of re-timing it, since a repeat's age is measured from the original
  capture and would inflate the tail. It lives in `core/` so the other four capture backends can
  wire it up with one line each, behind `ShareDiagCaps::captureLatency` so they print no empty
  column until they do. Answering "what does WGC's convenience cost" needs that column and nothing
  else; only a bad number justifies writing a Duplication backend, and if one is written it takes
  a `--capture wgc|dxgi` flag that stops when the named backend will not start — the same rule
  `--encoder` already follows, for the same reason. The column has now been read, on an Intel UHD
  750 capturing 3440 × 1440 and downscaling to 1920 × 802: across sixteen one-second windows
  `cap_us_p50` sits at 0.5-2 ms and `cap_us_p99` at 2-20 ms, with `cap_repeat=0` throughout — WGC
  hands a frame over in about the time the encoder then spends compressing it (`enc_us_p50`
  1.5-2.6 ms), so its convenience is cheap and **no Duplication backend is justified**. The one
  outlier is the first window after `Start`, where `cap_us_p99` reads 242 ms: that is the age of
  the very first frame, not a steady-state tail.
- **Receive coalescing mirrors the send side, and it costs one field in the read contract**: the
  remaining half of P2 was GRO on Linux and URO on Windows, and one thing blocked both — `RecvBatch`
  promised one datagram per slot. With `UDP_GRO` (Linux) or `UDP_RECV_MAX_COALESCED_SIZE` (Windows)
  a single read hands back a run of equal-sized datagrams in one buffer, with the segment size in a
  control message, so `InboundDatagram` gained a `segment` field and `DatagramsIn` / `DatagramAt`
  split a slot back into the datagrams that were sent. `segment == 0` means a slot holding exactly
  one datagram, which is every caller that does not ask for coalescing, so the old contract is the
  default rather than a special case. Coalescing is opt-in through `EnableReceiveCoalescing()` and
  never automatic, because a slot that is too small loses data: measured here, a 16 × 1200 byte GSO
  run arrives as one 19 200-byte coalesced skb, and reading it into a 2048-byte buffer returns 2048
  bytes with `MSG_TRUNC` set and **discards the other 17 152** — the next read finds nothing. The
  kernel will coalesce up to a full 64 KB IP payload, so only a caller whose slots hold
  `kMaxCoalescedBytes` may turn coalescing on, and `RecvBatch` logs the `MSG_TRUNC` case as an error
  naming that requirement rather than letting a burst vanish quietly. `QuicEndpoint` therefore
  trades slot count for slot size — 16 × 1350 bytes on the stack becomes 4 × 65 535 bytes owned by
  the endpoint — and the count turned out not to matter: 4, 8 and 16 slots all landed within
  run-to-run noise of each other, so the cheapest footprint won. Measured over loopback with the two
  binaries interleaved to cancel the machine's drift, medians of four pairs: a 16-datagram burst
  read 573 → 203 ns/datagram (2.8x), `quic/stream-drain-scaling` 2095 → 1629 ns/KB,
  `quic/stream-throughput-64k` 2114 → 1785 ns/KB. Rows whose code is identical in both binaries
  moved 3.3-5.1%, which is this machine's noise floor, so `quic/handshake` at +4.9% (13 → 15
  allocations, one of them the 256 KB read buffer) is not separable from it, and
  `quic/datagram-delivery` at +0.6% says reading the control message costs the uncoalesced path
  nothing. The Windows half is written to the same shape through `WSARecvMsg` and
  `UDP_COALESCED_INFO`, and it has now been compiled and run: the stack accepts
  `UDP_RECV_MAX_COALESCED_SIZE`, but over loopback it **coalesces nothing** — a 16 × 1200 byte USO
  run comes back as sixteen separate reads, each carrying `segment == 0`, and an undersized
  2048-byte slot therefore loses nothing, because there is no run to truncate. The 2.8x above stays
  a Linux result. On Windows the whole measurable win sits on the send side, where USO takes a
  16-datagram burst from 7183 to 3942 ns/datagram (−45%, medians of eleven runs), while receive
  batching and URO both land inside a noise floor of 45-60% — an order of magnitude wider than the
  Linux machine's 5%, and wide enough that nothing under roughly 1.5x can be measured there at all.
  Whether URO ever fires needs a real NIC; loopback cannot answer that.
- **A bake-off needs a clip before it needs a second backend**: C1 had three encoder columns and
  no comparison, because each one was measured on a different machine, at a different capture
  size, against whatever happened to be on that desktop, and none of them on a fixed clip. Worse,
  every number was latency: nothing measured what an encoder gave up to be fast.
  `scripts/encoder-bake-off.sh` closes both gaps with one command. It builds a clip (deterministic
  `testsrc2`, or `--clip FILE` for a real one), hands the *same* raw frames, size, fps and bitrate
  to every backend, and prints VMAF, `enc_us_p50`, `enc_us_p99`, CPU% and GPU% in one table with
  the clip's SHA-256 beside it so two machines can prove they measured the same pixels. The
  encoders take an `ID3D11Texture2D`, not a file, so the clip is fed by
  `client/windows/cpp/bench/EncoderBench.cpp` — a bench binary that uploads BGRA frames through a
  four-texture ring and times only the `Encode` call. Measuring ffmpeg's `h264_qsv` and
  `h264_nvenc` instead would have been a one-line script and would have answered a different
  question: those are not the code Deskhub ships. Two limits are printed with the table rather
  than buried: `cpu_pct` and `gpu_pct` are whole-process numbers that include the harness's own
  frame upload, and `testsrc2` is not desktop content — the synthetic clip makes runs comparable,
  not representative. A backend that will not start is left out of the table rather than measured
  under another backend's name, the same rule `--encoder` already follows.
- **Ten seconds of silence on loopback was a link waiting for an answer nobody gave**:
  `platform_tests` failed exactly eight checks in about one Windows run in five, across
  `HostLinkTests` and `FileTransferTests`, with a log that said a QUIC link had gone quiet on
  loopback for over ten seconds — which scheduling pressure does not explain, so the deadlines
  were not the thing to widen. The link was neither stalled nor slow: it was **parked**.
  `HostLink::SettleTrust` compares the key the host presents against `known_hosts`, and on
  `TrustVerdict::Changed` it moves to `Deciding` and waits for a person to accept or reject. A
  parked link sends nothing, so both ends report the other silent; the test supplies no
  `onTrustAsked` handler and never accepts, so it waits out its own deadline. Everything else in
  the incident follows: "a terminal record goes out" still passes, because the QUIC connection is
  up; the echo comes back and is swallowed by the `Deciding` read loop instead of reaching a
  channel, which fails two more checks; and `FileTransferTests` fails three more on its own port
  for the same reason. Planting one valid but different fingerprint for `127.0.0.1:47845` and
  `127.0.0.1:47836` reproduces all eight failures by name, with the same silence, on demand.
  **The key differed because the tests had no state of their own on Windows.**
  `KeepTestLogsOutOfTheDeveloperHome()` moved `HOME` aside on POSIX and was an empty function on
  Windows, so both test binaries read and wrote `%USERPROFILE%\.deskhub` — the same single
  `host_cert.pem`, `known_hosts` and `paired_devices` used by the installed app and by every other
  Deskhub process on the machine. On top of that, the one shared identity is scratch space for the
  suites: a run creates about fifty of them, each snapshotting the previous pair and restoring it
  afterwards, and three of the four files that did this restored at the end of a function with
  four to six early `return`s in between. Any early exit, any kill, or any write from a live app
  leaves the next `HostLinkTests` run comparing this run's key against last run's record. The fix
  is in four parts, none of them a wider deadline: both test mains now point `SetAppDataDir` at a
  private directory on Windows too; the RAII `SavedIdentity` guard that `SessionTransportTests`
  already had moved into `TestSupport.h` and replaced every manual restore; the tests that dial a
  fixed endpoint take a `ForgottenHost` guard so their verdict does not depend on what an earlier
  run left behind; and `SettleTrust` now logs the moment it parks, naming the key it saw, so the
  next silence explains itself in the log instead of looking like a dead handshake.
- **A log line assembled in three `printf`s is not one line**: the same capture showed
  `[Deskhub] [Deskhub] quic: …silencequic: …silence` — two threads interleaved mid-line, which
  cost real time during the hunt because the check name that mattered was inside the corruption.
  The Windows `LOGI` was `printf("[Deskhub] ")`, then the caller's format, then `printf("\n")`:
  three chances for another thread to land between them, and the CRT only locks `stdout` for the
  duration of one call. It now formats tag, body and newline into one buffer and emits it with a
  single `fputs`, which is the shape the POSIX path already had. This is why the Android and Apple
  branches are left alone: `__android_log_print` and the single `fprintf` are already one call
  each.
