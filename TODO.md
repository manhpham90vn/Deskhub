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

## Ba lớp, ba vai trò

Việc chia lớp của repo đã quyết sẵn mỗi phần của một cuộc bake-off nằm ở đâu. Đừng chống lại
nó:

| Lớp | Vai trò trong bake-off |
| --- | --- |
| `core/` | Giữ **contract** (concept hoặc interface), **chính sách** quyết định, và **simulator**. Không mạng, không GPU, chạy được trong CI |
| `platform/` | Giữ các seam một-API-cho-mọi-OS: socket, QUIC, audio, đồng hồ. Đây là nơi các quyết định của `core/` gặp hệ điều hành thật |
| `client/` | Giữ **bản triển khai per-OS**: capture, encode, decode, render. Nhiều impl cho cùng một contract là chuyện bình thường ở đây |

**Điều đáng chú ý nhất trong cả bản kiểm kê: pattern bake-off đã tồn tại sẵn trong dự án.**
`core/include/deskhub/media/VideoContract.h` định nghĩa các concept `VideoEncoderLike`,
`VideoDecoderLike`, `CongestionAwareDecoder`, `PresentTimingDecoder`; còn
`client/windows/cpp/encode/IVideoEncoder.h` có `static_assert` ràng buộc mình vào các concept
đó, với `NvencEncoder` và `MfEncoder` là hai impl đứng sau một `EncoderFactory`. Linux cũng
vậy với `HwEncoder.h`, `NvEncoder` và `VaEncoder`.

Contract nằm ở `core/`, impl nằm ở `client/`, `static_assert` nối hai đầu. Đúng hình hài cần
có. Thứ duy nhất thiếu là **phép đo giữa các impl** — hiện tại chưa có cái nào từng được so
với cái nào.

## Quy tắc: không phải cái gì cũng cần interface

Cám dỗ lớn nhất của kế hoạch này là bọc interface quanh mọi thứ. Đừng. Mỗi interface là một
lớp gián tiếp, một nhánh test, và — với repo này — thêm một bề mặt cho ASan, TSan và fuzz
phải quét. Mười một thành phần mỗi cái ba bản triển khai là con số tổ hợp mà CI không bao giờ
chạy hết.

Chỉ dựng interface khi thoả **cả hai**: có không gian thiết kế thật (nhiều thuật toán đúng
khác nhau), và lựa chọn đó **đo được** bằng một con số người dùng cảm nhận được. Còn lại thì
để yên.

Quy tắc này là hàng phòng thủ chính, và nó càng quan trọng vì các impl **ở lại sau bake-off**
chứ không bị xoá (§"Giữ bản thua làm tham chiếu"). Kỷ luật nằm ở chỗ không dựng interface bừa,
chứ không nằm ở chỗ dọn dẹp về sau.

| Tầng | Nghĩa là gì | Việc cần làm | Số thành phần |
| --- | --- | --- | :--: |
| **Tier A** | Nhiều thuật toán đúng, khác biệt đo được, ảnh hưởng trực tiếp cái người dùng thấy | Contract + nhiều impl + bake-off | 11 |
| **Tier B** | Có không gian thiết kế nhưng tác động nhỏ, hoặc chỉ là chuyện chỉnh tham số | Quét tham số, không dựng interface | 10 |
| **Tier C** | Chỉ có đúng và sai, không có lựa chọn thuật toán | Để yên — đã có test và fuzz | ~60 |

---

# Tier A — `core/`

## A1. Sơ đồ FEC

**File:** `core/include/deskhub/transport/Packetizer.h` · `transport/Reassembler.h` ·
`protocol/Wire.h:21`

**Hiện tại:** XOR, `kFecGroupSize = 8`, đúng **một** gói parity cho mỗi group — cứu được tối
đa 1 gói mất trên mỗi group. Bật/tắt bằng một `bool` trong `SetFecEnabled()`.

**Group đã rải bước sẵn, không liền khối.** `Packetizer.cpp:31` gán gói `i` vào group
`i % numGroups`, còn `Reassembler.cpp:75` phục hồi bằng
`for (i = group; i < pktCount; i += numGroups)`. Nghĩa là **interleaving đã có trong bản đang
chạy** — nó không phải một phương án để thêm vào.

Nhưng độ sâu interleave là **dẫn xuất, không phải lựa chọn**: depth = `numGroups` =
`ceil(count/8)`. Keyframe 40 gói được depth 5. Delta frame 8 gói cho `numGroups == 1` — một
group duy nhất, **interleave bằng không** — và đó chính là loại frame chiếm đa số ở 60 fps
(`LossGoodputTests.cpp:21` đang mô hình delta frame đúng bằng 8 gói). Chống burst mạnh nhất ở
chỗ ít cần nhất, và biến mất ở chỗ cần nhất. Đây mới là phát hiện của A1, và nó là một tham
số cần tách ra chứ không phải một thuật toán cần viết.

**Overhead cũng không phải 1/8, và đổi theo từng frame.** Số gói parity là `ceil(count/8)`,
nên overhead là `ceil(count/8)/count` — chỉ xấp xỉ 12,5% khi `count` là bội số lớn của 8.
Frame 9 gói trả 22%. Frame 1 gói trả 100%. Tệ hơn: mọi gói parity đều phát ở nguyên
`kParityStride` (`kFecLenPrefix + kMaxVideoPayload` = 1176 byte) bất kể gói dữ liệu trong
group nhỏ đến đâu (`Packetizer.cpp:44-50`), nên một delta frame nhỏ vẫn mua trọn một gói
parity full-MTU.

**Phải sửa trước khi đo:** cắt gói parity xuống theo gói dữ liệu lớn nhất trong group.
`TryRecover` chỉ cần parity ≥ `2 + độ dài gói lớn nhất`, nên việc này an toàn và ngắn. Không
sửa thì mọi lần quét tỉ lệ parity ở Phase 3 đều đo trên một baseline bị thổi phồng, và số
liệu áp lực hàng đợi mà P1 cần cũng bị thổi theo.

Ở 1% loss ngẫu nhiên, khoảng 0,27% group không cứu được; ở 5% thì lên khoảng 5,7%. Hai con số
đó là P(mất ≥2 trên 8), bỏ qua trường hợp mất chính gói parity — cộng thêm khoảng 0,07 điểm
phần trăm ở mức 1% và 1,5 điểm ở mức 5%. Và vì loss WiFi đi theo cụm nên thực tế còn tệ hơn.

**Phương án:**

- **XOR rải bước, depth dẫn xuất** *(đang dùng)* — rẻ nhất về CPU, nhưng mù ở frame nhỏ.
- **XOR với depth tách rời khỏi group size** — cùng overhead, cùng CPU, chỉ khác cách chọn
  tham số. Đây là phép quét phải chạy đầu tiên, không phải một impl mới.
- **Reed-Solomon (k,n)** — cứu k gói bất kỳ, CPU trung bình, GF(256) có SIMD.
- **RS + tỉ lệ parity thích ứng** theo loss đo được.
- **NACK-only** — miễn phí khi mạng sạch, tốn thêm 1 RTT mỗi lần mất.
- **Lai FEC/NACK chuyển theo RTT** — nhiều khả năng là đáp án đúng.

**Thị trường:** Moonlight dùng Reed-Solomon. WebRTC dùng FlexFEC kết hợp NACK, chọn theo
RTT. RustDesk nghiêng hẳn về retransmit. Không ai dừng ở XOR đơn parity.

**Hàm mục tiêu:** số lần phải xin IDR trên mỗi phút, ở mỗi tổ hợp (loss rate × độ dài burst
× RTT). Không phải "tỉ lệ cứu được gói" — người dùng không thấy gói, họ thấy ảnh khựng.

> **Phụ thuộc:** phải làm **P1 trước**. Nếu một phần đáng kể "loss" thực ra là do hàng đợi
> datagram của mình tự bỏ gói, thì cuộc bake-off này đang tối ưu cho một mô hình sai.

## A2. Điều khiển tắc nghẽn

**File:** `core/include/deskhub/control/BitrateController.h`

**Hiện tại:** AIMD theo mất gói, cộng tín hiệu backlog (`kBacklogMs = 150`,
`kSevereBacklogMs = 400`) và `kCleanSecondsBeforeDroppingFec = 10`.

Đây là thành phần **dễ A/B nhất trong toàn repo**: `Update(Feedback, frameAgeMs, nowUs)`
trả về `BitrateDecision` đã là một interface hoàn hảo sẵn rồi. Một hàm thuần, vào là
feedback, ra là quyết định. Chỉ cần thêm `virtual`.

**Phương án:**

- **AIMD theo loss + backlog** *(đang dùng)* — đơn giản, an toàn, phản ứng sau khi hàng đợi
  đã đầy.
- **Delay-gradient / trendline filter** (kiểu GCC) — thấy nghẽn trước khi mất gói.
- **SCReAM (RFC 8298)** — viết riêng cho video real-time, đọc RFC thay vì đọc code nên sạch
  về giấy phép.
- **Lai loss + delay**, lấy giá trị bảo thủ hơn trong hai.

**Thị trường:** WebRTC dùng GCC (delay-gradient) — chuẩn công nghiệp cho video real-time.
Moonlight gần như không tự thích ứng, để người dùng đặt bitrate cố định. Sunshine đẩy quyết
định về phía client.

**Hàm mục tiêu:** thời gian hồi phục sau khi băng thông tụt · bitrate trung bình giữ được ·
số lần overshoot tự gây loss.

`docs/ARCHITECTURE.md` đã ghi lại đúng ca overshoot này: controller đọc 15 ms RTT thành
headroom rồi kéo bitrate lên lại. Dùng chính ca đó làm test hồi quy đầu tiên.

> **Phụ thuộc:** đọc P1 trước khi đụng vào. Có một controller thứ hai bên dưới mà thành phần
> này không biết là có tồn tại.

## A3. Jitter buffer âm thanh

**File:** `core/include/deskhub/transport/AudioJitterBuffer.h`

**Hiện tại:** target delay cố định `kDefaultAudioDelayMs = 60`, che bằng conceal khi thiếu
frame.

Thành phần này **đã có sẵn hàm mục tiêu**: struct `Stats` đang đếm `framesConcealed`,
`framesLate`, `underruns`, `resyncs`. Không cần chế thêm gì để chấm điểm.

**Phương án:**

- **Delay cố định 60 ms** *(đang dùng)* — đơn giản, phí độ trễ khi mạng tốt, thiếu khi mạng
  xấu.
- **Target thích ứng** theo jitter đo được.
- **NetEQ-style** — co giãn thời gian bằng WSOLA thay vì drop hoặc chèn im lặng.

**Thị trường:** NetEQ của WebRTC là chuẩn vàng, và khác biệt nghe thấy rõ khi mạng phập
phù — tai người nhạy với gián đoạn âm thanh hơn nhiều so với một frame hình bị rớt.

**Hàm mục tiêu:** `(underruns + framesConcealed)` trên mỗi phút, vẽ theo trục độ trễ audio
trung bình. Đây là đánh đổi trực tiếp, nên kết quả phải là một đường cong chứ không phải
một con số.

## A4. Phục hồi sau mất gói

**File:** `core/include/deskhub/session/host/ViewerFeedback.h` ·
`session/host/SourcePipeline.h` · impl encoder ở `client/*/encode`

**Hiện tại:** xin IDR nguyên khung — 37 chỗ trong `core/` nhắc tới `IDR`, **không có chỗ nào
dùng long-term reference** (đã grep lại cả `core/`, `platform/` và `client/`: không có gì).
Ở 20 Mbps, một IDR là cú vọt bitrate đột ngột, mà cú vọt đó lại gây thêm loss.

**Phương án:**

- **IDR** *(đang dùng)* — đơn giản, encoder nào cũng có, spike bitrate.
- **LTR + reference invalidation** — rẻ hơn IDR cả chục lần.
- **Intra-refresh cuộn** — không bao giờ có spike, đổi lại chất lượng thấp hơn chút.

**Thị trường:** Moonlight và Sunshine đều dùng reference invalidation trên NVENC. Đây là
chênh lệch rõ nhất giữa Deskhub và nhóm dẫn đầu.

**Cảnh báo:** đây là món **đắt nhất trong Tier A và trải khắp ba lớp**. Chính sách quyết định
khi nào xin cái gì nằm ở `core/`; khả năng thực thi thì nằm ở bốn code path encoder khác
nhau — NVENC và Media Foundation trên Windows, VA-API và NVENC trên Linux, VideoToolbox trên
Apple, MediaCodec trên Android. Mỗi cái hỗ trợ LTR một kiểu, có cái không hỗ trợ. Để sau
cùng, và làm sau khi C1 đã cho biết backend nào thực sự đáng đầu tư.

**Hàm mục tiêu:** độ lớn spike bitrate sau mất gói · thời gian tới khi ảnh sạch trở lại.

## A5. Ước lượng lệch đồng hồ

**File:** `core/include/deskhub/control/ClockOffset.h`

**Hiện tại:** rolling-min trên cửa sổ `kWindowUs = 10 s` — lấy min quan sát được làm mốc 0,
rồi `LatencyUs()` trả về phần vượt trên mốc đó.

**Hệ quả cần biết:** `e2e_ms` mà app hiển thị là số *tương đối*, không phải latency tuyệt
đối — baseline một chiều bị triệt tiêu. Tốt để bắt bufferbloat, nhưng không đem so với tool
khác được.

**Phương án:**

- **Rolling-min 10 s** *(đang dùng)* — bền với nhiễu, mù với drift chậm.
- **Hồi quy tuyến tính / trendline** — tách được drift khỏi jitter.
- **Kalman** — chính xác nhất, khó chỉnh nhất.

**Vì sao đáng làm:** cải thiện chỗ này là điều kiện cần để có một con số latency *công bố
được*, mà đó lại chính là con số cần cho bảng so sánh và bài viết.

**Hàm mục tiêu:** sai số ước lượng so với offset thật, trong sim có drift và jitter đã biết
trước.

## A6. Nhịp phát hình

**File:** `core/include/deskhub/control/VideoPacer.h` · `control/FrameGate.h` ·
`transport/Pacer.h`

**Hiện tại:** lead cố định `kDefaultLeadUs = 33 ms`, resync khi lệch quá
`kResyncThresholdUs = 250 ms`. `FrameGate` nhận frame với dung sai
`kJitterToleranceUs = 500 µs`.

**Phương án:**

- **Lead cố định 33 ms** *(đang dùng)* — một frame ở 30 fps, hai frame ở 60 fps.
- **Lead thích ứng** theo jitter đo được — trả lại độ trễ khi mạng tốt.
- **Khớp vsync màn hình client** — bỏ judder do lệch pha.

**Thị trường:** Moonlight có tuỳ chọn frame pacing khớp vsync, và người dùng cảm nhận được
rõ.

**Hàm mục tiêu:** phương sai khoảng cách giữa hai lần hiển thị (judder), vẽ theo trục độ trễ
cộng thêm.

---

# Tier A — `platform/`

## P1. Hai tầng điều khiển tắc nghẽn chồng lên nhau

**File:** `platform/src/net/QuicEndpoint.cpp:74` (`MakeConfig`)

**Đây là phát hiện quan trọng nhất của cả bản kiểm kê, và là lý do bản chỉ có `core/` là
chưa đủ.**

`MakeConfig` đặt idle timeout, kích thước payload, cửa sổ flow control, số stream, và bật
datagram với `kDatagramQueue`. Nhưng nó **không bao giờ gọi
`quiche_config_set_cc_algorithm`** — nghĩa là quiche đang chạy thuật toán mặc định của nó.

Trong QUIC, DATAGRAM frame **có** chịu điều khiển tắc nghẽn. Vậy nên video của bạn đang đi
qua CC của quiche, trong khi `BitrateController` (A2) ngồi bên trên quyết định bitrate
encoder mà không hề biết tầng dưới tồn tại. Hai bộ điều khiển chồng nhau, không cái nào biết
cái nào.

Hệ quả cụ thể: khi CC của quiche không cho datagram ra, chúng dồn vào `kDatagramQueue`; hàng
đợi đầy thì gói bị bỏ **ngay trong máy mình**. Với `Reassembler` thì cái đó trông y hệt mất
gói trên mạng — và FEC ở A1 sẽ tốn parity để cứu thứ mà chính mình vứt đi.

**Thuật toán mặc định đó là CUBIC** — không cần thí nghiệm để biết, chỉ cần thấy rằng
`quiche_config_set_cc_algorithm` không xuất hiện ở bất kỳ đâu trong cây nguồn.

**Phương án:**

- **Mặc định, không cấu hình** *(đang dùng)* — chạy CUBIC của quiche, nhưng lựa chọn đó nằm
  trong thư viện chứ không nằm trong code mình.
- **Chọn tường minh thuật toán CC** cho quiche và chỉnh AIMD cho khớp.
- **Để quiche làm CC duy nhất**, bỏ vòng điều khiển bitrate của mình.
- **Để AIMD làm chủ**, cấu hình quiche thoáng nhất có thể để nó không can thiệp.
- **Đưa video sang `VideoPath::RawUdp`**, bỏ hẳn CC của quiche khỏi đường video.
  `SessionTransport.cpp:480` đã có sẵn nhánh này; mặc định là `QuicDatagram`
  (`SessionTransport.h:129`) và ngoài test không ai gọi `SetVideoPath`.

**Phép đo rẻ nhất, chạy trước tiên:** cùng link, cùng tải, video trên `RawUdp` so với
`QuicDatagram`. Không phải viết dòng code nào, và nó cô lập đúng phần đóng góp của CC quiche.

**Hàm mục tiêu:** tỉ lệ datagram bị bỏ ở `kDatagramQueue` so với loss thật đo được trên dây.
Nếu tỉ lệ này đáng kể thì mọi kết luận của A1 và A2 đều lệch.

**Bộ đếm cần thêm đã có sẵn chỗ để ở:** `QuicSendStats` và `Impl::Counters`
(`QuicEndpoint.cpp:717-725`) đang đếm `datagrams` và `capped`, còn `SendDatagram` (dòng 699)
đã rẽ nhánh trên `sent > 0` — chỉ là đang vứt tín hiệu thất bại đó đi. Đếm nó rồi phơi ra
dòng diag cạnh `e2e_ms`: khoảng mười dòng, không phải một buổi.

## P2. Gộp syscall cho UDP

**File:** `platform/src/net/UdpSocketPosix.cpp:48` · `UdpSocketWin.cpp:69`

**Hiện tại:** chỉ đặt `SO_RCVBUF` và `SO_SNDBUF`. Không có `sendmmsg`/`recvmmsg`, không UDP
GSO (`UDP_SEGMENT`), không GRO, không RIO trên Windows. Mỗi datagram là một syscall.

Ở 1080p60 tại 20 Mbps, đó là hàng nghìn syscall mỗi giây cho mỗi chiều, nhân với số viewer.

**Phương án:**

- **Một syscall một gói** *(đang dùng)* — đơn giản, di động, tốn CPU.
- **`sendmmsg` / `recvmmsg`** — gộp nhiều gói mỗi lần gọi, chỉ POSIX.
- **UDP GSO + GRO** trên Linux — kernel tự cắt và gộp, giảm syscall mạnh nhất.
- **RIO (Registered I/O)** trên Windows.

**Thị trường:** đây là bài tối ưu chuẩn của mọi stack QUIC hiệu năng cao; quiche có sẵn
đường đi cho GSO.

**Hàm mục tiêu:** CPU% ở host và client tại 1080p60 20 Mbps · số syscall mỗi giây · và quan
trọng nhất, ngưỡng bitrate mà tại đó CPU trở thành nút cổ chai.

---

# Tier A — `client/`

## C1. Chọn backend encoder

**File:** `client/windows/cpp/encode/EncoderFactory.cpp` · `encode/IVideoEncoder.h` ·
`client/linux/cpp/encode/HwEncoder.h` · contract ở `core/media/VideoContract.h`

**Hiện tại:** `CreateEncoder()` thử `NvencEncoder` trước, hỏng thì rơi xuống `MfEncoder`,
hỏng nữa thì trả `nullptr`. Linux có `NvEncoder` và `VaEncoder`. Apple có `VtEncoderApple`,
Android có `MediaCodecEncoder`.

**Vấn đề không nằm ở cấu trúc mà nằm ở tiêu chí chọn:** factory chọn theo *cái nào khởi tạo
được trước*, không theo *cái nào tốt hơn*. Trên một máy có NVIDIA, `MfEncoder` không bao giờ
được thử — dù trên một số cấu hình Intel, MFT mà Media Foundation dẫn tới lại cho latency
thấp hơn. (Repo không có backend QSV riêng; Media Foundation chính là đường tới MFT của
Intel.)

Linux thì không thuần "init được trước": `HwEncoder.h:20` chặn NVENC sau
`frameKind == FrameMemory::Mapped && NvEncoder::DriverPresent()`. Tiêu chí đã có sẵn ở đó,
chỉ là chưa ai đo xem nó chọn có đúng không.

**Đây là chỗ bake-off rẻ nhất trong toàn dự án**, vì interface, factory và `static_assert`
ràng buộc vào concept của `core/` đều đã tồn tại. Chỉ thiếu phép đo.

**Phương án:**

- **Cái nào init được trước** *(đang dùng)* — không tốn gì, có thể chọn nhầm.
- **Xếp hạng theo bảng tra sẵn** dựng từ kết quả bake-off, theo vendor và thế hệ GPU.
- **Đo một lần lúc chạy đầu tiên** rồi nhớ lựa chọn.
- **Đo định kỳ** — nhiều khả năng là thừa.

**Hàm mục tiêu:** VMAF ở cùng bitrate · encode latency p99 · CPU/GPU chiếm dụng · **có hỗ
trợ LTR hay không** (kết quả này quyết định A4 có khả thi trên backend nào).

## C2. Backend capture

**File:** `client/windows/cpp/capture/ScreenCapture.cpp` ·
`client/linux/cpp/capture/ScreenCapture.cpp` · `platform/src/media/PortalScreenCast.cpp`

**Hiện tại:** Windows dùng **Windows.Graphics.Capture**. Linux đi qua `xdg-desktop-portal` và
PipeWire. macOS dùng ScreenCaptureKit, iOS dùng ReplayKit qua Broadcast Extension, Android
dùng MediaProjection.

**Phương án trên Windows:**

- **Windows.Graphics.Capture** *(đang dùng)* — bắt được từng cửa sổ riêng, sống sót khi đổi
  độ phân giải, hỗ trợ HDR tốt hơn.
- **DXGI Desktop Duplication** — độ trễ thấp hơn ở chế độ toàn màn hình, nhưng chỉ toàn màn
  hình và đứt khi đổi chế độ hiển thị.

**Hàm mục tiêu:** độ trễ capture→texture p50/p99 · số frame trùng lặp bị trả về · hành vi khi
đổi độ phân giải giữa phiên.

Ưu tiên thấp hơn C1: WGC nhiều khả năng vẫn là lựa chọn đúng, nhưng cần một con số để biết
mình đang trả bao nhiêu độ trễ cho sự tiện lợi đó.

## C3. Codec và chroma

**File:** toàn bộ đường encode/decode ở cả năm client

**Hiện tại:** chỉ H.264, chỉ 4:2:0. Grep ra 158 tham chiếu `H264`, chỉ có `NV12` và
`YUV420`, không có HEVC, AV1 hay VP9 ở bất kỳ đâu.

**Phương án:**

- **H.264 4:2:0** *(đang dùng)* — HW decode ở mọi nơi kể cả iPhone và Android đời cũ.
- **H.264 4:4:4** — chữ sắc nét, đúng use case "chạy VS Code từ quán café" mà README bán
  ngay ở dòng đầu. Hỗ trợ phần cứng hẹp hơn nhiều.
- **HEVC** — khoảng 30% tốt hơn ở cùng chất lượng, vướng patent, decode trên Android không
  đều.
- **AV1** — tốt nhất ở bitrate thấp, không patent, chỉ GPU rất mới.

**Đây là câu hỏi ma trận phần cứng, không giải bằng simulator.** Đáp án gần như chắc chắn:
giữ H.264 4:2:0 làm baseline phổ quát, **thương lượng nâng cấp** khi cả hai đầu cùng hỗ trợ.
Việc cần làm là dựng bảng khả năng và cơ chế thương lượng, không phải chạy đua.

---

# Tier B — chỉnh tham số, đừng dựng interface

| Lớp | Thành phần | Hiện tại | Việc cần làm |
| --- | --- | --- | --- |
| `core/` | `transport/RetransmitCache.h` | `kCacheFrames = 8` | Quét kích thước cache và chính sách NACK trong cùng cái sim của A1 — nó vốn là mặt kia của bài toán FEC |
| `core/` | `session/LinkRecovery.h` | Backoff 0,5 s → 5 s | **Lỗi nhỏ:** backoff không có jitter. Nhiều viewer mất kết nối cùng lúc sẽ reconnect đồng pha. Không cần bake-off, nhưng là **đổi API chứ không phải sửa tại chỗ**: `ReconnectDelayUs(attempt)` là hàm thuần theo `attempt`, mà `core/` không được include `deskhubp/system/Random.h`, nên caller phải truyền vào seed hoặc một tỉ lệ 0..1 |
| `core/` | `media/RgbDownscale.h` | Đường fallback khi không có HW scaler | Box vs bilinear vs Lanczos, chấm bằng SSIM và ns/frame. Chỉ chạm đường fallback nên ưu tiên thấp |
| `core/` | `media/RatePlan.h` · `control/QualityLadder.h` · `media/QualityPreset.h` | Thang chất lượng cố định | Quét tham số cùng lúc với A2 — ladder và controller là một hệ, chỉnh riêng lẻ sẽ ra kết luận sai |
| `core/` | `net/LanScan.h` | Quét subnet | mDNS hay broadcast beacon là câu hỏi UX và bảo mật, không phải câu hỏi thuật toán. Quyết bằng tranh luận, không bằng đo |
| `core/` | `control/LinkStats.h` · `control/StreamSize.h` | Cửa sổ thống kê | Là *đầu vào* của A2. Chỉnh sau khi A2 đã chốt, không chỉnh trước |
| `platform/` | `audio/AudioSink*.cpp` (5 impl) | WASAPI · PipeWire · CoreAudio · AAudio · None | Đã là pattern nhiều impl sau một API. Không cần đua — nhưng nên quét kích thước buffer và chế độ shared/exclusive của WASAPI |
| `platform/` | `media/OpusCodec.cpp`, hằng số ở `core/media/AudioTypes.h:10` | Opus 64 kbps (`kAudioBitrateBps`), frame 20 ms | Quét bitrate và kích thước frame theo chất lượng cảm nhận. `OpusCodec.cpp` nhận bitrate làm tham số, nên chỗ cần sửa là hằng số ở `core/`. Ưu tiên thấp, 64 kbps đã đủ tốt |
| `platform/` | `net/UdpSocket*.cpp` | `SO_RCVBUF` / `SO_SNDBUF` cố định | Quét kích thước buffer cùng lúc với P2 — hai thứ tương tác với nhau |
| `client/` | `client/windows/cpp/gpu/GpuSelect.cpp` | Chọn GPU | Trên máy hai GPU, chọn sai là cả pipeline zero-copy sụp. Kiểm tra tiêu chí chọn hiện tại, không cần đua |

# Tier C — để yên

Những thành phần này chỉ có đúng và sai, không có "phương án triển khai" để so. Chúng đã
được test và được bảy target libFuzzer quét hằng đêm. Bọc interface vào đây là thêm gián
tiếp mà không đổi lấy gì.

| Nhóm | Vì sao không có gì để so |
| --- | --- |
| `core/protocol/` — Wire, RecordStream, ByteOrder | Định dạng trên dây. Chỉ có phân tích đúng hoặc sai |
| `core/terminal/` — VtParser, Screen, Palette, Snapshot, Repaint, KeyEncoder, ScrollAnchor | Bám chuẩn VT. Lệch chuẩn là bug, không phải lựa chọn |
| `core/input/` — KeyMap, ScancodeTable, Set1Scancodes, VirtualKeys, PointerMap, Hotkeys | Bảng ánh xạ. Đúng hoặc sai, không có thuật toán |
| `core/transfer/` — Crc32, SafeName | CRC32 là CRC32. SafeName là bề mặt bảo mật, đã có fuzz |
| `core/media/` — AnnexB, H264Sps, BitWriter | Phân tích bitstream theo chuẩn |
| `core/net/` — Ipv4, BindAddress, PairedDevices, TrustStore | Phân tích địa chỉ và lưu trạng thái tin cậy |
| `core/ui/` · `core/cli/` · `core/diag/` | Định dạng chuỗi hiển thị |
| `platform/auth/` · `system/AuthProof` · `system/HostIdentity` | Mật mã và bắt tay. Ở đây "phương án thay thế" là rủi ro bảo mật, không phải cơ hội tối ưu |
| `platform/system/` — Clock, Random, FileStore, Pty, KeepAwake, Autostart… | Bọc mỏng quanh lời gọi OS. Một cách đúng cho mỗi OS |
| `platform/ffi/` | Lớp đệm sang Kotlin và Swift. Chỉ là chuyển đổi kiểu dữ liệu |

---

# Hình hài của harness

Điểm mấu chốt của việc dựng contract không phải là để hoán đổi lúc chạy, mà là để **mọi bản
triển khai đi qua đúng một bộ test hành vi**. Một contract, một bộ test tham số hoá, N bản
impl. Bản nào sai hành vi thì trượt trước khi kịp được đo tốc độ.

Dự án đã có sẵn tiền lệ để bắt chước: `core/media/VideoContract.h` dùng concept C++20 thay
cho lớp cơ sở ảo, và `IVideoEncoder.h` tự ràng buộc bằng `static_assert`. Với thứ nằm hoàn
toàn trong `core/` như FEC thì lớp cơ sở ảo tiện hơn vì cần chọn lúc chạy; với thứ trải sang
`client/` thì theo lối concept sẵn có.

```cpp
class IFecScheme {
public:
    virtual ~IFecScheme() = default;
    virtual void Encode(std::span<const PacketView> group, ParitySink& out) = 0;
    virtual RecoverResult Recover(PacketGroup& group) = 0;
    virtual double OverheadRatio() const = 0;
    virtual const char* Name() const = 0;
};

std::unique_ptr<IFecScheme> MakeFecScheme(std::string_view name);
```

| Việc | Ở đâu | Vì sao ở đó |
| --- | --- | --- |
| Test hành vi, chạy mọi impl | `core/tests/transport/FecTests.cpp` | File đã tồn tại — chuyển thành tham số hoá theo tên scheme, mỗi impl phải pass hết |
| Chi phí CPU từng impl | `core/perf/VideoPerf.cpp` | Harness sẵn có đo ns/op, đếm allocation và có so baseline trong CI — nhưng job `perf-compare` (`test.yml:216`) chỉ phát `::warning::`, **không chặn hồi quy**. Đừng dựa vào nó để bảo vệ bản thắng. File này còn sẵn `kDroppedPacketIndex = 3` |
| Chất lượng phục hồi | `core/tests/transport/LossGoodputTests.cpp` *(tổng quát hoá)* | Đã là một link sim tất định: hàng đợi in-flight `multimap<uint64_t, Datagram>`, độ trễ một chiều, nhịp frame 16'667 µs, độ trễ xin keyframe — và nó chấm đúng hàm mục tiêu của A1 (`longestStallUs`, goodput, số lần mất). Chỉ cần thay mô hình loss. Dựng `core/bench/` riêng chỉ khi phần quét xuất CSV không nhét vừa một test binary |
| Đo ở tầng `platform/` (P1, P2) | `platform/perf/` | Đã có `QuicPerf.cpp` và `PerfMain.cpp`. Đây là chỗ đo QUIC và socket, vì `core/` không được đụng OS |
| Đo backend encoder (C1) | Kịch bản chạy tay trên máy thật | Cần GPU thật, không tự động hoá trong CI được. Ghi kết quả thành bảng tra trong repo |
| Kết luận sau mỗi bake-off | `docs/ARCHITECTURE.md` §Decisions worth remembering | Đúng chỗ mà `CLAUDE.md` quy định cho tri thức không được phép mất. Ghi cả khi kết luận là "giữ nguyên" — kèm ba bản dịch |

**Mô hình mất gói phải là Gilbert-Elliott**, không phải uniform random. Markov hai trạng thái
good/bad là thứ duy nhất tách được các độ sâu interleave khỏi nhau và tách chúng khỏi
Reed-Solomon. Chạy uniform random song song làm đối chứng — nếu hai mô hình cho cùng thứ hạng
thì burst đã bị mô phỏng sai.

Sim phải **seeded và tất định**: không mạng, không GPU, chạy được trong CI như một cổng chống
hồi quy — đúng ràng buộc mà `CLAUDE.md` đặt ra cho `core/`.

# Có nên cho người dùng chọn thuật toán?

Phần lớn là **không**, và đây là chỗ đáng tiết kiệm công nhất.

Người dùng không có cách nào đánh giá giữa XOR và Reed-Solomon; đưa lựa chọn đó vào Settings
là đẩy một quyết định kỹ thuật cho người không có dữ liệu để quyết. Tệ hơn, mỗi dropdown nhân
ma trận test lên, mà ASan, TSan và fuzz thì phải quét từng nhánh.

Quan trọng hơn cả: **lựa chọn đúng phụ thuộc vào RTT và loss đo được — thứ code nhìn thấy còn
người dùng thì không.** Nên hình hài đúng là tự thích ứng, không phải một menu.

| Hình thức | Dùng cho | Kết luận |
| --- | --- | --- |
| Tự thích ứng theo số đo | FEC, congestion control, jitter buffer, pacing | **Mặc định.** Code biết RTT và loss, người dùng thì không |
| Dò khả năng rồi thương lượng | Codec, chroma, backend encoder | **Mặc định.** Máy biết mình có phần cứng gì, người dùng không cần biết |
| Cờ CLI / biến môi trường | `deskhub-cli --fec=rs` khi đo và khi gỡ lỗi (`client/cli`) | **Bắt buộc có.** Không tài liệu hoá cho người dùng cuối, nhưng đây là đường duy nhất chạm tới các bản giữ lại, nên nó là điều kiện cần để việc giữ chúng có nghĩa |
| Dropdown trong Settings | Chọn thuật toán | **Tránh.** Đẩy quyết định kỹ thuật sang người không đủ dữ liệu để quyết |
| Dropdown trong Settings | Sở thích thật: thiên về độ trễ hay chất lượng, mức delay audio | **Đã đúng.** Đây là khẩu vị người dùng, không phải thuật toán. Giữ nguyên chỗ đang có |

# Giữ bản thua làm tham chiếu, chỉ chạy bản thắng

Cuộc đua sinh ra dữ liệu, mà dữ liệu đó mất giá trị nếu code sinh ra nó không còn chạy lại
được. Một bảng số trong `docs/ARCHITECTURE.md` mà không có cách tái lập chỉ là một lời khẳng
định. Nên **mọi impl đều ở lại trong cây nguồn sau bake-off** — nhưng chỉ bản thắng được nối
vào đường chạy thật.

Cái giá của việc giữ là có thật, và phải trả bằng cách khoanh vùng chứ không phải bằng cách
phớt lờ: mỗi impl là thêm một nhánh cho ASan, TSan và fuzz phải quét, cộng một thứ phải sửa
mỗi lần contract đổi. Ranh giới dưới đây giữ cho cái giá đó không nhân lên:

| | Bản thắng | Bản thua giữ lại |
| --- | --- | --- |
| Chọn lúc chạy trong app | Có, và là đường duy nhất | Không — không reachable từ production |
| Chọn qua `deskhub-cli --fec=…` | Có | **Có. Đây là lý do giữ chúng** |
| Test hành vi tham số hoá | Bắt buộc pass | Bắt buộc pass — đây là thứ giữ chúng khỏi mục |
| Workload trong `core/perf` | Có | Có |
| Ma trận fuzz và sanitizer của CI | Đầy đủ | Không. Chỉ chạy khi chính file của nó đổi |

Đổi lại, mỗi bản giữ lại phải **kèm số đo của chính nó** trong `docs/ARCHITECTURE.md` — giữ
code mà không giữ lý do nó thua thì lần sau vẫn phải đua lại từ đầu.

Chỉ xoá khi một bản không còn pass test hành vi và không ai chịu sửa: lúc đó nó đã hết là
tham chiếu, chỉ còn là nợ.

`client/` vốn đã theo đúng luật này vì một lý do khác: nhiều backend encoder **phải** cùng tồn
tại vì phần cứng người dùng khác nhau. Ở đó việc cần làm không phải chọn một lần cho tất cả,
mà là chọn cho đúng trên từng máy — đó chính là C1.

---

# Việc cần làm, theo thứ tự

## Tiếp tục từ đây (đặt lại ngày 03/09/2026, phiên tối — trên máy macOS)

Phiên tối chạy trên macOS, nên **không có nhánh Windows nào được biên dịch ở đây**.
`make test`, `make test-all`, `make lint` và `make lint-tidy` đều xanh. Chưa commit.

**Đã xong ở phiên tối:**

- **Tài liệu cho A4 và P2** — ba bullet mới ở `docs/ARCHITECTURE.md` §Decisions worth
  remembering (hai cho A4, một cho P2/USO) cùng ba bản dịch, cộng hai câu cũ nay đã sai
  (bullet A4 nói "chưa backend nào khai báo", bullet C1 nói "chưa có gì tiêu thụ
  `invalidateBeforeFrame`") được sửa lại. Bullet thứ tư là của C2 ở dưới
- **C2 — nửa `core/` của phép đo độ trễ capture**, đã biên dịch và test tại chỗ:
  `SourceDiag::NoteCapture(frameTimestampUs, nowUs)` cộng `capUs` (percentile) và
  `capRepeat` (đếm), in ra `cap_us_p50` / `cap_us_p99` / `cap_repeat` sau
  `ShareDiagCaps::captureLatency`. Khung được trao lại lần nữa **chỉ được đếm, không đo lại**
  — tuổi của nó tính từ lần capture gốc nên sẽ thổi phồng đuôi. 9 check mới trong
  `core/tests/diag/DiagTests.cpp`. Phía Windows đã nối (`onFrame` gọi `NoteCapture`, caps bật
  cờ thứ tư) nhưng **chưa qua compiler ở đây** — cùng loại rủi ro như A4/P2 hôm chiều

**Hai việc xong buổi chiều, cả hai vẫn mới chỉ đúng ở mức "biên dịch và test đơn vị":**

- **A4 — nhánh thực thi trên Windows** (chi tiết trong Phase 7 bên dưới)
- **P2 — gộp gói phía gửi trên Windows bằng USO** (chi tiết trong Phase 6 bên dưới)

**Việc đầu tiên của ngày mai, trước khi viết thêm bất kỳ dòng nào:** chạy A4 trên một link
thật và đọc log. Cả đường LTR lẫn đường USO **chưa từng gặp một gói mất thật hay một NIC
thật nào** — đúng loại "cấu hình chưa ai đo" mà A1 và C1 đã dính một lần mỗi cái.

    deskhub-cli share --bind 192.168.1.3 --encoder nvenc

Bằng chứng phải thấy trong log host khi viewer báo mất gói:

| Dòng | Nghĩa |
| --- | --- |
| `[NVENC] Initialized: … ltr_slots=4` | LTR bật được thật, không bị driver từ chối |
| `[Host][…] recovery=invalidate_ref after losing frame N` | chính sách chọn vá thay vì IDR |
| `[NVENC] recovery: next picture references long-term frame M only.` | encoder **thực sự** thi hành |
| `evt=sum … idr=0` trong cùng cửa sổ | và cú vọt bitrate của IDR biến mất |

Nếu thấy `The encoder kept no reference older than frame N` thì nhánh LTR đang rỗng — đọc
`ltr_slots` ở dòng Initialized trước khi nghi ngờ chỗ khác.

Sau đó, theo thứ tự:

1. **Dựng lại trên Windows trước mọi thứ khác.** Ba thay đổi đang chờ compiler ở đó:
   A4, P2/USO và nay cả nhánh `NoteCapture` trong `client/windows/cpp/SharingHost.cpp`.
2. **Đo P2 trên Windows**: `platform_perf` trước/sau, đối chiếu với bảng loopback Linux ở
   Phase 6. Chưa có một con số Windows nào cả.
3. **Đọc `cap_us_p50` / `cap_us_p99` / `cap_repeat` thật** — đó chính là câu trả lời của C2
   cho "WGC tốn bao nhiêu độ trễ". Chỉ khi số đó xấu mới đáng viết backend Duplication.
4. **Phase 4 còn dư** — bảng tra `EncoderFactory`, và kịch bản clip cố định.
5. **C3 4:4:4** — có một phát hiện chặn, xem Phase 7.

### Phase 0 — Sửa baseline, rồi đo thực tế trước khi mô phỏng

*Chặn mọi thứ phía sau. Hai việc đầu là code, phần còn lại là đo.*

- [x] Cắt gói parity xuống theo gói dữ liệu lớn nhất trong group **(A1)** — không sửa thì mọi phép đo sau đều tính trên overhead sai
- [x] Đếm datagram bị `SendDatagram` từ chối, phơi ra dòng diag cạnh `e2e_ms` **(P1, chẩn đoán)** — `QuicSendStats` đã có sẵn chỗ
- [ ] Bắt loss và jitter thật trên ba link: WiFi nhà, WiFi quán, Tailscale qua WAN
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
- [ ] Rút ra tham số Gilbert-Elliott từ phần loss thật: tỉ lệ mất, độ dài burst trung bình, phân bố
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

- [ ] Chạy A/B `RawUdp` so với `QuicDatagram` trên cùng một link — không phải viết code, và nó đo thẳng phần đóng góp của CC quiche
  — **núm vặn đã kiểm chứng xong (03/09), phép đo thì chưa.** Xem "Kiểm núm vặn trước khi đi đo"
  và "Vì sao A/B phải chạy trên link chật" bên dưới
- [ ] Quyết kiến trúc: quiche làm chủ, AIMD làm chủ, hay chia vai rõ ràng
  — chặn bởi mục trên; không có số thì mọi lựa chọn đều là suy đoán
- [x] Đặt cấu hình tường minh trong `MakeConfig` để lựa chọn đó nằm trong code chứ không nằm trong mặc định CUBIC của thư viện
  — `QuicSettings::congestionControl` mặc định `Cubic`, gọi `quiche_config_set_cc_algorithm`
  tường minh. **Không đổi hành vi** (đã xác nhận mặc định của quiche là CUBIC tại
  `third_party/quiche/src/quiche/src/lib.rs:654`); giá trị của nó là ghim baseline lại để một
  lần nâng quiche không âm thầm đổi phép đo. Reno và BBR2 cũng chọn được, cho phép quét
- [ ] Ghi quyết định vào `docs/ARCHITECTURE.md` §Decisions worth remembering kèm ba bản dịch

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
- [ ] Xác nhận lại trên link thật bằng bài camera 240 fps qua `netem`, kiểm tra sim không nói dối
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
- [ ] Dựng kịch bản đo: cùng clip, cùng bitrate, VMAF + CPU/GPU — **chưa**; latency p99 thì đã có
- [ ] Chạy trên từng backend: NVENC, Media Foundation, VA-API, VideoToolbox, MediaCodec
  — **mới có NVENC và MF trên máy này** (bảng dưới), ba backend còn lại cần máy khác
- [x] Ghi lại backend nào hỗ trợ LTR — kết quả này quyết định A4 khả thi tới đâu
  — **cả hai backend Windows đều có**, đọc từ driver chứ không suy đoán: NVENC qua
  `nvEncGetEncodeCaps` cho `max_ltr_frames=8 ref_pic_invalidation=1 intra_refresh=1` trên
  RTX 5070 Ti; Media Foundation trả `IsSupported` = `S_OK` cho cả ba thuộc tính LTR lẫn
  `GradualIntraRefresh`. **Caps chỉ ghi log, chưa đưa vào `RecoveryPolicy`** — chưa có gì tiêu thụ
  `invalidateBeforeFrame` hay `wantIntraRefresh`, nên khai khả năng lúc này sẽ biến phục hồi mất
  gói thành vô tác dụng. Đó là phần thực thi của A4, không phải của C1
- [ ] Thay tiêu chí chọn trong `EncoderFactory` từ "init được trước" sang bảng tra dựng từ số đo
  — chưa đổi, và số đo đầu tiên nói thứ tự hiện tại (NVENC trước) là **đúng trên máy này**.
  Ghi thêm 03/09: bảng tra nên khoá theo `GpuChoice::vendor` (đã có sẵn, `GpuSelect.h`), vì
  hôm nay trên máy Intel hay AMD thì `CreateEncoder` vẫn nạp `nvEncodeAPI64.dll` trước rồi
  mới chịu thất bại — thứ tự đúng trên máy này đang phải trả giá trên mọi máy khác

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

- [ ] P2 — `sendmmsg`/`recvmmsg`, rồi GSO/GRO trên Linux; đo ngưỡng bitrate mà CPU thành nút cổ chai
  — **xong phần gộp syscall; còn GRO, RIO và phép đo ngưỡng bitrate.** `UdpSocket::SendBatch` nay
  thử `UDP_SEGMENT` (GSO) trước rồi mới rơi về `sendmmsg`; `UdpSocket::RecvBatch` là `recvmmsg`
  với `MSG_WAITFORONE` trên Linux và vòng `RecvFrom` ở mọi OS khác; `QuicEndpoint::Poll` đọc theo
  lô 16 gói thay vì từng gói một. **Nhánh Linux nay đã được biên dịch, chạy test và strace trên chính máy này** —
  ngược với ghi chú cũ; nhánh Windows là nhánh chưa qua compiler ở đây.
  **Chưa làm: GRO và RIO** — GRO gộp nhiều datagram vào một buffer, tức phá vỡ giao kèo
  "một slot một datagram" mà `RecvBatch` dựng trên; đó là một thay đổi API riêng, không phải
  nửa sau của mục này. Xem kết quả đo ở dưới

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

  ⚠️ **Chưa đo trên Windows.** Bảng loopback ở dưới là số Linux. Chưa chạy `platform_perf`
  trước/sau ở đây, nên đừng trích bảng đó cho Windows. Tài liệu **đã xong** (03/09, phiên
  tối): một bullet ở `docs/ARCHITECTURE.md` §Decisions worth remembering cùng ba bản dịch,
  và nó nói thẳng rằng bảng loopback kia là số Linux.

  **Vẫn chưa làm: URO** (`UDP_RECV_MAX_COALESCED_SIZE`) — chặn bởi đúng cái giao kèo
  "một slot một datagram" đã chặn GRO. Hai cái là **một** thay đổi API, làm cùng nhau
- [x] A6 — pacing thích ứng và khớp vsync, đo judder
  — quét 24 điểm (lead 8…66 ms × wobble 0/5/20 ms, có và không khớp vsync) xuất CSV.
  **Phát hiện: trục "độ trễ cộng thêm" của A6 không có trade nào cả.** Không khớp vsync thì
  phase spread ~6000 µs trên chu kỳ 6944 µs ở *mọi* mức lead — mua lead gấp tám lần không thu
  hẹp được một micro giây. Khớp vsync đưa spread về **0** và không tốn độ trễ nào
- [ ] C2 — đo độ trễ capture của WGC, đối chiếu với DXGI Desktop Duplication
  — **bộ đếm đã viết xong, số đo thì chưa có (03/09/2026, phiên tối).**
  `SourceDiag::NoteCapture(frameTimestampUs, nowUs)` nằm trong `core/`: nó cộng tuổi khung vào
  percentile `capUs` và đếm khung được trao lại lần nữa vào `capRepeat`. Một khung lặp **chỉ
  được đếm**, không đo lại — tuổi của nó tính từ lần capture gốc nên sẽ thổi phồng đuôi. Timestamp
  bằng 0 và khung "đến từ tương lai" đều bị bỏ, không kẹp về 0; tuổi vượt 32 bit thì bão hoà.
  `evt=sum` in `cap_us_p50` / `cap_us_p99` / `cap_repeat` sau `ShareDiagCaps::captureLatency`,
  nên bốn client kia không in cột rỗng và nối vào bằng đúng một dòng mỗi cái khi tới lượt. 9 check
  ở `core/tests/diag/DiagTests.cpp`. Phía Windows: `onFrame` gọi `NoteCapture` ngay sau
  `captured.fetch_add`, và `SourcePipeline` bật cờ thứ tư trong caps — **chưa qua compiler**.
  **Còn lại:** chạy trên Windows và đọc ba con số đó. Chỉ khi chúng xấu mới đáng viết backend
  Duplication, và khi đó theo đúng khuôn `--encoder`: một cờ `--capture wgc|dxgi` gọi tên, gọi tên
  cái không khởi động được thì **dừng**.

  Ba điều đã khảo sát trước khi viết, giữ lại vì vẫn là lý do của thiết kế trên:

  - `client/windows/cpp/capture/ScreenCapture.cpp` **không có một bộ đếm độ trễ nào** — con
    số C2 cần chưa tồn tại, không phải chưa được in
  - `fi.meta.timestampUs` (chính là `SystemRelativeTime` của WGC) **chưa có ai đọc** ở phía
    host Windows: `SharingHost` chỉ dùng `width`/`height`, còn `Encode` được truyền
    `NowUs()` mới tinh. Nên `enc_lat_ms` hôm nay đo độ trễ **encode**, không phải
    capture→texture — đừng đọc nhầm nó thành số của C2
  - hai đồng hồ khớp nhau sẵn: `SystemRelativeTime` là QPC (đơn vị 100 ns) và `NowUs()` trên
    Windows cũng là QPC, nên hiệu hai số là dùng được ngay, không cần quy đổi epoch

  Đó cũng là lý do bước đầu tiên chỉ là ba bộ đếm chứ không phải một backend thứ hai: riêng
  chúng đã trả lời được "trả bao nhiêu độ trễ cho sự tiện lợi của WGC"
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

- [ ] A4 — chính sách phục hồi ở `core/`, thực thi ở từng backend encoder mà Phase 4 xác nhận là có hỗ trợ
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
- [ ] C3 — thêm 4:4:4 cho use case đọc chữ, giữ 4:2:0 làm baseline phổ quát
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
  — `scripts/bake-off-csv.sh` chạy `core_tests` rồi tách ra ba bảng: `fec-sweep.csv` (179
  dòng), `audio-delay.csv` (21), `pacer-judder.csv` (24). Mọi dòng sinh từ sim có seed, tất
  định, không mạng không GPU — chạy lại cho ra đúng từng byte. Phần "chỗ Deskhub thua" đã có
  thật trong dữ liệu: A3 thích ứng thua target cố định dưới jitter
- [ ] Viết bài từ dữ liệu bake-off: "tôi thử N sơ đồ FEC dưới burst loss, đây là số liệu"
