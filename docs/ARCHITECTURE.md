**English** · [Tiếng Việt](ARCHITECTURE.vi.md)

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
online state (ping/pong probes) and the LAN scan feed one merged device list on
Windows.

## 7. Data on disk

Everything lives in the user's Deskhub folder (`~/.deskhub`,
`%USERPROFILE%\.deskhub`): `host_key.pem` + `host_cert.pem` (identity),
`known_hosts` (hosts this machine trusts), `paired_devices` (machines this host
admits), `auth_salt` (non-secret verifier salt), `ui-settings.txt`,
`recent-devices.txt` (addresses + obscured passcodes), and per-run logs. File I/O
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

CI additionally enforces clang-format and clang-tidy (both pinned), SwiftLint
`--strict`, Android Lint, actionlint + shellcheck, ASan/TSan runs of all three suites,
CodeQL over C++/Kotlin/Swift, a gitleaks sweep of the whole history, and ≥ 90 % line /
80 % branch coverage on `core/`. The three suites are additionally cross-built and run
on arm64 Linux, an Android emulator and the iOS Simulator. The Linux and macOS release
jobs also run `core_perf` and `platform_perf` with their allocation and scaling gates
(no time baseline exists on a shared runner), and each pull request additionally gets
a perf-and-lag report posted as one self-updating comment: both perf suites A/B'd
against the base commit on the same runner (drift as warnings, never a failure), the
under-load integration numbers from the pull-request build, and the core coverage
line.

## 9. Decisions worth remembering

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
