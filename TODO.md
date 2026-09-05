# Core Bake-off

> Tài liệu làm việc nội bộ, viết bằng tiếng Việt và đứng ngoài scheme bốn ngôn ngữ mô tả
> trong `CLAUDE.md` — giống `CLAUDE.md`, đây không phải tài liệu xuất bản cho người dùng.
> Nội dung thay đổi theo tiến độ nên một bản dịch sẽ lệch ngay sau lần cập nhật đầu tiên.

Kiểm kê từng thành phần có không gian thuật toán trong `core/`, `platform/` và `client/`,
liệt kê các phương án triển khai của mỗi loại, đối chiếu với tool trên thị trường, rồi cho
chúng chạy đua sau một contract chung — chọn bằng số đo, không chọn bằng danh tiếng.

**Vòng lặp:** Kiểm kê → Không gian thiết kế → Đo thực tế → Bake-off → Chốt bản thắng, giữ bản
thua làm tham chiếu

Phạm vi kiểm kê: `core/` 98 header 12 nhóm · `platform/` 67 header 10 nhóm · `client/` 5 app,
cộng `client/apple` (Swift dùng chung) và `client/cli`. Tính từ nhánh
`windows-transfer-and-asan` ngày 01/09/2026. Mọi hằng số và số đếm trích dẫn dưới đây đã được
đối chiếu lại với cây nguồn hiện hành.

# Việc cần làm — đã chuyển sang GitHub Issues

**Từ 04/09/2026, mọi việc còn mở được theo dõi bằng GitHub Issue, không phải bằng file này.**
Mỗi issue ghi rõ *Bối cảnh · Yêu cầu · Tiêu chí Done · Đang chặn bởi*.

👉 https://github.com/manhpham90vn/Deskhub/issues

Phần còn lại của tài liệu này là **sổ đo**: bảng số, kết luận, và những cảnh báo phương pháp phải
đọc kèm mỗi lần trích số. Nó không còn là danh sách việc — đừng thêm checkbox mới vào đây.

## Bảng tra: việc mở, theo nhóm chặn

| Issue | Việc | Chặn bởi |
| --- | --- | --- |
| [#54](https://github.com/manhpham90vn/Deskhub/issues/54) | Bắt loss và jitter thật trên link thứ ba | link chật |
| [#55](https://github.com/manhpham90vn/Deskhub/issues/55) | Rút tham số Gilbert-Elliott từ loss thật | link chật |
| [#56](https://github.com/manhpham90vn/Deskhub/issues/56) | A/B `RawUdp` vs `QuicDatagram` | link chật |
| [#57](https://github.com/manhpham90vn/Deskhub/issues/57) | Quyết kiến trúc hai tầng CC | chờ #56 |
| [#58](https://github.com/manhpham90vn/Deskhub/issues/58) | Ghi quyết định CC vào ARCHITECTURE ×4 | chờ #57 |
| [#59](https://github.com/manhpham90vn/Deskhub/issues/59) | netem + camera 240 fps: sim có nói dối không | bộ đo glass-to-glass |
| [#60](https://github.com/manhpham90vn/Deskhub/issues/60) | Kịch bản đo encoder chung (VMAF) | dựng xong — còn lần chạy thứ hai trên cùng máy Windows |
| [#61](https://github.com/manhpham90vn/Deskhub/issues/61) | Bake-off encoder trên 3 backend còn lại | macOS · Android · Linux-GPU |
| [#62](https://github.com/manhpham90vn/Deskhub/issues/62) | Bảng tra `EncoderFactory` theo vendor | thiếu máy AMD |
| [#63](https://github.com/manhpham90vn/Deskhub/issues/63) | A4 thi hành trên từng backend | máy NVIDIA + các OS khác |
| [#64](https://github.com/manhpham90vn/Deskhub/issues/64) | C3 4:4:4 — điền mask hay viết decode | chờ quyết định |
| ~~[#65](https://github.com/manhpham90vn/Deskhub/issues/65)~~ | Viết bài từ dữ liệu bake-off | **xong** — `docs/posts/fec-under-burst-loss.md` ×4 ngôn ngữ |
| [#66](https://github.com/manhpham90vn/Deskhub/issues/66) | `platform_tests` fail 8 check ~20% trên Windows | gốc đã rõ + đã vá — còn 20 lần chạy liên tiếp trên Windows |
| [#67](https://github.com/manhpham90vn/Deskhub/issues/67) | Kiểm chứng `overtakenLimit=8` trên link thật | cần netem |

**Đã đóng:** chỉ [#65](https://github.com/manhpham90vn/Deskhub/issues/65).
[#60](https://github.com/manhpham90vn/Deskhub/issues/60) và
[#66](https://github.com/manhpham90vn/Deskhub/issues/66) đã dựng và đã vá xong phần viết được ở
đây, nhưng tiêu chí Done cuối cùng của cả hai chỉ chạy được trên máy Windows: một lần đo encoder
thứ hai để chứng minh tái lập, và 20 lần `platform_tests` liên tiếp. Nửa `netem` của
[#59](https://github.com/manhpham90vn/Deskhub/issues/59) và
[#67](https://github.com/manhpham90vn/Deskhub/issues/67) cũng chạy được ngay trên máy Ubuntu.

Rà lại 05/09 tìm thấy lỗ cùng loại với [#66](https://github.com/manhpham90vn/Deskhub/issues/66)
vẫn còn mở: chỉ 1 trong 7 bài `HostLinkTests` có `ForgottenHost`, còn bài cắm khoá rác thì khôi
phục bằng lệnh tay ở cuối hàm, và `TestASenderAcceptsAChangedHostKey` có `return` sớm ngay sau khi
cắm. Nay mọi bài quay số tới một cổng cố định đều cầm guard RAII, `SavedIdentity` dùng chung giữ
luôn cả `known_hosts` và xoá lại đúng thứ nó thấy lúc vào, và hai bản `SavedIdentity` chép tay
trong `HostIdentityTests`/`AuthProofTests` đã bỏ. 10 lần chạy liên tiếp trên macOS xanh — nhưng
macOS **không** biên dịch nhánh Windows của phần vá, nên nó không thay 20 lần trên Windows được.

## Trạng thái phần cứng đã dùng để đo

| Máy | Dùng cho | Ghi chú |
| --- | --- | --- |
| Ubuntu | viewer trong mọi bài đo mạng | `--fec` `--nack` `--hold` `--video-path` **chỉ chạy trên viewer Linux** |
| Windows + NVIDIA RTX 5070 Ti | NVENC, MF-qua-NVIDIA (02/09) | |
| Windows + Intel UHD 750 | MF-qua-Quick-Sync, USO/URO, C2 (04/09) | **không có driver NVIDIA** |
| iPhone | host trong bài đo link chật | |
| máy AMD | — | **chưa có** |

⚠️ `make test-asan` và `make test-tsan` **fail ngay trên HEAD chưa sửa gì** ở máy Ubuntu: ASan báo
leak trong `libpipewire`, TSan báo đua trong `libglib`. Cả hai là thư viện của desktop session,
không phải code mình — CI chạy headless nên hai đường ấy không bao giờ chạy ở đó. Muốn đọc kết quả
ASan/TSan thật thì đọc job của CI.

# Kiểm kê: thành phần nào có không gian thuật toán, và nó đã ngã ngũ chưa

Ba lớp, ba vai trò: `core/` là logic thuần không OS header · `platform/` là một API duy nhất
che khác biệt OS · `client/*` là thứ chỉ OS đó mới làm được. Luật chọn chỗ đặt code nằm ở
`CLAUDE.md`, không nhắc lại ở đây.

**Không phải cái gì cũng cần interface.** Tier A là chỗ đáng dựng `virtual` để chạy đua; Tier B
chỉ cần quét tham số; Tier C để yên. Dựng interface cho Tier B/C là trả giá trừu tượng cho một
cuộc đua không có ai chạy.

### Tier A — đã dựng interface và đã chạy đua

| | Thành phần | Hiện trạng | Kết luận |
| --- | --- | --- | --- |
| **A1** | Sơ đồ FEC | `xor` + `rs` sau `IFecScheme`, depth tách khỏi `numGroups` | **không có bản thắng** — xem Phase 3 |
| **A2** | Điều khiển tắc nghẽn | AIMD + delay-gradient + SCReAM sau `Update()` | quét xong, chưa có caller production |
| **A3** | Jitter buffer âm thanh | target cố định vs thích ứng | quét xong, đường cong ở Phase 5 |
| **A4** | Phục hồi sau mất gói | `RecoveryPolicy` ở `core/`, thi hành ở encoder | Windows xong, [#63](https://github.com/manhpham90vn/Deskhub/issues/63) |
| **A5** | Ước lượng lệch đồng hồ | ba bản, so trong sim có drift | quét xong |
| **A6** | Nhịp phát hình | pacing thích ứng + khớp vsync | **khớp vsync thắng tuyệt đối** — spread về 0, không tốn độ trễ |
| **P1** | Hai tầng CC chồng nhau | quiche CUBIC + AIMD của mình | **chưa quyết** — [#56](https://github.com/manhpham90vn/Deskhub/issues/56) → [#57](https://github.com/manhpham90vn/Deskhub/issues/57) |
| **P2** | Gộp syscall UDP | GSO/GRO (Linux) · USO/URO (Windows) | xong trừ RIO — số ở Phase 6 |
| **C1** | Chọn backend encoder | `--encoder auto\|nvenc\|mf\|vaapi\|videotoolbox` | 3/6 cột, [#60](https://github.com/manhpham90vn/Deskhub/issues/60) [#61](https://github.com/manhpham90vn/Deskhub/issues/61) [#62](https://github.com/manhpham90vn/Deskhub/issues/62) |
| **C2** | Backend capture | WGC, có `cap_us` đo được | **xong** — WGC rẻ, không cần DXGI |
| **C3** | Codec và chroma | `NegotiateCodec` có, nhưng **không đầu nào điền mask** | [#64](https://github.com/manhpham90vn/Deskhub/issues/64) |

### Tier B — quét tham số, đừng dựng interface

`LinkRecovery` backoff (đã thêm jitter) · `kStallTimeoutMultiple` · luật overtaken cạnh RTT ·
ngưỡng bật FEC · kích thước ring NACK. Những thứ này chỉ có **một** cách làm đúng, khác nhau ở
con số — quét bằng test tham số hoá, không bằng `virtual`.

### Tier C — để yên

Packetizer wire format · SPAKE2 handshake · cấu trúc `Reassembler` · logging · discovery. Không
có không gian thuật toán, hoặc chi phí đổi lớn hơn mọi phần thắng có thể có.

### Ba luật của harness, vẫn áp dụng

1. **Chốt hàm mục tiêu và ngưỡng đạt trước khi nhìn số.** Bảng ngưỡng ở Phase 0. ⚠️ Kỷ luật này
   **đã bị phá một phần** — ngưỡng được đặt sau khi sweep Phase 2 đã chạy, ghi rõ ở đó.
2. **Giữ bản thua làm tham chiếu, chỉ chạy bản thắng.** Bản thua ở lại sau cờ `--fec=` /
   `--cc=` / `--encoder=` và **ra khỏi** ma trận fuzz/sanitizer, để CI không trả giá cho code
   không ai chạy.
3. **Không cho người dùng cuối chọn thuật toán.** Các cờ trên là để **đo**, không phải để cấu
   hình. Người dùng không có dữ liệu để chọn, và mỗi lựa chọn phơi ra là một tổ hợp CI phải gác.

## Kế hoạch theo phase

### Phase 0 — Sửa baseline, rồi đo thực tế trước khi mô phỏng

*Chặn mọi thứ phía sau. Hai việc đầu là code, phần còn lại là đo.*

- [x] Cắt gói parity xuống theo gói dữ liệu lớn nhất trong group **(A1)** — không sửa thì mọi phép đo sau đều tính trên overhead sai
- [x] Đếm datagram bị `SendDatagram` từ chối, phơi ra dòng diag cạnh `e2e_ms` **(P1, chẩn đoán)** — `QuicSendStats` đã có sẵn chỗ
- ⏳ [#54](https://github.com/manhpham90vn/Deskhub/issues/54) — Bắt loss và jitter thật trên ba link: WiFi nhà, WiFi quán, Tailscale qua WAN
  — **2/3 xong**: WiFi nhà (01/09) và Tailscale qua WAN (03/09, hai phiên). **Còn link chật.**
  Kế hoạch: đo trên **WiFi công ty** thay cho WiFi quán — cùng loại chế độ (nhiều máy, nhiễu, AP
  chia sẻ) mà chủ động về thời gian hơn. Bố trí **bắt buộc: iPhone làm host, Ubuntu làm viewer** —
  `wire_loss` nằm trong `Reassembler`, tức phía nhận, nên video phải đi qua chỗ chật *rồi mới* tới
  máy có dụng cụ đo; đảo vai là mất trắng phép đo. Không cần build lại iPhone: bản cũ vẫn nói
  chuyện được vì không có thay đổi nào trên dây, chỉ mất `dgram_refused` phía host — mà đó là câu
  hỏi P1, đã trả lời trên các link khác
- [x] Đối chiếu: bao nhiêu phần "loss" là của mạng, bao nhiêu là tự mình bỏ ở hàng đợi
  — **xong trên mọi link đã đo**: LAN + WiFi nhà (01/09) và Tailscale WAN (03/09, host đã
  instrument ở cổng 47778). `dgram_refused = 0` ở mọi cửa sổ, tới 12,7 Mbps. **Phải làm lại trên
  WiFi công ty** — chưa link nào bị đẩy tới bão hoà, nên đó mới là link có cơ hội cho kết quả khác 0,
  và cũng là link duy nhất còn có thể lật kết luận này
- ⏳ [#55](https://github.com/manhpham90vn/Deskhub/issues/55) — Rút ra tham số Gilbert-Elliott từ phần loss thật: tỉ lệ mất, độ dài burst trung bình, phân bố
  — **dụng cụ đã xong và đã đối chứng với đáp án biết trước (03/09)**: bơm một quá trình
  Gilbert-Elliott có seed (5% loss, burst trung bình 4) qua 400 frame, bộ đếm dựng lại **đúng từng
  gói** (`packetsEverAbsent` khớp chính xác số gói đã bơm) và **đúng từng thùng** của histogram
  burst. Đây là điều kiện cần: một dụng cụ không dựng lại nổi tham số đã biết trong sim thì không
  dựng lại nổi tham số thật ngoài đời. Xem "Bộ đếm gói từng vắng mặt" dưới đây. Còn chặn bởi
  **số mẫu**:
  hai phiên trên cùng một link cho hai bức tranh trái ngược, nên phải đo nhiều phiên rải theo giờ
  rồi mới chốt tham số
- [x] Chốt hàm mục tiêu và ngưỡng đạt **trước khi** nhìn thấy bất kỳ con số nào — xem bảng dưới

**Quyết định: bỏ LAN có dây khỏi kế hoạch (03/09).** Không phải bỏ sót. LAN không nằm trong ba link
mà mục này liệt kê; nó có trong bảng 01/09 chỉ vì đo cùng lúc cho tiện. Bảng đó cho 0/93 cửa sổ mất
gói và RTT phẳng 14,8–15,1 ms — một link không có chế độ mất gói thì mọi sơ đồ FEC đều hoà, tức là
một phép đo không phân biệt được gì cho A1. Thứ duy nhất LAN còn cho được là kiểm tra bộ đếm mới có
báo nhầm ở bitrate cao không; phần đó coi như đã có từ phiên WAN tối 03/09 (`reordered = 0` trên 308
cửa sổ ở 5,7 Mbps, `absent = 0` trên 99 cửa sổ lúc màn hình tĩnh). Nếu sau này muốn chắc thêm thì
một phiên LAN 2 phút là đủ.

#### Hàm mục tiêu và ngưỡng đạt

> **Kỷ luật đã bị phá một phần, phải nói rõ.** Mục này yêu cầu chốt *trước khi* thấy bất kỳ
> con số nào, nhưng sweep Phase 2 và các phép đo trên phần cứng đã chạy trước. Các ngưỡng
> dưới đây vì thế được đặt *sau* khi đã thấy số. Ai đọc lại cần biết điều đó để không coi
> chúng là dự đoán độc lập.

| Thành phần | Hàm mục tiêu | Ngưỡng đạt | Đo ở đâu |
| --- | --- | --- | --- |
| **A1** FEC | Số IDR do `KeyframeReason::Loss` trên mỗi phút | ≤ 2/phút ở điểm vận hành đo được (0,1% loss, burst 1); ≤ 20/phút ở 5% loss burst 4 | `LossGoodputTests`, cột `idr_per_min` |
| **A1** chi phí | CPU encode mỗi frame | Bản thắng ≤ 3× bản XOR, nếu không thì phải cứu được ≥ 2× số frame | `core/perf`, `video/fec-encode-*` |
| **A2** CC | Thời gian hồi phục sau khi băng thông tụt · bitrate trung bình giữ được | Không bản nào để bitrate leo lên khi `frameAgeMs ≥ kSevereBacklogMs` (ca Pixel 4) | `ControlTests` |
| **A3** audio | `(underruns + framesConcealed)` mỗi phút, vẽ theo trục độ trễ trung bình | Ở cùng số gián đoạn, target thích ứng phải giữ độ trễ thấp hơn target cố định | `AudioJitterBufferTests` |
| **A5** đồng hồ | Sai số so với offset thật trong sim có drift đã biết | Trên link có drift, bản thắng sai số ≤ 1/4 bản rolling-min | `ClockOffsetTests` |
| **A6** nhịp hình | Số pha khác nhau mà frame rơi vào trong một chu kỳ quét | Khớp vsync phải cho đúng 1 pha | `VideoPacerTests` |
| **P1** hai tầng CC | Tỉ lệ `dgram_refused / dgram_tx` | > 0,1% thì kết luận của A1/A2 phải đo lại | dòng `[DIAG][host] evt=sum` |

**Quy tắc chấm chung, áp cho mọi bake-off:** IDR phải tách theo `KeyframeReason` trước khi
đếm — `q_overflow`, `dec_fail`, `display_congested` đến từ pipeline client chứ không phải từ
mạng, và trên phần cứng thật chúng chiếm phần đáng kể. Xem dòng `evt=kf_sum`.

#### Đã đo: Tailscale qua WAN (03/09/2026) — và link này **bursty**, trái hẳn WiFi nhà

Viewer là `deskhub-cli` release trên Ubuntu (`manh-pham-ubuntu`, VA-API), host là MacBook Pro qua
Tailscale. Đường đi **direct qua IP công cộng**, không qua DERP — `tailscale status` báo
`direct 1.54.20.207:42986`, nên đây là WAN thật chứ không phải Tailscale đi tắt qua LAN.
5 phút, 309 cửa sổ, H264 1920x1246 thương lượng ở 60 fps / 20 Mbps.

| | WiFi nhà (01/09) | **Tailscale WAN (03/09)** |
| --- | --- | --- |
| Cửa sổ | 300 | 309 |
| min RTT | — | 5,9 ms (p50 6,2 · max 8,1) |
| RTT trong phiên | p50 15,9 · max 91,7 ms | p50 8,9 · p90 12,3 · max 44,4 ms |
| Bitrate | p50 8483 kbps | p50 11 860 · max 15 818 kbps |
| fps | p50 38 | p50 29 · min 8 · max 37 |
| e2e | p50 20,1 · p90 29,8 · max 47,6 ms | p50 34,9 · p90 53,1 · **max 285,5 ms** |
| Cửa sổ có mất gói | 5/196, đều 0,1% | 12/309 |
| **Độ dài run mất gói** | **tất cả bằng 1**, longest 1 | **TB ≥ 4,9 gói, longest 41** |
| FEC vá được | 0 | 24 gói, đổi bằng 7350 gói parity |

**Phân bố độ dài burst (129 burst):**

| độ dài | 1 | 2 | 3 | 4–7 | 8–15 | 16–31 | 32+ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| số burst | 33 | 25 | 11 | 29 | 18 | 10 | 3 |

47% số burst dài từ 4 gói trở lên. Đây là chế độ loss **hoàn toàn khác** WiFi nhà, nơi mọi burst
đều đúng 1 gói.

##### Ba kết luận rút ra được ngay

- **Không có một bộ tham số Gilbert-Elliott nào phục vụ được cả hai link.** WiFi nhà: 0,0015%,
  burst 1,0, không bursty chút nào. Tailscale WAN: burst TB ≥4,9, dài nhất 41. Giả định
  `5% / burst 4` mà sim Phase 2 đang chạy **lệch ba bậc so với WiFi nhà nhưng gần đúng về hình
  dạng với WAN**. Nghĩa là bảng xếp hạng FEC phải có **hai điểm vận hành**, không phải một.
- **XOR đơn parity không có cửa trên link này, và giờ có số để nói.** Group size 8, một gói parity
  mỗi group, cứu tối đa 1 gói mất mỗi group. Với burst 41 gói thì không sơ đồ XOR nào chạm tới.
  Con số 7350 parity đổi lấy 24 gói vá là bằng chứng thực nghiệm đầu tiên ủng hộ Reed-Solomon hoặc
  NACK trên WAN — trước đó A1 chỉ có lập luận, chưa có số từ link thật.
- **A1 trượt ngưỡng của chính nó trên link này.** 13 IDR do `KeyframeReason::Loss` trong 5,15 phút
  = **2,5 IDR/phút**, trong khi ngưỡng đạt đặt ở ≤ 2/phút. Tổng 16 IDR (13 loss · 2 dec_fail ·
  1 wait_idr), trung bình 130 KB mỗi cái, tổng 2080 KB — chỉ để vá cho lượng dữ liệu mất nhỏ hơn
  hai bậc.

Frame bị bỏ, theo lý do: `timeout` 16 frame (264 gói vắng), `overtaken` 13 frame (216), `evicted`
6 frame (300).

##### Bốn cảnh báo phải nói kèm mỗi lần trích bảng này

- **Nội dung màn hình không kiểm soát được** — desktop Mac bất kỳ, không phải clip cố định. Bitrate
  và fps vì thế chỉ đọc được như bậc độ lớn.
- **fps p50 29 trên một phiên thương lượng 60 fps**: encoder phía Mac là trần, đúng như nhận xét đã
  ghi cho máy Windows. Phép đo này chạm trần encoder trước khi chạm trần transport.
- **`dgram_refused` không có**: Mac chạy build cũ (nhận ra vì `evt=sum` không có `e2e_abs_ms`). Nên
  mục "bao nhiêu phần loss là tự mình bỏ ở hàng đợi" **vẫn chưa trả lời được cho link WAN**. Kết
  luận P1 ngày 01/09 chỉ đúng trong biên LAN + WiFi nhà, và biên đó vẫn chưa được nới.
- **Phân bố burst ở trên là chặn dưới, không phải loss trên dây.** Xem mục kế tiếp.

##### Phiên thứ hai, host đã instrument (03/09/2026): P1 có câu trả lời cho WAN

Phiên đầu chạm cổng 47777 của bản build cũ nên không có bộ đếm phía host. Dựng thêm một host
`deskhub-cli share --port 47778 --bind <tailscale-ip> --encoder videotoolbox --no-input` từ chính
nhánh này trên Mac, giữ nguyên host cũ ở 47777. Kiểm chứng host đúng là bản mới **từ xa**, không
cần nhìn log bên kia: `e2e_abs_ms` xuất hiện trên dòng `evt=sum` của viewer, mà con số đó chỉ tính
được khi host gửi kèm `hostTimeUs` trong `PingPong` — code chỉ có trên nhánh này.

308 cửa sổ, tách theo bitrate (không theo thời gian, nên vài cửa sổ IDR đầu phiên rơi vào nhóm có
tải):

| | màn hình tĩnh (151) | có tải, video toàn màn hình (157) |
| --- | --- | --- |
| bitrate | p50 123 kbps | p50 6470 · p90 8896 · **max 12 489 kbps** |
| fps | p50 49 | p50 49 · min 35 |
| RTT | p50 6,6 · max 29,9 ms | p50 7,0 · p90 9,4 · max 53,0 ms |
| e2e | p50 11,2 · max 41,9 ms | p50 12,7 · p90 28,5 · max 47,4 ms |
| cửa sổ có loss | 0/151 | 5/157, cao nhất 1,5% |
| frame bị bỏ | 0 | 10 |

**Phía host, 521 cửa sổ: `dgram_refused = 0` ở tất cả**, `dgram_tx` đỉnh 1328 gói/giây. Nhân với
MTU thì 1328 × ~1200 byte ≈ 12,7 Mbps, khớp với `max 12 489 kbps` mà viewer báo — nên bộ đếm này
đúng là đang đo đường video, không phải đo một kênh phụ.

**Kết luận cho P1, và biên của nó.** Trên WAN thật, một viewer, tới 12,7 Mbps, CC của quiche không
từ chối một datagram nào. Cộng với phép đo LAN + WiFi nhà ngày 01/09, ràng buộc *"phải làm P1
trước A1 và A2"* được gỡ cho cả ba link đã đo. Nhưng **chưa link nào bị đẩy tới bão hoà**: trần
thương lượng là 20 Mbps còn thực tế chỉ chạm 12,7, và đường cáp này rõ ràng còn thừa băng thông ở
mức đó. Câu hỏi "hàng đợi datagram có tự bỏ gói khi link thật sự chật không" vẫn chưa được hỏi —
nó cần một link bị bó (WiFi quán), không cần thêm bộ đếm nào.

##### Phát hiện về chính phương pháp: một mẫu 5 phút không đặc trưng hoá được link

Hai phiên trên **cùng một đường Tailscale, cách nhau hai tiếng**, cho hai bức tranh trái ngược:

| | phiên 1 (47777) | phiên 2 (47778) |
| --- | --- | --- |
| tổng số burst | 129 | 15 |
| burst dài nhất | **41 gói** | **4 gói** |
| burst ≥ 4 gói | 47% | 7% |
| IDR do loss | **2,5/phút** — trượt ngưỡng A1 | **1,9/phút** — đạt ngưỡng |
| e2e max | 285,5 ms | 47,4 ms |

Nếu chỉ có phiên 2 thì kết luận "link WAN không bursty, A1 đạt ngưỡng" — ngược hẳn phiên 1. Không
phiên nào sai; **mẫu mới là thứ thiếu**. Mọi tham số Gilbert-Elliott rút từ một phiên đơn lẻ đều là
tham số của giờ đó, không phải của đường đó. Trước khi chốt bất kỳ con số nào cho sim, phải đo
nhiều phiên rải theo giờ và báo cáo cả phương sai, không chỉ trung vị.

Một chi tiết nhỏ nhưng đáng nhớ khi đọc số: host báo 6 IDR do loss, viewer báo 5. Không mâu thuẫn —
log host trải 521 cửa sổ, dài hơn phiên 308 cửa sổ của viewer, nên nó bao cả những lần kết nối
khác. Đối chiếu hai phía phải cắt theo cùng cửa sổ thời gian.

Và lần nữa, tỉ lệ FEC vẫn tệ: **3479 gói parity đổi lấy 2 gói vá.**

##### Bộ đếm gói từng vắng mặt — đã làm (03/09/2026)

`stats_.packetsLost` chỉ cộng bên trong `Drop()` (`core/src/transport/Reassembler.cpp`), và
`lossRuns[]` cũng vậy. Nghĩa là cả hai chỉ đếm gói mất trên những frame **đã bị bỏ**. Gói mất mà
FEC vá, hay NACK vá **kịp lúc** cho frame hoàn thành, thì không xuất hiện ở đâu.

> **Sửa một lập luận sai của chính tài liệu này.** Bản nháp trước trích "810 gói late" của phiên WAN
> đầu tiên làm bằng chứng cho vùng mù. Sai: `NoteLatePacket` chỉ đếm gói tới **sau khi frame đã bị
> bỏ**, mà những gói đó đã được `packetsLost` tính lúc drop. Chúng không hề vô hình — chúng chỉ cho
> thấy bản vá tới quá muộn. Bằng chứng đúng nằm ở phép đo bên dưới, và nó nhỏ hơn nhiều.

**Cách đo mới.** Một chỗ trống chỉ lộ ra ở đúng ba thời điểm, và cả ba đều được đánh dấu:

| Lộ ra khi | Chỗ đánh dấu |
| --- | --- |
| Một gói lấp vào chỉ số **thấp hơn** chỉ số cao nhất đã thấy | `Push` — nghĩa là ta đã đi qua chỗ đó lúc nó còn trống |
| FEC dựng lại một mảnh | `TryRecover` — FEC chỉ lấp vào chỗ đang trống |
| Frame rời hàng đợi mà mảnh vẫn trống | `NoteWireLoss`, gọi ở **cả hai** lối ra: hoàn thành và bị bỏ |

Nhờ vậy không cần quét tìm gap: mỗi chỗ trống hoặc được lấp (và lối lấp tự khai báo), hoặc còn
trống lúc frame rời đi. Đuôi frame bị mất — thứ mà phép quét theo chỉ số tăng dần không bao giờ
thấy — rơi vào vế thứ ba.

**Bốn thùng, cộng lại đúng bằng tổng.** Mỗi gói từng vắng mặt rơi vào đúng một thùng:

| Thùng | Nghĩa | Có phải loss trên dây? |
| --- | --- | --- |
| `packetsNeverArrived` | không bao giờ tới | có |
| `packetsRepairedByFec` | parity dựng lại | có |
| `packetsRepairedAfterNack` | tới sau khi ta đi hỏi đích danh | có |
| `packetsReordered` | tới muộn mà **không** ai hỏi | **không** — chỉ là đảo thứ tự |

Tách thùng thứ tư ra là phần bắt buộc, không phải trang trí: đảo thứ tự không phải mất gói, và
gộp nó vào sẽ thổi phồng tham số Gilbert-Elliott. Vì thế
`wire_loss% = (everAbsent − reordered) / (received + neverArrived)`.

Ranh giới phải nói kèm: `nacked[i]` được đặt lúc `PlanNack` chọn chỉ số đó, nên một gói vừa bị hỏi
vừa chỉ đơn thuần tới muộn sẽ được tính là "vá bằng NACK". Đó là thiên lệch **về phía coi là loss**,
tức thận trọng đúng hướng cho A1, nhưng phải nhớ khi trích.

**Ra tới đâu.** Dòng `evt=sum` phía client nay chở `wire_loss=…% absent=… gone=… nack_fix=…
reorder=…`, và có thêm dòng `[Client] wire runs: 1x… 2x… | longest ever …` đứng cạnh dòng
`loss runs` cũ. Hai dòng cố ý để cạnh nhau: dòng cũ là **loss không cứu được** (thứ người dùng
thấy), dòng mới là **loss trên dây** (thứ sim cần). Không dòng nào thay thế dòng nào.

**Đối chứng với đáp án biết trước.** Trước khi tin bộ đếm ngoài đời, cho nó chạy lại một quá trình
đã biết: mô hình Gilbert-Elliott có seed (5% loss, burst trung bình 4 gói) bơm qua 400 frame × 20
gói, FEC và NACK đều tắt. Kết quả: `packetsEverAbsent` bằng **đúng** số gói harness đã nuốt, và
`absentRuns[]` khớp **đúng từng thùng** với histogram burst mà harness đã dựng độc lập. Cùng lúc,
`packetsLost` cũ cũng bằng đúng con số đó — đó là **nhóm chứng**: khi không có gì vá thì hai bộ đếm
phải trùng nhau, và chúng chỉ tách ra khi FEC hoặc NACK đặt được một gói trở lại. Mô hình
Gilbert-Elliott vì thế được nâng từ `LossGoodputTests` lên `core/tests/support/` để sim và test
dụng cụ dùng chung một bản, không phải hai bản có thể lệch nhau.

Năm test trong `core/tests/transport/ReassemblerTests.cpp` khoá năm hành vi: chỗ trống được một gói
tới sau lấp vẫn bị đếm dù `packetsLost == 0`; chỗ trống FEC vá được xếp vào thùng fec; gói bị hỏi
đích danh vào thùng nack chứ không vào thùng đảo thứ tự; đuôi frame mất được đếm đúng một lần và
gộp thành một run; và bộ đếm dựng lại đúng một quá trình Gilbert-Elliott đã biết.

**Đã xác nhận trên link thật (03/09).** Phiên 5 phút qua Tailscale, 308 cửa sổ, 293 cửa sổ có tải,
bitrate trung vị 5744 kbps, đỉnh 12 294:

| | |
| --- | ---: |
| `packetsEverAbsent` | **48** |
| ├ `neverArrived` | 41 |
| ├ `repairedAfterNack` | **7** |
| └ `reordered` | 0 |
| `fec_rx` / `fec_fix` | 3056 / **0** |

Bảy gói ở dòng `repairedAfterNack` chính là phần bộ đếm cũ không thấy: chúng vắng mặt trên dây,
NACK lấy về kịp, frame hoàn thành, nên `Drop()` không bao giờ chạy. Histogram cũng lệch đúng một
chỗ — một cửa sổ có `wire runs … 2x1 … 8-15x1` trong khi `loss runs` cùng cửa sổ chỉ có `8-15x1`:
một run dài 2 gói đã bị mất khỏi phân bố.

Chênh lệch là **17%** (48 so với 41), không phải nhiều lần. Đó là con số thật của vùng mù trên link
này, và nó nhỏ vì `fec_fix = 0` — FEC phát 3056 gói parity mà không vá được gói nào, nên toàn bộ
phần vá đến từ NACK. Trên một link mà FEC thật sự làm việc thì khoảng cách sẽ rộng hơn.

#### Bốn dụng cụ đo phải thêm mới đo được, ngoài kế hoạch ban đầu

Kế hoạch không nêu ba cái này, nhưng thiếu chúng thì không mục nào ở trên chấm điểm được:

| Dụng cụ | Ở đâu | Không có nó thì mù chỗ nào |
| --- | --- | --- |
| `dgram_tx` · `dgram_refused` | `[DIAG][host] evt=sum` | Không tách được loss của mạng khỏi gói mình tự bỏ ở `kDatagramQueue` |
| `fec_rx` · `fec_fix` | `[DIAG] evt=sum` phía client | Không phân biệt được "FEC không cứu nổi" với "FEC **không hề bật**" — đây chính là chỗ tôi suy luận sai một lần trước khi có số |
| `evt=kf_sum` theo `KeyframeReason` | dòng riêng phía client | Hàm mục tiêu của A1 bị `q_overflow` làm nhiễu; `RequestKeyframe` nay nhận enum có kiểu thay vì `const char*` nên thêm nguyên nhân mới là bắt buộc thêm bộ đếm |
| `enc_us_p50` · `enc_us_p99` | `[DIAG][<source>] evt=sum` | Hàm mục tiêu của C1 là **đuôi** encode latency, mà `enc_ms_avg`/`enc_ms_max` chỉ cho trung bình và một điểm ngoại lai duy nhất — không phân biệt được một backend chậm đều với một backend thỉnh thoảng khựng |

Cả ba đều cùng một hình dạng: bộ đếm **vốn đã tồn tại** trong `Reassembler::Stats`,
`QuicSendStats` hoặc dòng log, chỉ là chưa bao giờ được phơi ra.

#### Bốn lỗi mà chỉ việc vẽ đường cong mới bắt được

Ghi lại vì chúng là lý lẽ mạnh nhất cho việc quét-và-vẽ, chứ không chỉ viết test điểm:

| Lỗi | Ở đâu | Vì sao test điểm không bắt |
| --- | --- | --- |
| **Bộ lọc jitter tràn số** — `jitterUs_ += (spread - jitterUs_) >> shift` với cả hai là `uint64_t`; mẫu nào yên hơn trung bình làm phép trừ quấn về gần 2⁶⁴ nên ước lượng **phồng** thay vì suy giảm | `AudioJitterBuffer` **và** `VideoPacer` — cùng một dòng, hai chỗ | Test điểm chỉ hỏi "jitter cao có lớn hơn jitter thấp không", và điều đó vẫn đúng khi cả hai đều sai |
| Sim FEC thiếu **chính sách bật FEC** | `LossGoodputTests` | Sim đo một hệ thống không tồn tại: FEC luôn bật, còn thật thì 30% thời gian |
| Sim FEC thiếu **đường NACK** | `LossGoodputTests` | Ghi công cho FEC những lần vá mà retransmit vốn làm được |
| Harness audio hút cạn buffer mỗi lần push, rồi sau khi sửa lại đẩy-một-lấy-một | `AudioJitterBufferTests::FeedLink` | Lần một cho gap ∝ frames/target ở **mọi** mức jitter; lần hai cho **0** gap ở mọi mức. Cả hai đều "pass" nếu chỉ nhìn một điểm |

Điểm chung: mỗi lỗi làm một **hình dạng** sai, không làm một giá trị sai. Một điểm đơn lẻ luôn
có thể giải thích được; cả đường cong thì không.

#### Đã đo: LAN có dây và WiFi nhà (01/09/2026)

Viewer là `deskhub-cli` release trên Windows 192.168.1.3 (Ethernet). Hai host: máy Mac
192.168.1.8 và iPhone 192.168.1.5, cả hai chạy build đã cài sẵn.

**Dụng cụ phải thêm trước khi đo được:** `Reassembler::Stats::fecReceived` vốn đã đếm gói
parity nhận được nhưng chưa bao giờ phơi ra — đúng hình dạng của bộ đếm P1. Nay dòng
`evt=sum` phía client mang `fec_rx` (parity tới) và `fec_fix` (parity vá được). Không có hai
số này thì không phân biệt được "FEC không cứu nổi" với "FEC không hề bật".

| | Mac, có chuyển động | iPhone qua WiFi |
| --- | --- | --- |
| Cửa sổ | 93 | 300 |
| probe RTT | 4 ms | **67 ms** |
| Bitrate | p50 11790 · max 15585 kbps | p50 8483 · max 16406 kbps |
| fps | p50 37 · avg 38 | p50 38 · avg 39 |
| RTT trong phiên | 14,8–15,1 ms (phẳng) | p50 15,9 · p90 31,2 · **max 91,7 ms** |
| e2e | p50 23,6 · p90 32,4 · max 46,9 ms | p50 20,1 · p90 29,8 · max 47,6 ms |
| Cửa sổ có mất gói | 0/93 | 5/196, đều 0,1% |
| Độ dài run mất gói | — | **tất cả bằng 1**, `longest ever 1 pkts` |
| FEC vá được | 0 | **0** |

**Tham số Gilbert-Elliott đo được (WiFi nhà, sóng tốt):** ~330.832 gói video, 5 gói mất
không cứu được ⇒ tỉ lệ **0,0015%**, độ dài burst trung bình **1,0**. Sim ở Phase 2 đang chạy
giả định 5% / burst 4 — **lệch ba bậc**, và lệch cả về hình dạng: link này không hề bursty.
Mọi thứ hạng scheme rút ra từ giả định đó chưa nói gì về link thật này.

##### Phát hiện chính: chính sách bật FEC luôn đi sau, và mù với loss nhỏ

Đo trên Mac bắt được trọn vẹn một chu kỳ bật/tắt:

| Cửa sổ | loss | `fec_rx` |
| --- | --- | --- |
| 1–10 | 0,0% | 0 |
| 11 | **0,5%** | 0 |
| 12–21 | 0,0% | 88–174 gói/giây |
| 22 | 0,0% | 4 |
| 23+ | 0,0% | 0 |

Khớp chính xác với code: `fb.lossPct = uint8_t(std::lround(w.lossPct))`
(`LinkStats.cpp:45`) biến 0,5% thành 1, `BitrateController.cpp:10` thấy `>= 1` nên bật FEC —
**sau** khi mất gói đã xảy ra — rồi `kCleanSecondsBeforeDroppingFec = 10` tắt lại sau đúng 10
cửa sổ sạch. Tổng cộng 1224 gói parity phát ra, vá được **1** gói.

Trên iPhone thì tệ hơn: loss thật là 0,1% mỗi lần, mà `lround(0,1) = 0`, nên host thấy y hệt
một link hoàn hảo. `fec_rx = 0` suốt cả 118 cửa sổ đo lại — **FEC không bật một lần nào**.
Hậu quả đo được: mỗi gói mất (≤1174 byte) tốn một frame bị bỏ và một IDR nguyên khung
(144591 và 155642 byte) — khoảng **128 lần** lượng dữ liệu đã mất.

Nói cách khác: **mọi mất gói dưới 0,5% đều vô hình với chính sách bật FEC**, và đó đúng là
chế độ mất gói mà WiFi nhà thực sự có. Chuyện này nằm ở A1/A2 chứ không phải ở việc chọn sơ
đồ FEC — không sơ đồ nào cứu được khi parity không được phát.

##### Hai cảnh báo về chính phép đo

- `stats_.packetsLost` chỉ cộng bên trong `Drop()` (`Reassembler.cpp:206`), nên `lost%` chỉ
  đếm gói mất trên những frame **đã bị bỏ**. Mất gói mà NACK vá kịp không xuất hiện ở đâu cả.
  Con số 0,0015% vì vậy là "loss không cứu được", không phải loss trên dây. Muốn có loss thật
  cần thêm một bộ đếm "gói từng vắng mặt" — hiện chưa có.
- `q_overflow` xuất hiện 20 lần trên Mac và 6 lần trên iPhone. Đó là hàng đợi decode phía
  client, không phải transport — nhưng mỗi lần đều xin một IDR, nên nó cũng đang bơm bitrate
  y như loss thật. Đáng tách ra khỏi số liệu của A1.

**P1 vẫn chưa trả lời được:** `dgram_refused` do host in ra, mà cả hai host đều chạy build
không có bộ đếm. Muốn đo phải đảo vai — chạy host trên máy đã instrument.

### Phase 1 — Làm rõ quan hệ hai tầng điều khiển tắc nghẽn

*P1. Phải xong trước A1 và A2, nếu không cả hai đều tối ưu cho mô hình sai.*

- ⏳ [#56](https://github.com/manhpham90vn/Deskhub/issues/56) — Chạy A/B `RawUdp` so với `QuicDatagram` trên cùng một link — không phải viết code, và nó đo thẳng phần đóng góp của CC quiche
  — **núm vặn đã kiểm chứng xong (03/09), phép đo thì chưa.** Xem "Kiểm núm vặn trước khi đi đo"
  và "Vì sao A/B phải chạy trên link chật" bên dưới
- ⏳ [#57](https://github.com/manhpham90vn/Deskhub/issues/57) — Quyết kiến trúc: quiche làm chủ, AIMD làm chủ, hay chia vai rõ ràng
  — chặn bởi mục trên; không có số thì mọi lựa chọn đều là suy đoán
- [x] Đặt cấu hình tường minh trong `MakeConfig` để lựa chọn đó nằm trong code chứ không nằm trong mặc định CUBIC của thư viện
  — `QuicSettings::congestionControl` mặc định `Cubic`, gọi `quiche_config_set_cc_algorithm`
  tường minh. **Không đổi hành vi** (đã xác nhận mặc định của quiche là CUBIC tại
  `third_party/quiche/src/quiche/src/lib.rs:654`); giá trị của nó là ghim baseline lại để một
  lần nâng quiche không âm thầm đổi phép đo. Reno và BBR2 cũng chọn được, cho phép quét
- ⏳ [#58](https://github.com/manhpham90vn/Deskhub/issues/58) — Ghi quyết định vào `docs/ARCHITECTURE.md` §Decisions worth remembering kèm ba bản dịch

> **"Không phải viết code" là sai.** `SetVideoPath` chỉ có caller trong
> `platform/tests/net/SessionTransportTests.cpp` — từ một binary đã build không có cách nào
> đưa video sang `RawUdp`. Đã thêm `deskhub-cli share|connect --video-path raw-udp|quic-datagram`,
> nối qua `HostEngine` và `HostLink` tới `SetVideoPath`. Cả hai đầu phải đặt giống nhau vì
> `SessionTransport::Deliver` chỉ nhận video raw khi chính nó đang ở chế độ `RawUdp`.

#### Kiểm núm vặn trước khi đi đo (03/09/2026)

Áp lại bài kiểm tra đã dùng cho A1 và C1 — *"một núm vặn không có caller ngoài test của chính nó
thì không phải núm vặn"* — lên `--video-path`, trước khi tiêu một buổi đo vào nó.

| Kiểm | Kết quả |
| --- | --- |
| Có caller thật hai đầu? | **Có.** Host: `ShareCommand` → `ShareOptions::videoPath` → `HostEngine.cpp:241` `sock_.SetVideoPath(...)`. Viewer: `ConnectCommand` → `ScreenViewerConfig` → `HostLink.cpp:94` |
| Tên gõ sai có bị bắt? | **Có**, `Command.cpp:877` qua `media::IsVideoPathName`, và `CommandTests` đã khoá cả ca tên sai lẫn hai tên đúng |
| Đường raw có chở được video thật? | **Có**, `SessionTransportTests` đẩy một gói video qua cả hai chế độ và khoá luôn ca lệch vế: viewer còn ở `QuicDatagram` thì **từ chối** video raw |

**Nhưng có một lỗ, và nó đủ để làm hỏng cả buổi đo:** host **không bao giờ in ra nó đang chạy leg
nào.** Dòng `[Host] measurement:` chở `cc/fec/parity/depth/arm` mà không có video path, và tệ hơn,
nó **chỉ được in khi có cờ fec hoặc cc**. Chạy A/B chỉ với `--video-path raw-udp` thì host im lặng
hoàn toàn — không có gì trong log chứng minh leg nào đã chạy, và hai chân A/B sau này không phân
biệt được bằng bằng chứng.

Đã sửa: dòng đó nay chở thêm `video=…`, đọc **ngược từ `sock_.videoPath()`** chứ không phải từ chuỗi
tuỳ chọn — nên nếu tên rơi về mặc định thì log nói đúng cái mặc định đó. Điều kiện in cũng mở rộng
để `--video-path` một mình cũng đủ kích hoạt. Thêm `VideoPathName()` làm ánh xạ ngược, có test khoá
cặp `VideoPathName`/`VideoPathFromName` khứ hồi.

#### Vì sao A/B phải chạy trên link chật, không phải trên link đã có

Dự đoán, viết ra **trước** khi đo để sau này không tự lừa: trên Tailscale WAN, A/B sẽ ra **kết quả
rỗng**. Lý do không phải phỏng đoán mà là số đã đo: `dgram_refused = 0` trên 521 cửa sổ, tới
12,7 Mbps. CC của quiche **chưa bao giờ can thiệp** trên link này. Gỡ một cơ chế chưa từng kích hoạt
thì không thể đổi được gì, nên A/B ở đây chỉ đo được chênh lệch của **pacing** — thứ raw UDP bỏ qua
— chứ không đo được cái mà mục này đặt ra.

Nên A/B phải đi cùng chuyến đo WiFi công ty của Phase 0, và bố trí là chung: **iPhone host, Ubuntu
viewer**. Mỗi khung giờ chạy hai chân liên tiếp, cùng nội dung màn hình, cùng 5 phút:

| Chân | Host | Viewer |
| --- | --- | --- |
| A | `share --video-path quic-datagram` | `connect … --video-path quic-datagram` |
| B | `share --video-path raw-udp` | `connect … --video-path raw-udp` |

Hai đầu **bắt buộc** đặt giống nhau: `SessionTransport::Deliver` chỉ nhận video raw khi chính nó
đang ở chế độ `RawUdp`, lệch vế là viewer im lặng không thấy hình. Chân A cũng chính là mẫu Phase 0
cho khung giờ đó — một lần chạy phục vụ hai mục.

Cái cần đọc ra khi so hai chân: `wire_loss` và phân bố `wire runs` (raw không có CC thì loss có tăng
không), bitrate đạt được, e2e p50/p90/max, và `dgram_refused` phía host ở chân A. Nếu chân B đạt
bitrate cao hơn với loss tương đương thì CC của quiche đang kìm; nếu chân B loss vọt lên thì nó đang
bảo vệ.

#### Đã đo: đảo vai, host là máy đã instrument (01/09/2026)

Host là `deskhub-cli share --bind 192.168.1.3` trên Windows (NVENC, RTX 5070 Ti, nguồn
3440x1440 hạ xuống 1920x802). Viewer: MacBook Pro qua LAN có dây, rồi thêm iPhone qua WiFi.

| | 1 viewer (Mac, LAN) | 2 viewer (+ iPhone, WiFi) |
| --- | --- | --- |
| Cửa sổ | 47 | 148 |
| Datagram gửi | 34.981 | 111.685 |
| **`dgram_refused`** | **0** | **0** |
| `send_fail` · `q_drop` | 0 · 0 | 0 · 0 |
| Viewer báo loss | 0% | 0% × 148 |
| Bitrate gửi | max 12.475 kbps | p50 9104 · max 12.884 kbps |

**Kết luận cho P1, trong biên đã đo:** CC của quiche **chưa từng từ chối một datagram nào**
qua 146.666 gói. Nỗi lo "một phần đáng kể loss thực ra là mình tự bỏ ở `kDatagramQueue`"
không xảy ra ở lớp link này, kể cả khi có một viewer WiFi và hai connection song song. Nghĩa
là A1 và A2 **không** đang tối ưu cho một mô hình sai vì lý do này — ràng buộc "phải làm P1
trước" được gỡ cho lớp link này.

Biên của kết luận, phải nói kèm mỗi lần trích: LAN gigabit cộng WiFi nhà sóng tốt, ≤13 Mbps,
≤2 viewer, RTT ≤40 ms. Trên link bị bó thật (Tailscale qua WAN, WiFi quán) kết quả có thể
khác hẳn, và bộ đếm giờ đã sẵn sàng để đo.

**Đã nới biên (03/09):** Tailscale qua WAN cũng cho `dgram_refused = 0` trên 521 cửa sổ, tới
12,7 Mbps — xem "Phiên thứ hai, host đã instrument". Còn WiFi quán, và chưa link nào bị đẩy tới
bão hoà.

##### Một confounder cho hàm mục tiêu của A1

Trong 148 cửa sổ hai viewer: **10 IDR (~72 KB, 62 gói mỗi cái) trong khi cả hai viewer báo
0% loss và không có reconfig nào.** Chúng đến từ `q_overflow` phía viewer — hàng đợi decode
tràn rồi xin keyframe — chứ không phải từ mạng.

A1 lấy "số lần xin IDR trên mỗi phút" làm hàm mục tiêu. Nếu đếm gộp thì một phần đáng kể số
đó không phải do mất gói mà do pipeline decode của chính client. Lý do đã được log sẵn phía
client (`kf_req reason=loss|q_overflow|wait_idr|pre_idr`) nhưng **chưa được đếm ở đâu cả**.
Phải tách theo reason trước khi dùng con số này để chấm điểm bất kỳ sơ đồ FEC nào.

**Đã xong (02/09/2026).** Phía viewer thực ra đã có `KeyframeRequestLog` đếm theo reason và in
`evt=kf_sum`; chỗ thiếu là **phía host** — bên thực sự tiêu cái IDR, và cũng là bên A1 chấm
điểm. `RequestKeyframe` trước đây là datagram rỗng nên mọi yêu cầu giống hệt nhau khi tới host.
Nay nó chở reason bằng **một byte payload**: `deskhub::KeyframeReason` chuyển từ `diag` sang
`Wire.h` (nơi cả hai bên nhìn thấy), `ParseRequestKeyframe` đọc lại, `ScreenHostSession` đưa nó
vào `onKeyframeRequest`, và `SourceDiag::FormatKeyframeRequests` in `evt=kf_req_sum total=N`
kèm số đếm từng reason mỗi cửa sổ.

Hai chi tiết phải nhớ khi đọc số:

- **Peer cũ không gửi byte nào** → `ParseRequestKeyframe` trả `Unknown`, không phải `Loss` (ô 0
  của enum). Golden vector `023300001122334402` đã cập nhật, và `WireVectorTests` giữ thêm một
  ca dựng tay 8 byte đúng kiểu peer cũ để khoá tương thích ngược lại
- **Đường host tự re-join giữa chừng** (viewer thứ hai gửi `Start` khi đã streaming) được gán
  `ViewerJoin`, không mượn reason của viewer nào

Nghĩa là từ giờ `idr_per_min` trong bảng bake-off **tách được** theo nguyên nhân. Chưa làm:
nối `evt=kf_req_sum` vào chính CSV của sim để cột `idr_per_min` tự chia — sim hiện chỉ sinh
IDR do loss nên con số của nó vốn đã sạch; việc tách chỉ cần trên máy thật.

##### Nút cổ chai ở đây là encoder, không phải link

`enc_ms` trung bình 19,2 ms, đỉnh 104 ms, trong khi một frame ở 60 fps chỉ có 16,7 ms. Đó là
lý do fps thực tế 23–44 chứ không phải 60, và nó không liên quan gì tới mạng. Mọi phép đo
video trên máy này đều bị trần bởi encoder trước khi chạm trần transport — cần nhớ khi đọc
lại các con số ở trên.


### Phase 2 — Dựng harness trên một thành phần làm mẫu

*A1 · FEC.*

- [x] Tách `IFecScheme` ra khỏi `Packetizer`, giữ XOR hiện tại làm impl đầu tiên — không đổi hành vi
  — đặt tên là **`FecScheme`** (không tiền tố `I`, theo lối đặt tên trong repo), ở
  `core/include/deskhub/transport/FecScheme.h`. `Encode`/`Recover` làm việc ở mức một group
  và trả span vào scratch của chính scheme, nên không cấp phát sau lần đầu
- [x] Tách độ sâu interleave khỏi `numGroups` để nó thành tham số quét được, mặc định vẫn là `ceil(count/8)`
- [x] Chuyển `core/tests/transport/FecTests.cpp` thành tham số hoá, chạy qua mọi impl đăng ký
- [x] Đổi mô hình loss của `LossGoodputTests.cpp` từ "bỏ 6 gói đuôi mỗi frame thứ 50" sang Gilbert-Elliott có seed, xuất CSV
- [x] Thêm Workload đo CPU từng scheme vào `core/perf/VideoPerf.cpp`
- [x] Nối `deskhub-cli --fec=…` vào factory — điều kiện cần để giữ được các bản thua

### Phase 3 — Chạy đua FEC

*Quét tham số trước, viết impl mới sau.*

> **Chặn: chính sách bật FEC phải xong trước cả phép quét.** `LossGoodputTests` nay chạy
> `BitrateController` thật qua `MakeFeedback` thật. Tại điểm vận hành đo được trên WiFi nhà
> (0,1% loss, burst 1):
>
> | | FEC luôn bật | Chính sách đang ship |
> | --- | --- | --- |
> | Frame hỏng cứu được | 16/16 | 6/16 |
> | Thời gian FEC bật | 100% | 30,0% |
> | **IDR/phút** | **0** | **20,0** |
>
> Hàm mục tiêu của A1 là IDR/phút. Chính sách bật FEC một mình gây ra 20 IDR/phút, còn sơ đồ
> FEC thì 70% thời gian không chạy. Cho Reed-Solomon đua với XOR ở trạng thái này là đo một
> cấu hình không tồn tại trên máy thật — **quét chính sách bật trước, quét sơ đồ sau**.
>
> Không gian tham số của chính sách: ngưỡng arm (hiện `lossPct >= 1` sau khi `lround` đã bóp
> mọi giá trị dưới 0,5% thành 0) · tín hiệu arm (phần trăm hay số gói mất tuyệt đối) ·
> `kCleanSecondsBeforeDroppingFec` · có nên tắt hẳn không. Cả bốn đều quét được bằng harness
> hiện có, không cần viết impl mới nào.

- [x] **Quét trước khi viết:** depth × group size × tỉ lệ parity trên chính bản XOR đang có. Reed-Solomon chỉ đáng viết nếu lần quét này chạm trần dưới ngưỡng đạt

#### Kết quả quét (180 điểm, FEC luôn bật để cô lập sơ đồ khỏi chính sách bật)

Ở 5% loss, burst 4, RTT 40 ms — trích các điểm đáng chú ý:

| scheme | parity | depth | **overhead** | cứu được | IDR/phút |
| --- | --- | --- | --- | --- | --- |
| xor | 1 | dẫn xuất *(đang chạy)* | **15%** | 33,9% | 171 |
| **rs** | **1** | **dẫn xuất** | **15%** | **33,9%** | **171** |
| xor | 1 | 8 | **100%** | 78,7% | 81 |
| **rs** | **1** | **8** | **100%** | **78,7%** | **81** |
| rs | 2 | 8 | 200% | 86,7% | 45 |
| rs | 3 | 4 | **150%** | 90,6% | **36** |
| rs | 3 | 8 | **300%** | 89,3% | 54 |

**Ba kết luận rút thẳng từ bảng:**

1. **`rs` ở parity = 1 cho số liệu giống hệt `xor`, tới từng chữ số, ở cả ba mức depth.** Đúng
   như lý thuyết — RS một hàng parity *là* XOR — nhưng nó tốn 6,3× CPU encode (143 µs so với
   22,7 µs mỗi frame). **RS chỉ đáng dùng từ parity ≥ 2 trở lên.**
2. **Depth không hề miễn phí, và bản viết đầu tiên của chính mục này đã gọi nó là "đòn bẩy rẻ
   nhất trong lưới" — sai.** Nó không thêm CPU, đúng; nhưng nó đưa overhead từ 15% lên **100%**.
   Ở một group mỗi gói thì mỗi gói tự mang parity của chính mình — đó là nhân đôi, không phải
   mã hoá. Trên ngân sách 20 Mbps, đó là tiêu nửa bức ảnh để giảm nửa số lần xin keyframe.
3. **Phép quét không sinh ra bản thắng nào để đưa lên.** Mọi điểm cứu được nhiều hơn đều mua
   bằng overhead lớn hơn ngân sách video chịu nổi. Kết quả là **mặc định đứng nguyên**, các bản
   dự thi ở lại sau `--fec=…` làm tham chiếu — đúng như §"Giữ bản thua làm tham chiếu" dự liệu.

`overhead_pct` nay là một cột riêng trong CSV, và có test khẳng định depth tốn hơn ba lần
parity, để lần sau không ai đọc tỉ lệ cứu mà quên mất cái giá của nó.
- [x] Viết các impl còn thiếu theo đúng thứ tự cần thiết: Reed-Solomon, RS thích ứng, NACK-only
  — **xong cả ba**: Reed-Solomon (GF(256), Cauchy, khử Gauss); RS thích ứng qua
  `FecParityRowsFor(lossPct)` → 1/2/3 hàng parity, nối tới packetizer bằng `wantFecParity`;
  NACK-only đã trả lời bằng đo, không cần impl

> **Sáu phương án của A1 không phải sáu impl.** Đối chiếu lại với cây nguồn:
>
> | Phương án A1 | Thực chất | Trạng thái | Đặt từ CLI |
> | --- | --- | --- | --- |
> | XOR rải bước, depth dẫn xuất | baseline | xong — `xor` | `--fec=xor` |
> | XOR depth tách khỏi group size | tham số | xong — `SetFecGroups` + byte wire | `--fec-depth N` |
> | **Reed-Solomon (k,n)** | impl `FecScheme` | **xong — GF(256), ma trận Cauchy, khử Gauss** | `--fec=rs` |
> | RS + parity thích ứng | chính sách trên RS | **xong và đang chạy trên đường thật** — `FecParityRowsFor(lossPct)` → 1/2/3 hàng, ghi vào `wantFecParity` mỗi giây | `--fec-parity N` để **ghim**, chặn chính sách đè |
> | NACK-only | không phải scheme — là chế độ vá | **xong — quét 72 điểm, kết luận âm tính cũ bị lật** | `--fec-arm never` + `--nack` |
> | Lai FEC/NACK theo RTT | chính sách trên luật giữ frame | **xong — `OvertakenLimit()` suy từ cửa sổ vá; mặc định không đổi** | `--hold N` (viewer) |
>
> **NACK-only đã có câu trả lời mà không cần viết impl nào.** Sim nay có đường retransmit
> thật (`PlanNack` + `RetransmitCache`, NACK tốn trọn 1 RTT). Ở điểm đo được, host phục vụ
> đủ 16/16 chỉ số viewer hỏi, mọi gói đều tới — và **cứu được 0 frame**. `PopReady` bỏ frame
> non-IDR ngay khi có 2 frame mới hơn hoàn chỉnh, ở 60 fps là ~33 ms, còn gói vá về sau
> ~57 ms. Rút RTT xuống 4 ms thì chính đường đó bắt đầu cứu được.
>
> Nên "NACK-only" và "lai FEC/NACK theo RTT" **không phải câu hỏi về retransmit** — chúng là
> câu hỏi về `kStallTimeoutMultiple` và luật overtaken đặt cạnh RTT. Đó là Tier B (quét tham
> số), không phải Tier A (dựng interface).
>
> Cũng đo được: khi parity đang bật thì không frame nào thiếu gói đủ lâu để bị NACK
> (`nack_requests = 0`). FEC và NACK không tranh nhau cùng một lần vá.
>
> ⚠️ **Kết luận âm tính ở trên đã bị lật (02/09/2026).** Nó đo ở **đúng một điểm vận hành**
> (0,1% loss, luật overtaken cố định ở 2) — nên nó đo *luật giữ frame*, không đo retransmit.
> Phép quét 72 điểm mới (`out/bake-off/nack-hybrid.csv`, chế độ vá × RTT × độ dài giữ) ở
> **5% loss, burst 4**:
>
> | RTT | chế độ | giữ | cứu | IDR/phút | parity gửi |
> | --- | --- | --- | --- | --- | --- |
> | 40 ms | `fec-only` | – | 21 | 171 | 484 |
> | 40 ms | `nack-only` | 2 *(đang ship)* | 11 | 171 | **0** |
> | 40 ms | **`nack-only`** | **suy ra** | **30** | **72** | **0** |
> | 40 ms | `fec+nack` | suy ra | 38 | 90 | 484 |
> | 80 ms | **`fec+nack`** | **suy ra** | **41** | **63** | 484 |
> | 4 ms | `nack-only` | 2 hay suy ra | 30 | 126 | 0 |
>
> **`nack-only` với luật giữ suy từ RTT thắng `fec-only` ở cùng điểm — ít hơn 2,4× số keyframe
> mà không tốn một gói parity nào.** Ở RTT 4 ms không đổi gì, vì bản vá vốn về kịp trong hai
> frame: cái lợi là hàm của RTT so với chu kỳ frame. Vì thế `OvertakenLimit()` **suy ra** con
> số từ cửa sổ vá (`kNackHoldUs + RTT*3/2`) chứ không nâng một hằng số.
>
> **Mặc định đang ship không đổi**: `overtakenLimit_` mặc định là 2, phần suy ra chỉ chạm tới
> khi có caller nâng trần (`--hold N`). Quét trước, chốt sau — chưa chốt.
>
> Cái phải trả **không** phải độ trễ đều đặn: `longest_stall_ms` dịch chuyển cả hai chiều trong
> lưới (40 ms `nack-only`: 150→133; 40 ms `fec+nack`: 100→150), vì bớt keyframe có thể trả thừa
> cho cái chờ dài hơn. Nói "mua bằng độ trễ" là nói sai số liệu.
- [x] Quét scheme × tỉ lệ parity × group size × độ sâu interleave × loss × độ dài burst × RTT — 180 điểm, xem bảng ở trên
- [x] Chốt bản thắng theo tiêu chí đã đặt ở Phase 0 — không sửa tiêu chí sau khi thấy số
  — **kết luận: không có bản thắng, mặc định đứng nguyên.** Áp đúng ngưỡng đã đặt: không cấu
  hình nào đạt ≤20 IDR/phút ở 5% burst 4 (tốt nhất là 36, và phải trả 150% overhead). Ngưỡng
  CPU cũng không cứu được RS: 6,3× so với trần 3×, và điều khoản thoát "cứu gấp đôi" thì đạt
  nhưng vô nghĩa khi cái giá là băng thông chứ không phải CPU. Tiêu chí **không sửa sau khi
  thấy số** — đây là kết quả âm tính thật, không phải thất bại của phép quét
- ⏳ [#59](https://github.com/manhpham90vn/Deskhub/issues/59) — Xác nhận lại trên link thật bằng bài camera 240 fps qua `netem`, kiểm tra sim không nói dối
  — **đã gỡ chặn (02/09/2026).** Trước đây bài này không chạy được: `--fec` chỉ chọn tên sơ đồ,
  mà theo chính bảng kết quả `rs` ở parity = 1 tái lập `xor` tới từng chữ số. Hai trục thật sự
  — số hàng parity và độ sâu — thì `SetFecGroups` không có caller production nào (đúng lỗi
  `SetVideoPath` của Phase 1), còn tỉ lệ parity bị `ViewerBroadcast` ghi đè mỗi frame từ
  `FecParityRowsFor(lossPct)`. Trên link sạch chính sách trả về 1, nên `--fec=rs` trên WiFi tốt
  cho kết quả **không phân biệt được** với mặc định — đủ để ai đó kết luận nhầm "RS chẳng khác
  gì". Nay có `--fec-parity N` (ghim, chặn chính sách đè), `--fec-depth N`, và
  `--fec-arm always|policy` (giữ parity trên dây để cô lập sơ đồ khỏi chính sách bật, đúng như
  sim). Cả ba trục của phép quét giờ đặt được từ dòng lệnh; phần còn lại của mục này là **đi đo
  thật**, cần hai máy và `netem`

  **Đã chạy thử đường dây trên máy thật (02/09/2026)**, host là `deskhub-cli share --display all`
  trên Windows (NVENC, RTX 5070 Ti, 3440x1440 → 1920x802). Mỗi source nay in cấu hình FEC nó
  thật sự rơi vào, **đọc ngược từ packetizer** chứ không từ tuỳ chọn:

  | Xin trên dòng lệnh | Dòng `FEC measurement` in ra |
  | --- | --- |
  | `--fec=rs --fec-parity 3 --fec-depth 8 --fec-arm always` | `scheme=rs parity=3 depth=8 arm=always` |
  | `--fec=xor --fec-parity 3 --fec-depth 4 --fec-arm policy` | `scheme=xor parity=1 depth=4 arm=policy` + cảnh báo |
  | `--fec=rs --fec-parity 2 --fec-depth 1 --fec-arm policy` | `scheme=rs parity=2 depth=1 arm=policy` |

  Hàng giữa là hàng đáng giá nhất: `xor` chỉ chở được một hàng parity nên yêu cầu 3 bị từ chối
  kèm lý do, **còn depth thì vẫn nhận** — depth độc lập với scheme. Nếu không có dòng log này,
  một dòng CSV dán nhãn "xor parity=3" sẽ mô tả một cấu hình chưa từng chạy.
- [x] Nối bản thắng vào đường chạy thật; các bản khác ở lại sau `--fec=…` và ra khỏi ma trận fuzz/sanitizer
  — **xong cả hai nửa.** Bản thắng là mặc định đang chạy, nên "nối vào" ở đây là *khẳng định*
  chứ không phải đi dây lại: `TestOnlyTheWinnerIsReachableWithoutTheCommandLine` chốt rằng
  `Packetizer`/`Reassembler` khởi động bằng `kDefaultFecScheme` và `ShareOptions` /
  `ScreenClientConfig` không gọi tên sơ đồ nào — chỉ `deskhub-cli --fec=NAME` mới gọi.
  Ma trận: `DESKHUB_FEC_MATRIX=shipping` thu các phép quét về đúng sơ đồ đang ship
  (`FecSchemesUnderTest()` / `SchemeIsUnderTest()` trong `core/tests/support`), job sanitizer
  đặt biến đó trừ khi diff đụng `FecScheme` hoặc test của nó. **Đo được: 8,6 s → 2,9 s** cho
  một lần `core_tests` debug, trước khi ASan/TSan nhân lên. Giá trị lạ thì làm hỏng lần chạy
  chứ không lặng lẽ chọn giùm. Các libFuzzer target vốn đã không chạm `FecScheme` — chúng
  fuzz ở mức wire — nên nửa "fuzz" không cần làm gì
- [x] Ghi kết luận **kèm số đo của từng bản** vào `docs/ARCHITECTURE.md` kèm ba bản dịch
  — mục "Reed-Solomon at one parity row is XOR" đã chở số của từng bản (rescue, IDR/phút,
  overhead, CPU encode 143,9 µs so với 22,8 µs), và mục mới "A kept implementation earns its
  place by being reachable and tested" chở cái giá của việc giữ cùng ranh giới ma trận. Cả hai
  có đủ ở `vi`/`zh`/`ja`

### Phase 4 — Bake-off backend encoder

*C1. Rẻ nhất, vì interface và factory đã tồn tại sẵn.*

- [x] **Làm cho núm vặn có thật trước đã** — `CreateEncoder` giữ cái nào init được trước, nên trên
  máy có driver NVIDIA thì `MfEncoder` **không đo được**. Nay
  `--encoder auto|nvenc|mf|vaapi|videotoolbox` gọi tên backend; gọi tên một backend không khởi
  động được thì source **dừng**, không lặng lẽ đo
  cái còn lại. Windows chọn qua bảng trong `EncoderFactory`, Linux qua `HwEncoder::Init`, Apple
  từ chối mọi tên khác `videotoolbox`. Host in `[Encoder] measurement: backend=… requested=…`
  đọc ngược từ chính đối tượng encoder.
  ⚠️ **Chỉ nhánh Windows được biên dịch trên máy này.** `HwEncoder::Init` và
  `InstallVtEncoderFactory` chưa từng qua compiler ở đây — coi là chưa kiểm chứng cho tới khi CI
  Linux và macOS/iOS chạy
- [x] **Bộ đếm để chấm điểm** — `enc_ms_avg`/`enc_ms_max` không thấy được cái đuôi, mà đuôi mới là
  hàm mục tiêu của C1. `evt=sum` nay chở `enc_us_p50` và `enc_us_p99` từ histogram bước 512 µs
- [x] [#60](https://github.com/manhpham90vn/Deskhub/issues/60) — Dựng kịch bản đo: cùng clip, cùng bitrate, VMAF + CPU/GPU
  — **xong**: `make encoder-bake-off` (`scripts/encoder-bake-off.sh` + `deskhub_encbench`) nạp
  cùng một chuỗi frame BGRA thô vào **chính** encoder của Deskhub, đưa frame vào **đúng nhịp
  `--fps`** như một phiên share thật, rồi in `backend,enc_us_p50,enc_us_p99,cpu_pct,gpu_pct,vmaf,
  kbps` kèm SHA-256 của clip. Đo thử trên máy Intel UHD 750, 1920x1080/60, 20 Mbps, 240 frame:
  `mf` → p50 **1297 µs**, p99 14 789 µs, CPU 19,5%, GPU 26,1%, VMAF **100,00**, 20 004 kbps.
  Hai điều phải đọc kèm: (1) p50 khớp với dải 1,5-2,6 ms đã đo trên đường thật, tức harness không
  nói dối — nhưng chỉ sau khi **thêm pacing**; không pacing thì p50 là 15 ms vì đó là đo throughput
  chứ không phải đo độ trễ. (2) **VMAF 100,00 nghĩa là cột chất lượng không xếp hạng được gì** ở
  điểm vận hành này: 20 Mbps là quá thừa cho 1080p60, script nay tự cảnh báo khi mọi VMAF ≥ 99,5.
  Hạ xuống 2 Mbps thì cột này phân biệt được ngay: VMAF **90,71**, 2150 kbps. Rà lại 05/09 sửa hai
  chỗ: script bị commit ở mode 100644 nên `make encoder-bake-off` chết bằng `Permission denied`
  trên macOS/Linux, và danh sách backend mặc định kê cả `vaapi`/`videotoolbox` mà bench không dựng
  được — nay `EncoderFactory` phơi ra `BuiltInEncoderBackends()`, bench có `--backends`, script
  lấy mặc định từ chính bench. **Còn nợ:** lần chạy thứ hai trên cùng máy để chứng minh tái lập
- ⏳ [#61](https://github.com/manhpham90vn/Deskhub/issues/61) — Chạy trên từng backend: NVENC, Media Foundation, VA-API, VideoToolbox, MediaCodec
  — **đã có ba cột: NVENC, MF-qua-NVIDIA (02/09), MF-qua-Quick-Sync (04/09)** — xem hai bảng dưới.
  VA-API · VideoToolbox · MediaCodec vẫn cần máy khác. Nhắc lại: ba cột hiện có **đo trên hai máy
  khác nhau và không cùng kịch bản**, nên chưa cột nào so được với cột nào
- [x] Ghi lại backend nào hỗ trợ LTR — kết quả này quyết định A4 khả thi tới đâu
  — **cả hai backend Windows đều có**, đọc từ driver chứ không suy đoán: NVENC qua
  `nvEncGetEncodeCaps` cho `max_ltr_frames=8 ref_pic_invalidation=1 intra_refresh=1` trên
  RTX 5070 Ti; Media Foundation trả `IsSupported` = `S_OK` cho cả ba thuộc tính LTR lẫn
  `GradualIntraRefresh`. **Caps chỉ ghi log, chưa đưa vào `RecoveryPolicy`** — chưa có gì tiêu thụ
  `invalidateBeforeFrame` hay `wantIntraRefresh`, nên khai khả năng lúc này sẽ biến phục hồi mất
  gói thành vô tác dụng. Đó là phần thực thi của A4, không phải của C1
- ⏳ [#62](https://github.com/manhpham90vn/Deskhub/issues/62) — Thay tiêu chí chọn trong `EncoderFactory` từ "init được trước" sang bảng tra dựng từ số đo
  — chưa đổi, và số đo đầu tiên nói thứ tự hiện tại (NVENC trước) là **đúng trên máy NVIDIA**.
  Ghi thêm 03/09: bảng tra nên khoá theo `GpuChoice::vendor` (đã có sẵn, `GpuSelect.h`), vì
  hôm nay trên máy Intel hay AMD thì `CreateEncoder` vẫn nạp `nvEncodeAPI64.dll` trước rồi
  mới chịu thất bại — thứ tự đúng trên máy này đang phải trả giá trên mọi máy khác.
  **Đo được cái giá đó ngày 04/09:** trên máy Intel, `--encoder auto` in
  `[NVENC] Failed to load nvEncodeAPI64.dll` rồi mới rơi xuống MF. Tức mỗi lần tạo encoder trên
  máy không-NVIDIA đều trả tiền cho một lần `LoadLibrary` hỏng. Nó **không** làm hỏng gì — đường
  rơi chạy đúng và có in lý do — nên đây là việc dọn dẹp, không phải sửa lỗi.
  **Dữ liệu để dựng bảng tra nay đã có hai vendor:** `Nvidia` → nvenc trước, `Intel` → mf trước
  (Quick Sync khai đủ LTR + intra-refresh). Còn thiếu `Amd`, và `Unknown`/`Microsoft` (WARP) thì
  nên giữ nguyên thứ tự thử-lần-lượt như hôm nay

#### Phát hiện chặn trước cả phép đo: `IsSupported` bị đọc ngược cực, MF chạy bằng mặc định MFT

Mọi thuộc tính rate control của `MfEncoder` đi qua
`if (!codecApi->IsSupported(&api)) { report("NOT SUPPORTED"); return; }`. `S_OK` bằng **0**, nên
nhánh đó chạy đúng vào những thuộc tính MFT **có** hỗ trợ, còn `SetValue` chỉ được thử trên những
thuộc tính nó **không** hỗ trợ. In thêm HRESULT thô rồi chạy trên máy này:

| codecapi | trước khi sửa | HRESULT thật | sau khi sửa |
| --- | --- | --- | --- |
| `RateControlMode=CBR` | NOT SUPPORTED | `0x00000000` | ok |
| `MeanBitRate` | NOT SUPPORTED | `0x00000000` | ok |
| `GOPSize` | NOT SUPPORTED | `0x00000000` | ok |
| `BufferSize(VBV)` | NOT SUPPORTED | `0x00000000` | ok |
| `CommonLowLatency` | SetValue FAILED | `S_FALSE` | NOT SUPPORTED |
| `BufferInLevel` | SetValue FAILED | `S_FALSE` | NOT SUPPORTED |

Nghĩa là **backend MF suốt đời nó mã hoá bằng tham số mặc định của MFT** — không CBR, không bitrate
đích, không GOP vô hạn, không VBV — trong khi NVENC nhận trọn `PlanRateControl`. Chạy bake-off
C1 trước khi sửa là đem một encoder đã cấu hình so với một encoder chưa cấu hình: đúng loại lỗi
"đo một cấu hình không tồn tại" mà A1 đã dính một lần.

Nó cũng **lật bằng chứng của một mục trong `docs/ARCHITECTURE.md`**: dòng
`MeanBitRate: NOT SUPPORTED` từng được đọc là "MFT Intel không có thuộc tính này" thật ra có nghĩa
ngược lại. Kết luận của mục đó (fallback phải bắt buộc) vẫn đúng, lý do thì sai. `SetBitrate` và
`RequestKeyFrame` trong cùng file dùng **cực ngược lại** — hai chỗ gọi không thể cùng đúng, và
không test nào phân biệt nổi vì cả hai đều chỉ chạm phần cứng thật.

#### Đã đo: NVENC so với Media Foundation (02/09/2026)

Host là `deskhub-cli share --encoder NAME` trên Windows, nguồn 3440x1440 hạ xuống 1920x802,
20 Mbps, 60 fps, **desktop rảnh chứ chưa phải clip cố định** — nên đọc như một lần chạy thử đường
dây, không phải bake-off.

| | NVENC | Media Foundation |
| --- | --- | --- |
| Số cửa sổ | 6 | 4 |
| `enc_us_p50` | 2560–5632 | 512–13824 |
| `enc_us_p99` | **2755–5731** | **12303–17580** |
| `enc_ms_avg` | 2,0–4,9 | 3,5–9,0 |
| LTR | có | có |
| Intra-refresh | có | có |

Cột MF lấy từ lần chạy **sau** khi sửa cực `IsSupported` — tức lần đầu tiên MF thật sự chạy với
CBR, bitrate đích, GOP vô hạn và VBV như NVENC. Không trộn với lần chạy trước đó.

**Cảnh báo phải nói kèm mỗi lần trích:** trên máy này `mf` rơi vào **"NVIDIA H.264 Encoder MFT"** —
cùng một con chip. Đây **không** phải câu hỏi Intel-so-với-NVIDIA mà C1 đặt ra; đây là cái giá của
việc đi vòng qua Media Foundation để tới cùng phần cứng. Muốn trả lời câu hỏi thật của C1 phải chạy
trên một máy Intel, nơi Media Foundation dẫn tới MFT của Quick Sync. Và cả hai cột đều đo trên nội
dung không kiểm soát được, nên chênh lệch p50 không mang nghĩa gì cho tới khi có clip cố định.

#### Đã đo: Quick Sync trên một máy Windows thứ hai (04/09/2026) — cột Intel của C1

Máy này là **Intel UHD 750, không có driver NVIDIA** (`nvEncodeAPI64.dll` không tồn tại), nên
`--encoder mf` rơi vào **"Intel Quick Sync Video H.264 Encoder MFT"** — đúng con đường mà bảng
02/09 không chạm tới được. Cùng một lệnh `deskhub-cli share`, nguồn 3440x1440 hạ xuống 1920x802,
20 Mbps, 60 fps, desktop rảnh.

| | Quick Sync qua MF (04/09) |
| --- | --- |
| Số cửa sổ | 16 |
| `enc_us_p50` | **1536–2560** |
| `enc_us_p99` | **2480–8371** (một cửa sổ vọt 26944) |
| `enc_ms_avg` | 1,1–2,0 (một cửa sổ 5,5) |
| LTR | **có** — `LTRBufferControl` / `MarkLTRFrame` / `UseLTRFrame` đều `hr=0` |
| Intra-refresh | **có** — `GradualIntraRefresh` `hr=0` |
| `BufferInLevel` | **NOT SUPPORTED** (khác NVENC) |

**Ý nghĩa cho A4:** Quick Sync khai đủ cả LTR lẫn intra-refresh, nên `PrepareRecovery` có chỗ thi
hành trên máy Intel y như trên máy NVIDIA. Nghĩa là A4 không phụ thuộc riêng NVENC.

⚠️ **Bốn cảnh báo phải nói kèm bảng này:**

1. **Đây không phải bake-off.** So nó với bảng 02/09 là so **hai máy khác nhau** — khác CPU, khác
   GPU, khác cỡ và nội dung desktop. Con số Quick Sync trông đẹp hơn cột MF của 02/09, nhưng điều
   đó **không** kết luận được gì về Quick Sync so với NVENC
2. Vẫn là **desktop rảnh, không phải clip cố định** — đúng cái thiếu mà bảng 02/09 đã tự nhận
3. Chưa có **VMAF**: đây là số độ trễ, không nói gì về chất lượng ảnh ở cùng bitrate
4. Sàn nhiễu của chính máy này rất rộng (xem Phase 6) — đừng đọc chênh lệch nhỏ

**Việc còn lại để bảng này thành bake-off thật:** dựng kịch bản đo chung (cùng clip, cùng bitrate,
VMAF + CPU/GPU) rồi chạy lại **cả hai máy** qua nó. Việc ấy không bị chặn bởi phần cứng nào.

#### Luật gọi tên backend: đã xác nhận chạy thật (04/09/2026)

Trên máy không có NVIDIA, ba đường đều đúng như thiết kế:

| Lệnh | Kết quả |
| --- | --- |
| `--encoder nvenc` | `[NVENC] Failed to load nvEncodeAPI64.dll` → `[Encoder] nvenc was named on the command line and would not start, so this source stops rather than measuring a different backend under its name.` → **nguồn dừng** |
| `--encoder auto` | `nvenc unavailable, trying the next backend...` → MF/Quick Sync, có in lý do |
| `--encoder mf` | vào thẳng Quick Sync |

Đây là lần đầu luật "gọi tên cái không khởi động được thì **dừng**, không lặng lẽ đo cái khác dưới
tên nó" được chạy thật thay vì chỉ có trong test.

### Phase 5 — Lặp lại trên những thành phần dễ nhất

*A2 → A3 → A5.*

- [x] A2 — thêm `virtual` vào `Update()`, viết impl delay-gradient và SCReAM
  — contract `CongestionControl` + bốn bản: `aimd` (giữ nguyên), `delay-trend`, `scream`,
  `hybrid`. `SourcePipelineState::rate` đổi sang con trỏ đa hình.
  **Cảnh báo khi đọc tên:** `Feedback` mỗi giây chỉ mang loss%, RTT, recv rate — **không có
  dấu thời gian tới của từng gói**, nên bộ lọc delay-gradient đúng kiểu WebRTC (trên biến
  thiên độ trễ giữa các nhóm) không dựng được ở đây. `delay-trend` dùng độ trễ hàng đợi suy
  từ phần RTT vượt trên mức tối thiểu; `scream` bám tốc độ nhận báo về, bị chặn bởi chính độ
  trễ đó. Chúng là **bản thích nghi với tín hiệu dây này chở**, không phải bản cài lại các
  bài báo — đem so với số liệu GCC hay RFC 8298 đã công bố là so hai thuật toán khác nhau
- [x] A2 — dùng ca overshoot đã ghi trong `docs/ARCHITECTURE.md` làm test hồi quy
- [x] A3 — chấm điểm bằng chính struct `Stats` đã có, vẽ đường cong đánh đổi độ trễ
  — đường cong 21 điểm (target 20…200 ms × jitter 0/15/40 ms) xuất CSV.
  **Kết quả không nghiêng về bản thích ứng tôi viết:** ở jitter 0 nó thắng rõ (giữ 20 ms thay
  vì 60 ms, cùng số gap); nhưng ở jitter 40 ms nó giữ cùng 60 ms như target cố định mà tốn
  4 gap thay vì 1. Chi phí nằm ở chính việc thích ứng — nâng target giữa chừng buộc buffer
  chờ nạp lại, và cái chờ đó là underrun. **Chưa nên bật mặc định**
- [x] A5 — so ba bản ước lượng offset, lấy về một con số latency công bố được
  — contract `ClockOffsetEstimator` với ba bản `rolling-min`, `trendline`, `kalman`; **và**
  con số tuyệt đối đã có: `PingPong` nay chở thêm `hostTimeUs` (payload 12 → 20 byte, peer cũ
  đọc 12 byte đầu nên tương thích ngược), `ClockSync` giữ mẫu có RTT nhỏ nhất trong cửa sổ để
  ước lượng offset đồng hồ, và `e2e_abs_ms` xuất hiện cạnh `e2e_ms` trên dòng `evt=sum`.
  **Ràng buộc phải nói kèm:** offset chỉ tách được nếu giả định đường đi hai chiều đối xứng —
  đó là giả định của NTP và là giới hạn nền tảng, không phải thiếu sót của bản cài

#### Kiểm lại (02/09/2026): cả bốn núm vặn A2/A3/A5/A6 đều **không có caller production nào**

Áp đúng bài kiểm tra đã dùng cho A1 ("một núm vặn không có caller ngoài chính test của nó thì
không phải núm vặn") lên phần còn lại của Tier A. Kết quả trước khi sửa:

| Núm | Caller ngoài test | Hậu quả |
| --- | --- | --- |
| `SourcePipelineState::SetCongestionControl` **(A2)** | **0** | 3/4 impl CC không reachable, luôn chạy `aimd` |
| `AudioJitterBuffer::SetAdaptiveTarget` **(A3)** | **0** | bản thích ứng chỉ sống trong test |
| `VideoPacer::SetAdaptiveLead` · `SetDisplayIntervalUs` **(A6)** | **0** | pacing thích ứng và khớp vsync không bật được |
| `MakeClockOffsetEstimator` **(A5)** | **0** | cả 3 bản chỉ sống trong test |

**Đã sửa A2 và A3:**

- **A2** — `--cc aimd|delay-trend|scream|hybrid` ở `share`, nối qua `ShareOptions` tới
  `HostEngine`. Host in `measurement: cc=… fec=… parity=… depth=… arm=…`, đọc ngược từ đối
  tượng thật. Viewer không có tiếng nói ở đây nên `--cc` bị từ chối ở `connect`
- **A3** — `--audio-delay MS` và `--audio-adaptive` / `--no-audio-adaptive` ở `connect`, nối qua
  `ScreenViewerConfig` tới `AudioPlayer::Start`. Dòng `evt=player_start` nay chở
  `target=fixed|adaptive`. Trần 500 ms thành hằng số công khai `kMaxAudioDelayMs` — trước đó
  trần thật nằm trong một hằng số private của chính sách thích ứng

**Không tách A3 ra file riêng, và đây là lý do:** phần khác nhau giữa target cố định và target
thích ứng đúng **6 dòng** trong `Push()`. Dựng contract + hai file cho 6 dòng là nhân hạ tầng
chứ không phải tách phương án. Việc tách file đã làm ở nơi thật sự có nhiều impl độc lập:
`fec/` (2), `cc/` (4), `clock/` (3).

**A5 và A6 còn treo, vì chúng chỉ chạm được đường Apple.** `VideoPacer` — nơi duy nhất
`ClockOffset` được dùng — chỉ có caller trong `VtDecoder`. Windows và Linux không dùng
`VideoPacer` chút nào. Nên nối `--clock` hay `--vsync` vào CLI trên hai OS đó sẽ tạo ra đúng
cái núm vặn giả mà mục này vừa đi bắt. `RollingMinEstimator` đã được xác nhận là **wrapper
thuần quanh chính `ClockOffset`**, nên thay `VideoPacer::offset_` bằng contract sẽ không đổi
hành vi — nhưng phải làm cùng lúc với việc cho một viewer không-Apple dùng `VideoPacer`, chứ
không phải trước

**Đã làm A5 và A6 (02/09/2026), và A5 cho ra một kết quả âm tính đáng giữ.**

- **A5** — `VideoPacer` nay giữ `std::unique_ptr<ClockOffsetEstimator>` thay vì `ClockOffset`
  cụ thể, cộng `SetClockOffset(name)`. Không đổi hành vi (đã xác nhận `rolling-min` là wrapper
  thuần). Phép quét mới 18 điểm (3 bản × wobble 0/5/20 ms × có/không bước nhảy transit 30 ms):
  **ba bản không phân biệt được** — phase spread 6898–6937 µs ở mọi điểm, `kalman` tái lập
  `rolling-min` tới từng micro giây.

  **Lý do nằm ở interface, không ở thuật toán:** pacer gọi `AddSample` · `ready` · `Reset` ·
  `floorUs`, **không bao giờ gọi `LatencyUs`** — mà đó mới là method duy nhất ba bản cài khác
  nhau. `KalmanEstimator::floorUs()` trả thẳng `lowest_`, tức đúng rolling minimum. Nên **A5
  không chấm điểm được bằng judder**; trục nó làm dịch chuyển là `e2e_abs_ms`, chỗ test riêng
  của nó vốn đã đo.

- **A6** — thêm concept `PacedDecoder` vào `VideoContract.h`; `ScreenViewer` gọi
  `decoder.SetPacing(adaptive, displayIntervalUs)` và `SetClockOffset(...)` qua `if constexpr`,
  nên decoder nào không hỗ trợ thì bỏ qua sạch. `VtDecoder` cài hai method đó, forward tới
  `pacer_`. `ScreenViewerConfig` nay chở `pacingAdaptive` · `displayIntervalUs` · `clockOffset`.

**Vẫn không thêm cờ CLI cho A5/A6, và lý do đã kiểm chứng:** `deskhub-cli` **không có viewer
bản macOS** (`client/cli/` chỉ có `ViewerX11.cpp`, `ViewerWin32.cpp`, `ViewerNone.cpp`). X11
dùng `AvDecoder`, Win32 dùng `WinVideoDecoder` qua FFI — không cái nào dùng `VideoPacer`. Nên
pacer chỉ chạy trong app Apple, và một cờ `--pacing`/`--vsync`/`--clock` trên CLI sẽ không tới
đâu ở mọi OS mà CLI chạy được. Đường đo A6 là app macOS/iOS, hoặc sim.

⚠️ **Chưa biên dịch được `VtDecoder` trên máy này** (Apple-only). Hai method mới chỉ forward
thẳng tới `pacer_`, nhưng coi là chưa kiểm chứng cho tới khi CI macOS/iOS chạy.

### Phase 6 — Hiệu năng đường truyền và nhịp hình

*P2, A6, C2.*

- [x] P2 — `sendmmsg`/`recvmmsg`, rồi GSO/GRO trên Linux; **còn RIO và phép đo ngưỡng bitrate**
  — **xong phần gộp syscall và phần gộp phía nhận (GRO/URO, 04/09).** `UdpSocket::SendBatch` nay
  thử `UDP_SEGMENT` (GSO) trước rồi mới rơi về `sendmmsg`; `UdpSocket::RecvBatch` là `recvmmsg`
  với `MSG_WAITFORONE` trên Linux và vòng `RecvFrom` ở mọi OS khác; `QuicEndpoint::Poll` đọc theo
  lô 16 gói thay vì từng gói một. **Nhánh Linux nay đã được biên dịch, chạy test và strace trên chính máy này** —
  ngược với ghi chú cũ; nhánh Windows là nhánh chưa qua compiler ở đây.
  **GRO và URO đã xong (04/09/2026), RIO thì chưa.** GRO gộp nhiều datagram vào một buffer, tức
  phá vỡ giao kèo "một slot một datagram" mà `RecvBatch` dựng trên — đúng như ghi chú cũ nói, đó
  là một thay đổi API riêng, và nó đã được làm như một thay đổi API: chi tiết ở mục "Đã làm" bên
  dưới. Xem kết quả đo ở dưới

  **Bổ sung 03/09 (chiều): phía gửi trên Windows nay cũng gộp, bằng USO.** Ghi chú cũ nói
  "nhánh Windows chưa qua compiler ở đây" là đúng nhưng chưa đủ — `UdpSocketWin::SendBatch`
  hồi đó **không phải nhánh chưa đo, nó là vòng `sendto` từng gói**, tức phần gộp lô của
  Phase 6 chỉ tồn tại trên Linux. Nay nó gọi `WSASendMsg` với control message
  `UDP_SEND_MSG_SIZE` — đối ứng Windows của `UDP_SEGMENT`, và là thứ đáng làm trước RIO mà
  mục P2 ở trên liệt kê, vì rẻ hơn hẳn. Cùng một luật cắt run, cùng một lần tắt vĩnh viễn khi
  stack từ chối, cùng một đường rơi về gửi từng gói.

  `LeadingRunOfEqualSegments` được nâng từ `UdpSocketPosix.cpp` lên
  `deskhubp/net/UdpSocket.h` để hai OS dùng chung đúng một luật thay vì chép lại, và nó có
  test riêng — luật "gói ngắn chỉ được là segment cuối" giờ khoá được bằng test đơn vị chứ
  không chỉ bằng bài loopback.

  ✅ **Đã đo trên Windows (04/09, phiên chiều).** Bảng riêng ở mục "Đã đo: USO và URO trên
  Windows" bên dưới. Bảng loopback Linux vẫn là bảng Linux — hai bảng nói hai chuyện khác nhau,
  đừng trộn.

  **URO** (`UDP_RECV_MAX_COALESCED_SIZE`) đã viết cùng GRO trong đúng một thay đổi API, như ghi
  chú cũ đã dự đoán. ✅ Nhánh Windows **đã qua compiler và đã chạy trên loopback** — nhưng
  loopback Windows **không gộp một lần nào**, nên câu "URO có nổ trên NIC thật không" vẫn chưa có
  câu trả lời

#### Đã làm: gộp gói phía nhận, và cái phải trả để có nó (04/09/2026)

`RecvBatch` từng hứa **một slot một datagram**. GRO (`UDP_GRO`, Linux) và URO
(`UDP_RECV_MAX_COALESCED_SIZE`, Windows) phá đúng lời hứa đó: một lần đọc trả về cả một dãy
datagram cùng cỡ trong một buffer, cỡ segment nằm trong control message. Nên giao kèo đổi, chứ
không phải cài thêm một đường vòng:

- `InboundDatagram` có thêm trường `segment`; `DatagramsIn(slot)` và `DatagramAt(slot, i)` trong
  `deskhubp/net/UdpSocket.h` tách một slot trở lại thành đúng những datagram đã gửi. Hai hàm này
  test được không cần socket, giống `LeadingRunOfEqualSegments`
- `segment == 0` là "slot chứa đúng một datagram" — tức **mọi caller không xin gộp**. Giao kèo cũ
  là mặc định, không phải ngoại lệ
- Gộp phải xin mới có: `EnableReceiveCoalescing()`. Không tự bật

**Lý do phải xin, đo tại chỗ chứ không đoán:** một run GSO 16 × 1200 byte về thành **một** skb gộp
19 200 byte. Đọc nó vào buffer 2048 byte thì `recvmsg` trả 2048 byte, bật `MSG_TRUNC`, và **vứt
mất 17 152 byte còn lại** — lần đọc kế tiếp không còn gì. Kernel gộp tới trọn payload IP 64 KB,
nên slot nhỏ hơn `kMaxCoalescedBytes` là mất dữ liệu, không phải là chậm hơn. `RecvBatch` ghi ca
`MSG_TRUNC` thành lỗi nói thẳng đòi hỏi đó.

Vì thế `QuicEndpoint` **đổi số slot lấy cỡ slot**: 16 × 1350 byte trên stack → 4 × 65 535 byte do
endpoint sở hữu. Quét 4 · 8 · 16 slot: cả ba nằm trong nhiễu giữa các lần chạy của nhau, nên chọn
bản tốn ít bộ nhớ nhất — số slot không phải là núm đáng vặn.

Đo trên loopback, release build, **chạy xen kẽ hai binary** để triệt cái trôi nhiệt của máy (bốn
cặp, lấy trung vị):

| workload | trước | sau | |
| --- | ---: | ---: | ---: |
| đọc burst 16 gói (`udp/loopback-batched` → `udp/loopback-coalesced-recv`) | 573 ns/datagram | **203 ns/datagram** | 2,8x |
| `quic/stream-drain-scaling` | 2095 ns/KB | **1629 ns/KB** | −22% |
| `quic/stream-throughput-64k` | 2114 ns/KB | **1785 ns/KB** | −16% |
| `quic/datagram-delivery` | 4115 ns | 4139 ns | không đổi |
| `quic/handshake` | 374 227 ns, 13 alloc | 392 611 ns, 15 alloc | +5%, ở sàn nhiễu |

**Sàn nhiễu của máy này là ~5%, và bảng trên tự đo lấy nó:** những dòng mà mã hai binary **giống
hệt nhau** vẫn xê dịch 3,3-5,1%. Nên +4,9% của `quic/handshake` không tách khỏi nhiễu được, dù nó
có nguyên nhân thật (13 → 15 lần cấp phát, một trong số đó là buffer đọc 256 KB cấp lúc `Start`).
Còn +0,6% của `quic/datagram-delivery` là câu trả lời cho lo ngại đáng lo nhất: đọc thêm control
message **không tốn gì** cho đường một-gói-một-lần.

Bằng chứng GRO thật sự nổ, không phải suy từ số: test loopback in
`the stack coalesced up to 8 datagrams into one read`.

#### Đã đo: USO và URO trên Windows (04/09/2026) — và URO **không gộp gì cả**

Máy Intel UHD 750, build `x64-release`, loopback, **11 lần chạy**, lấy trung vị. Bốn workload nằm
trong **cùng một binary**, nên không cần dựng bản "trước" để so — chúng tự so với nhau:

| workload | gửi | nhận | trung vị | dải (min…max) |
| --- | --- | --- | ---: | --- |
| `udp/loopback-one-syscall-each` | từng gói | từng gói | 7183 ns/datagram | 5809…8399 |
| `udp/loopback-send-batched-recv-each` | **USO** | từng gói | **3942 ns/datagram** | 3528…5311 |
| `udp/loopback-batched` | **USO** | `RecvBatch` | 4028 ns/datagram | 3407…5449 |
| `udp/loopback-coalesced-recv` | **USO** | **URO** | 3797 ns/datagram | 3291…4934 |

**USO là toàn bộ phần thắng, và nó tách khỏi nhiễu rất sạch:** 7183 → 3942 ns/datagram (**−45%**),
và hai phân bố **không chồng lên nhau một điểm nào** — `min` của hàng một-syscall (5809) vẫn lớn
hơn `max` của hàng USO (5311). Đây là kết quả chắc chắn nhất của cả phiên.

**Ba hàng phía nhận thì không tách được khỏi nhau:** trung vị lệch nhau 6%, còn ba dải thì chồng
lên nhau gần như trọn vẹn (3291…5449). Nói "URO nhanh hơn `RecvBatch`" trên số này là nói quá.

**Vì sao: URO trên loopback Windows không gộp một lần nào.** Đây không phải suy từ số, mà đo thẳng
bằng một probe nhỏ dựng trên `platform.lib`:

- `EnableReceiveCoalescing()` trả về **true** — stack **nhận** `UDP_RECV_MAX_COALESCED_SIZE`
- gửi một run USO 16 × 1200 byte, rồi đọc bằng slot **2048 byte** (nhỏ hơn `kMaxCoalescedBytes`
  rất nhiều)
- nhận về **đúng 16 datagram qua 16 lần đọc riêng lẻ**, mỗi lần `segment == 0`, **không mất một
  byte nào**

Tức cái bẫy đã chặn nhánh Linux — buffer nhỏ **mất dữ liệu** — **không bắn trên loopback Windows**,
đơn giản vì không có run gộp nào để mà cắt. Bài test cũng nói đúng điều đó bằng lời:
`note: the stack coalesced nothing this time, so only the split was tested`.

⚠️ **Cho nên đừng kết luận "URO vô dụng trên Windows".** Kết luận đúng là: **loopback không trả lời
được câu hỏi này** — cần một NIC thật, và tốt nhất là một NIC có bật URO ở driver. Con đường
`WSAEMSGSIZE` trong `RecvCoalesced` (nuốt lỗi, trả 0) vẫn **chưa từng chạy**, vì muốn chạm vào nó
thì stack phải gộp trước đã.

#### Sàn nhiễu của máy Windows này rộng gấp một bậc so với máy Linux

Dải của **từng** workload ở trên là **45-60%** quanh giá trị nhỏ nhất, trong khi máy Linux đo được
sàn nhiễu ~5% (bằng chính những hàng có mã giống hệt nhau). Hệ quả thực dụng: **trên máy này mọi
khác biệt dưới khoảng 1,5x là không đo nổi.** Các hàng QUIC còn tệ hơn:

| workload | trung vị | dải |
| --- | ---: | --- |
| `quic/handshake` | 5 347 675 ns | 45% |
| `quic/datagram-delivery` | 11 245 ns | 80% |
| `quic/stream-throughput-64k` | 5686 ns/KB | 28% |
| `quic/poll-idle` | 1658 ns | 32% |
| `quic/terminal-record-delivery` | 28 027 ns | 23% |

⚠️ **Đừng so bảng này với bảng Linux để kết luận về hệ điều hành.** Hai máy khác CPU, khác đời,
khác cả nhiệt độ. Cái bảng này nói được đúng một điều: **muốn đo hiệu năng vi mô thì đừng đo trên
máy này** — hoặc phải chạy đủ nhiều lần và chỉ tin những khác biệt to như USO.

**Chưa làm: RIO**, và **ngưỡng bitrate mà CPU thành nút cổ chai** vẫn thuộc danh sách "cần link
thật" — loopback đo chi phí mỗi gói, không đo chỗ một link thật no
- [x] A6 — pacing thích ứng và khớp vsync, đo judder
  — quét 24 điểm (lead 8…66 ms × wobble 0/5/20 ms, có và không khớp vsync) xuất CSV.
  **Phát hiện: trục "độ trễ cộng thêm" của A6 không có trade nào cả.** Không khớp vsync thì
  phase spread ~6000 µs trên chu kỳ 6944 µs ở *mọi* mức lead — mua lead gấp tám lần không thu
  hẹp được một micro giây. Khớp vsync đưa spread về **0** và không tốn độ trễ nào
- [x] C2 — đo độ trễ capture của WGC, đối chiếu với DXGI Desktop Duplication
  — **đã có số (04/09/2026, phiên chiều trên máy Windows): WGC rẻ, không đáng viết backend
  Duplication.** Intel UHD 750, capture 3440 × 1440 rồi hạ xuống 1920 × 802 @60fps, 20 Mbps,
  qua 16 cửa sổ một giây:

  | | dải qua 16 cửa sổ |
  | --- | --- |
  | `cap_us_p50` | **0,5-2 ms** (một vài cửa sổ lên 7-8 ms) |
  | `cap_us_p99` | **2-20 ms** |
  | `cap_repeat` | **0** suốt cả bài |

  Để so: `enc_us_p50` của chính lần chạy đó là 1,5-2,6 ms. Tức **WGC trao một khung hết đúng
  khoảng thời gian encoder sau đó bỏ ra để nén nó** — sự tiện lợi ấy không đắt, và **không có
  con số xấu nào để biện minh cho một backend Duplication**. Câu hỏi C2 đặt ra coi như đã trả lời;
  cờ `--capture wgc|dxgi` chỉ viết nếu sau này có lý do khác.

  ⚠️ Một ngoại lệ phải nói kèm: **cửa sổ đầu tiên sau `Start` cho `cap_us_p99=242 ms`.** Đó là
  tuổi của chính khung đầu tiên, không phải cái đuôi ở trạng thái ổn định — đừng trích con số đó
  ra khỏi ngữ cảnh này.

  Cơ chế: `SourceDiag::NoteCapture(frameTimestampUs, nowUs)` ở `core/`, 9 check ở
  `core/tests/diag/DiagTests.cpp`. Lý do thiết kế đã chuyển vào `docs/ARCHITECTURE.md`
  §Decisions (cùng ba bản dịch).
- [x] Tier B — thêm jitter vào backoff của `LinkRecovery`; nhớ đây là đổi chữ ký `ReconnectDelayUs` cộng mọi caller, không phải sửa tại chỗ

#### Đã đo: gộp syscall UDP, và cái nó phơi ra (03/09/2026)

Đo bằng `platform_perf` trên loopback, release build, so trước/sau trên cùng máy. Loopback không
nói gì về thông lượng của một đường thật — nhưng những thứ nó gỡ đi là thời gian đồng hồ trên mọi
đường.

**Phát hiện chính, và nó lớn hơn chính phép gộp lô: mỗi lần `QuicEndpoint::Poll` đều kết thúc bằng
một giấc ngủ 1 ms.** Vòng đọc cũ gọi `RecvFrom` cho tới khi một lần trả về rỗng, mà "rỗng" chỉ về
sau khi `SO_RCVTIMEO` hết hạn. Nên một lần poll đã vét sạch socket vẫn trả trọn cái sàn timeout, ở
*mọi* lần gọi. `SessionTransport::RecvFrom` vốn đã chặn chính `Poll` ấy sau `WaitReadable(10 ms)`
của nó, nên giấc ngủ kia là phần cộng thêm thuần tuý trên mọi lần nhận.

Đọc theo lô bỏ hẳn lần đọc thăm dò: lô về ngắn nghĩa là socket rỗng, không cần hỏi lại. Còn lại
`SetRecvTimeout(0)` — POSIX hiểu là "chờ mãi mãi" nên code cũ ép thành 1 ms — nay nghĩa là
"đừng chờ" (`O_NONBLOCK` trên POSIX, `FIONBIO` trên Windows). An toàn vì mọi caller truyền 0 đều đã
có cái chờ của riêng mình bao quanh.

| workload | trước | sau | |
| --- | ---: | ---: | ---: |
| `quic/poll-idle` | 1 978 692 ns | **732 ns** | 2700x |
| `quic/terminal-record-delivery` | 4 244 632 ns | **9 201 ns** | 461x |
| `quic/stream-throughput-64k` | 61 210 ns/KB (16,7 MB/s) | **2 045 ns/KB (500,7 MB/s)** | 30x |
| `quic/datagram-delivery` | 248 515 ns | **3 986 ns** | 62x |
| `quic/handshake` | 47 947 298 ns | **361 538 ns** | 133x |
| `quic/stream-drain-scaling` | 62 469 ns/KB, 3,78x | **1 958 ns/KB, 3,84x** | vẫn tuyến tính |

**Bản thân phép gộp lô mua được ít hơn giấc ngủ nó phơi ra, và phía gửi mua được nhiều hơn phía
nhận.** `sendmmsg` vốn đã gom cả burst vào một syscall, nên GSO mua phần việc trong kernel chứ
không mua số syscall — một lượt qua ngăn xếp UDP/IP thay vì mười sáu:

| đường đi, 16 × 1200 byte trên loopback | ns/datagram |
| --- | ---: |
| `sendto` từng gói + `recvfrom` từng gói | 2036 |
| GSO gửi + `recvfrom` từng gói | 636 |
| GSO gửi + `recvmmsg` | **623** |

Tức GSO đáng 3,2x; bước cuối nằm trong sai số giữa các lần chạy — hai dòng gộp lô đổi chỗ cho nhau
giữa hai lần — vì trên loopback kernel đã giữ sẵn mọi gói và một lần nhận gần như miễn phí.
`recvmmsg` vẫn xứng chỗ vì nó gỡ đi *lý do* khiến vòng lặp phải thăm dò. Đo trên một lần chạy
`platform_tests` dưới `strace`: 17 317 datagram về trong 1631 lần gọi có kết quả — **10,6 gói mỗi
syscall**; phía gửi 16 953 datagram trong 1724 lần `sendmsg` mang `UDP_SEGMENT` — 9,8 gói mỗi lần.

**Cái thứ ba, chỉ lộ ra sau khi giấc ngủ biến mất: `Poll` cấp phát ~1,5 lần mỗi lần gọi.**
`Service()` dựng một `std::vector` id kết nối mỗi lần, `DrainStreams` một buffer 16 KB mới,
`DrainDatagrams` một buffer 1350 byte. Khi mỗi poll còn ngủ 1 ms thì không ai thấy; vừa bỏ ngủ thì
`quic/terminal-record-delivery` nhảy từ 9 lên **27** lần cấp phát mỗi record — cùng một record nay
tốn gấp ba số vòng poll. Sửa bằng mảng stack chặn bởi `kMaxConnections` và buffer do endpoint sở
hữu; `quic/poll-idle` về **0,00** lần cấp phát mỗi poll. Ngân sách cấp phát của workload đó đạt
chuẩn bao năm nay thật ra đang đo giấc ngủ, không đo đường chạy.

**Chưa trả lời:** ngưỡng bitrate mà CPU thành nút cổ chai. Loopback đo được chi phí *mỗi gói*, không
đo được chỗ mà một link thật no. Muốn con số đó phải chạy trên đường thật ở nhiều mức bitrate, cùng
bài đo với Phase 0 — nó thuộc về danh sách "cần link thật", không phải về sim.

### Phase 7 — Những món trải khắp ba lớp

*A4, C3. Đắt nhất, làm sau cùng.*

- ⏳ [#63](https://github.com/manhpham90vn/Deskhub/issues/63) — A4 — chính sách phục hồi ở `core/`, thực thi ở từng backend encoder mà Phase 4 xác nhận là có hỗ trợ
  — **phần `core/` xong**: `media::RecoveryPolicy` chọn giữa IDR · invalidate long-term
  reference · intra-refresh theo `EncoderRecoveryCaps`, có leo thang khi báo mất lần hai tới
  trước lúc bản vá đầu kịp có tác dụng. `HostNetLoop` nay hỏi chính sách thay vì
  `forceIdr.store(true)` vô điều kiện. Hai concept tuỳ chọn
  `ReferenceInvalidatingEncoder` và `IntraRefreshEncoder` đã có trong `VideoContract.h`.
  **Chưa backend nào khai báo hỗ trợ**, nên caps rỗng và hành vi vẫn đúng như cũ — phần thực
  thi cần encoder thật, tức chờ Phase 4.

  **Phase 4 đã trả lời được nửa câu hỏi (02/09/2026):** cả NVENC lẫn Media Foundation trên Windows
  đều báo có LTR và intra-refresh, đọc thẳng từ driver. Nhưng caps **vẫn chưa** đưa vào
  `RecoveryPolicy`, và đó là cố ý: `HostNetLoop` đặt `invalidateBeforeFrame` / `wantIntraRefresh`
  mà **không encoder nào đọc hai biến đó**. Khai caps lúc này sẽ khiến chính sách chọn một hành
  động không ai thực thi — tức mất gói sẽ không còn xin IDR nữa mà cũng chẳng vá gì. Việc còn lại
  đúng bằng một mệnh đề: cho `NvencEncoder` cài `MarkLongTermReference`/`InvalidateReference`
  (`NV_ENC_PIC_PARAMS` có sẵn trường), rồi mới gọi `SetCaps`

  **Mệnh đề đó đã viết xong cho Windows (03/09/2026) — nhưng chưa gặp một gói mất thật nào.**
  Thứ tự làm là bắt buộc và đã theo đúng: cho encoder **thi hành được** trước, rồi mới khai
  caps. Khai ngược lại thì chính sách chọn một hành động không ai làm, và mất gói sẽ chẳng
  còn được vá bằng gì cả.

  - `IVideoEncoder` có thêm `MarkLongTermReference` / `InvalidateReference` /
    `BeginIntraRefresh`, và `static_assert` buộc nó vào hai concept tuỳ chọn trong
    `VideoContract.h` — đúng cách hai concept kia đã được đặt ra để dùng
  - **NVENC** chạy LTR Per Picture (`enableLTR=1`, `ltrTrustMode=0`), ring 4 slot,
    `maxNumRefFrames` nâng theo, mark bằng `ltrMarkFrame`/`ltrMarkFrameIdx`, vá bằng
    `ltrUseFrames`/`ltrUseFrameBitmap`, refresh bằng `forceIntraRefreshWithFrameCnt`.
    `nvEncInvalidateRefFrames` **cố ý không dùng**: bitmap là đường tất định — nó nói thẳng
    khung kế tiếp được tham chiếu cái gì, thay vì nói cái gì đã hỏng rồi đoán phần còn lại.
    Nếu `InitializeEncoder` từ chối LTR thì tự thử lại một lần không có LTR, để một driver
    khó tính không làm hỏng cả buổi share
  - **Media Foundation** đi qua `AVEncVideoLTRBufferControl` lúc init rồi
    `MarkLTRFrame`/`UseLTRFrame`/`GradualIntraRefresh` theo từng khung. `RequestKeyFrame` nay
    quên ring — IDR xoá sạch DPB, giữ lại record là tự nói dối
  - `deskhubp/host/EncoderRecovery.h` mới: `PrepareRecovery()` tiêu thụ
    `invalidateBeforeFrame`/`wantIntraRefresh`, **rơi về IDR khi encoder không thi hành
    được**, và mark đúng những khung mà `RecoveryPolicy` sẽ gọi tên sau này (kể cả IDR — bỏ
    sót chỗ này thì `core` tin có một LTR mà encoder không giữ). Bọc trong `if constexpr`
    theo concept, nên Linux · Apple · Android **giữ nguyên hành vi hôm nay** cho tới khi
    encoder của họ có ba hàm kia
  - `SharingHost` Windows gọi `recovery.SetCaps(encoder->RecoveryCaps())` sau **mỗi** lần
    tạo encoder — `SetCaps` reset luôn trạng thái, đúng thứ cần khi encoder dựng lại và mọi
    LTR cũ đã mất. Preamble nằm trong `EncodeTimed` nên cả đường frame lẫn đường flush đều
    đi qua, không phải chép hai chỗ
  - `RecoveryPolicy` **nay có mutex**. Đây là một cuộc đua có thật chứ không phải phòng xa:
    `OnReferenceLost` chạy trên luồng net loop, `NoteEncoded`/`ShouldMarkLongTerm` chạy trên
    luồng encode dưới `encMutex`. Trước đây không ai thấy vì đường này chưa bao giờ chạy;
    bật nó lên là TSan bắt ngay. Lớp này giờ không copy được nữa — không chỗ nào copy cả
  - 8 check mới ở `platform/tests/session/EncoderRecoveryTests.cpp`, chạy trên encoder giả
    cả loại có lẫn loại không có ba hàm kia

  **Trạng thái sau phiên Windows 04/09:** cả `NvencEncoder` lẫn `MfEncoder` **đã qua compiler**,
  build sạch với `DESKHUB_WERROR=ON`, và 8 check ở `EncoderRecoveryTests.cpp` xanh. Trên máy
  Intel dùng hôm đó, `MfEncoder` khai `ltr=1 intra_refresh=1` đọc từ chính Quick Sync — nghĩa là
  **A4 có chỗ thi hành trên cả Intel, không riêng NVIDIA**.

  ⚠️ **Nhưng vẫn chưa gặp một gói mất thật nào.** Máy 04/09 không có NVENC, nên bảng bằng chứng
  của NVENC (`ltr_slots=4`, `recovery=invalidate_ref`, `[NVENC] recovery: next picture
  references long-term frame M only.`, `idr=0`) **chưa đọc được**. Muốn đóng việc này cần một máy
  Windows **có NVIDIA** cộng một viewer báo mất gói (netem trên Ubuntu là đủ, xem đầu chương).
  Đường Quick Sync thì về nguyên tắc kiểm được ngay trên máy Intel bằng đúng bài netem ấy —
  chưa làm.

  **Còn lại của A4:** bốn backend kia (VA-API, NVENC-Linux, VideoToolbox, MediaCodec) vẫn
  chưa khai gì, nên vẫn xin IDR như cũ — đúng như thiết kế, không phải thiếu sót. Tài liệu
  **đã xong** (03/09, phiên tối): hai bullet mới ở `docs/ARCHITECTURE.md` §Decisions worth
  remembering — một cho thứ tự "cho thi hành trước, khai caps sau", một cho cuộc đua mà
  TSan bắt — cùng ba bản dịch, và câu "chưa backend nào khai báo" ở bullet A4 cũ đã sửa
- [x] C3 — dựng bảng khả năng codec và cơ chế thương lượng khi kết nối
  — `kCodecMaskH264` / `H264High444` / `Hevc` / `Av1` trên wire, `NegotiateCodec(hostMask,
  clientMask)` chọn theo bảng ưu tiên tường minh và luôn tụt về H264 4:2:0 khi đó là thứ duy
  nhất chung. Host nay dùng nó thay cho phép thử một bit. Tương thích ngược: peer cũ chỉ báo
  bit 0 nên vẫn thoả thuận ra H264 và nhận về giá trị 0 như trước.
  **Thứ tự ưu tiên là tạm** — vị trí của 4:4:4 (chữ sắc nét) so với AV1/HEVC (ít bit hơn) là
  câu hỏi phải giải bằng đo, chưa giải
- ⏳ [#64](https://github.com/manhpham90vn/Deskhub/issues/64) — C3 — thêm 4:4:4 cho use case đọc chữ, giữ 4:2:0 làm baseline phổ quát
  — **chưa viết dòng nào, và có một phát hiện phải quyết trước khi viết (03/09/2026).**
  Bảng khả năng ở mục trên là thật, nhưng **không đầu nào điền nó**: `SetHostCodecMask`
  (`ScreenHostSession.h:71`) **không có caller nào trong toàn repo**, còn
  `ScreenClient.cpp:58` gán cứng `hello.codecMask = kCodecMaskH264`. Nghĩa là thương lượng
  hôm nay chỉ có đúng một kết quả khả dĩ, bất kể bảng ưu tiên viết gì.

  Tệ hơn cho 4:4:4 nói riêng: viewer Windows giải mã qua H.264 decoder của Media Foundation,
  vốn chỉ 4:2:0; repo **không có đường NVDEC** nào. Nên nếu chỉ làm phía encode thì được một
  cái núm không ai gọi được — đúng bài kiểm tra mà chính tài liệu này đã dùng để loại
  `--video-path` và `--encoder` khỏi danh sách "đã có núm vặn".

  Quyết trước, code sau: hoặc (a) điền mask hai đầu từ khả năng thật rồi dừng lại ở đó, hoặc
  (b) chấp nhận viết cả đường decode 4:4:4 trước khi phía host được phép khai bit đó

### Phase 8 — Công bố

- [x] Đăng CSV thô, script và cách chạy lại — kèm cả những chỗ Deskhub thua
  — `scripts/bake-off-csv.sh` chạy `core_tests` rồi tách ra bốn bảng: `fec-sweep.csv` (180
  dòng), `nack-hybrid.csv` (72), `audio-delay.csv` (21), `pacer-judder.csv` (24). Mọi dòng sinh từ sim có seed, tất
  định, không mạng không GPU — chạy lại cho ra đúng từng byte. Phần "chỗ Deskhub thua" đã có
  thật trong dữ liệu: A3 thích ứng thua target cố định dưới jitter
- [x] [#65](https://github.com/manhpham90vn/Deskhub/issues/65) — Viết bài từ dữ liệu bake-off: "tôi thử N sơ đồ FEC dưới burst loss, đây là số liệu"
  — **xong**: `docs/posts/fec-under-burst-loss.md` kèm đủ ba bản dịch `.vi`/`.zh`/`.ja`, và
  `docs/ARCHITECTURE*.md` §9 trỏ sang bài bằng đúng ngôn ngữ của nó. CSV thô
  check thẳng vào `docs/data/bake-off/{fec-sweep,nack-hybrid}.csv`, mỗi bảng trong bài kèm một lệnh
  `awk` lọc đúng những dòng đã trích, và mọi cảnh báo phương pháp nằm **cạnh** bảng chứ không ở
  cuối bài. Bài nói thẳng bốn chỗ Deskhub thua: RS ở 1 hàng parity ra số liệu **giống hệt** XOR
  trên cả 40 cặp mà tốn 6,3× CPU; depth đưa overhead 12,5% → 90,9%; **chính sách bật FEC đang ship
  gây 20 IDR/phút** ở điểm vận hành đo được, còn FEC luôn bật thì 0; và cấu hình **duy nhất** đạt
  ngưỡng ở điểm ấy là NACK-only trên link ngắn, tức bản không có FEC. Kỷ luật "chốt ngưỡng trước
  khi nhìn số" bị phá được ghi ngay ở đầu bài, không giấu
