**English** · [Tiếng Việt](fec-under-burst-loss.vi.md) · [中文](fec-under-burst-loss.zh.md) · [日本語](fec-under-burst-loss.ja.md)

# I raced five ways of surviving packet loss. None of them passed.

*Deskhub is a remote-desktop tool. This is the write-up of its A1 bake-off: forward error
correction under burst loss. The headline is a negative result, so the numbers below are mostly
the numbers where my own defaults lost.*

---

## Read this before any table

These are **simulation** numbers. Every row comes from a seeded, deterministic model that runs
inside `core_tests` with no network card and no GPU anywhere near it. That buys reproducibility —
two runs on two machines produce byte-identical CSVs — and it costs realism in four specific ways
that you should carry into every table below:

| The model has no… | Which means |
| --- | --- |
| **jitter** | one-way delay is a constant 20 ms (2 ms for the near-RTT rows). Every packet that is not dropped arrives exactly on time, so a NACK's round trip is exactly `2 × delay` and never worse. Real links stretch that tail, and NACK is the scheme that suffers when they do. |
| **reordering** | packets are delivered in send order. The reassembler's overtaken/reorder logic is therefore never exercised, and any scheme that would have been confused by reordering looks cleaner here than it is. |
| **congestion** | packets are dropped only by the loss model. There is no queue, so no drop is ever caused by the repair traffic itself. This is the big one: the parity overhead in the tables below is **free** in the simulation and is **not** free on a real link. |
| **real video** | a frame is `Rnd()` bytes — 8 packets for a delta frame, 40 for an IDR, always. No scene cuts, no size distribution, no content-dependent damage. "Frame damaged" means "at least one non-parity packet of it was dropped", and says nothing about how bad it looked. |

And one more, which is a process failure rather than a modelling one:

> **The pass/fail thresholds in this post were set *after* part of the data was already visible.**
> The rule for this bake-off was to fix the objective function and the bar before looking at any
> number. That rule held for the objective function and broke for the bar: the earlier parameter
> sweep and the hardware measurements had already run when the thresholds were written down. Read
> them as post-hoc bars, not as predictions. The negative result below survives that — nothing
> passed a bar I set *while already knowing the data* — but a positive result would not have.

## The question

Deskhub sends video as UDP datagrams. When a packet is lost, the viewer's reassembler cannot
finish the frame, the frame's reference is gone, and the viewer asks the host for a keyframe. A
keyframe is 40 packets where a delta frame is 8, so a link that is losing packets gets punished
twice: once by the loss, and again by the IDR storm the loss triggers.

So the objective function is **IDR requests per minute**, not rescue percentage. Rescue percentage
is how the schemes get argued about; IDR/min is what the user feels.

The bar, written down after seeing part of the data:

- **≤ 2 IDR/min** at the operating point actually measured on home WiFi (0.1% loss, bursts of 1)
- **≤ 20 IDR/min** at a deliberately hostile point (5% loss, bursts of 4)
- and the winner may cost at most **3×** the XOR encoder's CPU, unless it rescues **≥ 2×** the
  frames

Five contenders: XOR with the interleave depth derived from the group count (what ships), XOR with
depth set independently, Reed–Solomon over GF(256), Reed–Solomon with the parity row count driven
by measured loss, and NACK-only retransmission. `fec-sweep.csv` crosses them with parity rows
(1/2/3), interleave depth (derived/4/8), loss model (uniform / Gilbert–Elliott), loss rate
(0.1 / 1 / 5%) and RTT (4 / 40 ms) — 180 rows. `nack-hybrid.csv` takes the repair-mode question on
its own, across RTT 4 / 20 / 40 / 80 ms and a viewer hold window — 72 rows.

## Result 1: at 5% loss and bursts of 4, nothing passed

The bar was 20 IDR/min. The sweep contains 36 rows at that operating point. The best of them is
36 IDR/min — and it buys that by sending **143 parity packets for every 100 data packets**.

| scheme | parity rows | depth | overhead | rescued | **IDR/min** |
| --- | ---: | ---: | ---: | ---: | ---: |
| xor (shipped) | 1 | derived | 12.5% | 33.87% | **171.00** |
| xor | 1 | 8 | 90.9% | 78.69% | **81.00** |
| rs | 2 | 8 | 188.7% | 86.67% | **45.00** |
| rs | 3 | 4 | 142.9% | 90.57% | **36.00** ← best in the sweep |

> **A caveat that belongs beside this table, not at the end of the post:** the overhead column is
> why this is a negative result and not a shopping list. The simulation has no congestion, so
> 142.9% overhead costs nothing there. On a 20 Mbps link it would mean roughly 8 Mbps of video and
> 12 Mbps of parity — which would drive the bitrate controller down, shrink the frames, and change
> every other number in that row. The model cannot show that. So the bottom row is not "the
> winner"; it is the point where the model stops being able to answer.

Find those rows:

```sh
awk -F, 'NR==1 || ($6=="5.00" && $7=="4.0")' docs/data/bake-off/fec-sweep.csv
```

## Result 2: Reed–Solomon and XOR produced identical numbers on all 40 shared points

`fec-sweep.csv` holds 40 parameter points where an `xor` row and an `rs` row differ in nothing but
the scheme name. On all 40, **every one of the other 29 columns is character-for-character
equal**. Not close — equal.

```sh
python - <<'PY'
import csv, collections
rows = list(csv.DictReader(open('docs/data/bake-off/fec-sweep.csv')))
params = ('fec', 'armed_from_feedback', 'groups', 'model', 'loss_pct', 'burst_pkts', 'seed',
          'parity_rows', 'nack', 'rtt_ms', 'overtaken_limit')
by = collections.defaultdict(dict)
for r in rows:
    by[tuple(r[k] for k in params)][r['scheme']] = r
pairs = [(v['xor'], v['rs']) for v in by.values() if len(v) == 2]
same = sum(all(a[k] == b[k] for k in a if k != 'scheme') for a, b in pairs)
print(len(pairs), 'pairs,', same, 'identical')
PY
```

```
40 pairs, 40 identical
```

This is not a bug, and it stops being a surprise the moment you state it: **all 40 of those points
have `parity_rows=1`, and a Reed–Solomon code with one parity row *is* XOR.** The Cauchy matrix
degenerates to a row of ones. I wrote a GF(256) field, a Cauchy matrix and a Gaussian elimination
decoder, and at the parity setting that actually ships, all of it recomputes the parity byte a
two-line XOR loop already produced.

What it does not reproduce is the cost. On this machine, `core_perf`'s `video/fec-encode-rs` row
takes **143 µs/frame** against `video/fec-encode-xor`'s **22.7 µs/frame** — 6.3×. *(That pair is a
timing, not a simulation: it comes from `core_perf` on one Intel desktop and does not reproduce
byte for byte the way the CSVs do. It is the only number in this post that is not a CSV row, and
it is called out here rather than mixed into a table.)*

So Reed–Solomon starts being worth its keep at 2 parity rows, and at 1 parity row it is a 6.3× tax
for a bit-identical result. The shipped default stays XOR.

## Result 3: interleave depth is not the cheap lever, and I wrote that it was

The first draft of this analysis called interleave depth "the cheapest lever in the grid", on the
grounds that spreading a frame across more FEC groups costs no extra CPU. That is true, and it is
irrelevant.

| scheme | parity rows | depth | overhead | rescued | IDR/min |
| --- | ---: | ---: | ---: | ---: | ---: |
| xor | 1 | derived | **12.5%** | 33.87% | 171.00 |
| xor | 1 | 4 | **43.9%** | 62.90% | 117.00 |
| xor | 1 | 8 | **90.9%** | 78.69% | 81.00 |

Same scheme, same parity rows, same loss, same seed. Depth 8 halves the IDR rate — and takes
overhead from 12.5% to 90.9%, because a delta frame is 8 packets: at depth 8 each group holds one
packet, so each packet carries parity for itself alone. That is not coding, that is sending
everything twice. On a 20 Mbps budget it means spending half the picture to halve the keyframe
requests.

> **Caveat:** the 8-packet delta frame is a constant in the model, not a measurement. Real delta
> frames vary in size, and the degenerate depth-8 case is partly an artefact of that constant.
> What survives the artefact is the shape: depth costs bandwidth in proportion to how far it
> outruns the packet count, and it is never free.

```sh
awk -F, 'NR==1 || ($1=="xor" && $2=="1" && $3=="0" && $6=="5.00" && $7=="4.0" && $29=="40")' \
    docs/data/bake-off/fec-sweep.csv | sort -u
```

## Result 4: the shipped arming policy loses to leaving FEC on — and to not having it at all

This is the table where Deskhub's own default is the worst option shown.

FEC is not always on. A policy watches the feedback and arms FEC when reported loss looks bad
enough. At the operating point home WiFi actually produced — 0.1% loss, bursts of 1, over 1800
frames — here is that policy against three alternatives:

| configuration | FEC armed | overhead | frames rescued | **IDR/min** |
| --- | ---: | ---: | ---: | ---: |
| no repair at all | — | 0.0% | 0/16 | 32.00 |
| FEC always on | 100.0% | 12.5% | 16/16 | **0.00** |
| **shipped arming policy** | 30.0% | 4.1% | 6/16 | **20.00** |
| NACK only, RTT 4 ms | — | **0.0%** | 15/16 | **2.00** |
| NACK only, RTT 40 ms | — | 0.0% | 0/16 | 32.00 |

The bar at this operating point was ≤ 2 IDR/min. The shipped configuration misses it by 10×. The
policy spends 70% of its time disarmed because the reported loss is rounded before the threshold
compares it, and 0.1% rounds to 0.

The row that passes is the one with **no forward error correction at all**: NACK-only on a short
link, zero bandwidth overhead, 15 of 16 damaged frames rescued. That inverts an earlier conclusion
of mine — recorded in the same working document — that retransmission was not worth implementing.

> **Caveat, and it is a large one:** that NACK row is the model's no-jitter assumption paying out.
> At RTT 4 ms the retransmission always arrives before the frame is due, because the model
> guarantees it arrives in exactly 4 ms. The RTT 40 ms row directly beneath it — 0 of 16 rescued,
> 32 IDR/min — is what happens once the round trip no longer fits inside the deadline. A real 4 ms
> link with a jitter tail lives somewhere between those two rows, not on the good one.

```sh
awk -F, 'NR==1 || ($6=="0.10" && $7=="1.0" && $9=="1800")' docs/data/bake-off/fec-sweep.csv
```

## Result 5: repair mode against RTT — where each one breaks

`nack-hybrid.csv` runs the three repair modes across four RTTs and a viewer hold window, at 1% and
5% loss with bursts of 4. The interesting part is not the best case; it is the shape of each
mode's failure.

At **5% loss, RTT 4 ms**, no hold window:

| repair | overhead | rescued | IDR/min |
| --- | ---: | ---: | ---: |
| fec-only | 12.5% | 29.51% | 180.00 |
| nack-only | **0.0%** | 57.69% | 126.00 |
| fec+nack | 12.5% | **68.42%** | 126.00 |

At **5% loss, RTT 80 ms**, the same three rows on a long link:

| repair | overhead | rescued | IDR/min |
| --- | ---: | ---: | ---: |
| fec-only | 12.5% | **31.25%** | 153.00 |
| nack-only | 0.0% | **7.69%** | 180.00 |
| fec+nack | 12.5% | 25.42% | 153.00 |

NACK's rescue rate falls from 57.69% to 7.69% as the round trip grows, and it drags the hybrid
down with it: `fec+nack` at RTT 80 is *worse* than `fec-only` at RTT 80, because the frames it
spends its NACK budget on are already past due. A viewer hold window recovers most of that — at
RTT 80 with `hold_frames=8`, `fec+nack` returns to 67.21% rescued and 63.00 IDR/min, which ties
the lowest IDR/min any 5%-loss row in the file reaches — but holding 8 frames shows up as 116 ms in `longest_stall_ms`, on a tool
whose entire selling point is that it feels immediate.

> **Caveat:** `hold_frames` is the one column here that trades a measured good number for an
> unmeasured bad one. Latency added at the viewer is not in the objective function at all, so this
> table can rank hold windows and cannot tell you which of them a user would tolerate.

```sh
awk -F, 'NR==1 || ($4=="5.00" && $3=="0")' docs/data/bake-off/nack-hybrid.csv
```

## What shipped

Nothing. The default is where it started: XOR, one parity row, derived depth, armed by the
existing policy. Reed–Solomon, adaptive parity and NACK stay behind `--fec=`, `--fec-depth`,
`--fec-parity` and `--nack` as measurement flags — available for the next round, kept out of the
fuzz and sanitizer matrices so CI does not pay for code nobody runs, and not offered to end users,
who have no data with which to choose.

Three things the bake-off did produce:

1. A negative result strong enough to stop a rewrite: RS at 1 parity row is XOR with a 6.3× CPU
   bill, so the scheme was never the bottleneck.
2. A ranking of what is: the **arming policy** costs 20 IDR/min at the measured operating point,
   more than any scheme choice in the sweep is worth. That is the next thing to fix.
3. A reversal: NACK-only had been written off without measurement, and on short links it is the
   only configuration in the file that meets the bar — at zero overhead.

The honest summary is that the question I set out to answer ("XOR or Reed–Solomon?") was the wrong
question, and the sweep was worth running mostly because it said so.

## Reproduce all of it

```sh
git clone https://github.com/manhpham90vn/Deskhub
cd Deskhub
make test                  # builds and runs core_tests offline
scripts/bake-off-csv.sh    # splits the [csv] lines into out/bake-off/*.csv
diff out/bake-off/fec-sweep.csv docs/data/bake-off/fec-sweep.csv
diff out/bake-off/nack-hybrid.csv docs/data/bake-off/nack-hybrid.csv
```

The two `diff` lines should print nothing. Every row is produced by a seeded simulation — the seed
is `0x5DEECE66D`, printed in the `seed` column of every row of `fec-sweep.csv` — with no network
and no GPU, so the files reproduce byte for byte on any machine that can build the tests.

- Raw data: [`docs/data/bake-off/fec-sweep.csv`](../data/bake-off/fec-sweep.csv) (180 rows) ·
  [`docs/data/bake-off/nack-hybrid.csv`](../data/bake-off/nack-hybrid.csv) (72 rows)
- The script that produces them: [`scripts/bake-off-csv.sh`](../../scripts/bake-off-csv.sh)
- The simulation itself:
  [`core/tests/transport/LossGoodputTests.cpp`](../../core/tests/transport/LossGoodputTests.cpp)
- What each table concluded, in project terms: [`docs/ARCHITECTURE.md`](../ARCHITECTURE.md)

Column meanings are in the CSV header. `rescue_pct` is `frames_rescued / frames_damaged`;
`overhead_pct` is parity packets over data packets; `idr_per_min` is keyframe requests scaled by
`frames_sent × 16 667 µs`; `longest_stall_ms` is the largest gap between two delivered frames.
