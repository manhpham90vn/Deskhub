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

### Phase 0 — Sửa baseline, rồi đo thực tế trước khi mô phỏng

*Chặn mọi thứ phía sau. Hai việc đầu là code, phần còn lại là đo.*

- [ ] Cắt gói parity xuống theo gói dữ liệu lớn nhất trong group **(A1)** — không sửa thì mọi phép đo sau đều tính trên overhead sai
- [ ] Đếm datagram bị `SendDatagram` từ chối, phơi ra dòng diag cạnh `e2e_ms` **(P1, chẩn đoán)** — `QuicSendStats` đã có sẵn chỗ
- [ ] Bắt loss và jitter thật trên ba link: WiFi nhà, WiFi quán, Tailscale qua WAN
- [ ] Đối chiếu: bao nhiêu phần "loss" là của mạng, bao nhiêu là tự mình bỏ ở hàng đợi
- [ ] Rút ra tham số Gilbert-Elliott từ phần loss thật: tỉ lệ mất, độ dài burst trung bình, phân bố
- [ ] Chốt hàm mục tiêu và ngưỡng đạt **trước khi** nhìn thấy bất kỳ con số nào

### Phase 1 — Làm rõ quan hệ hai tầng điều khiển tắc nghẽn

*P1. Phải xong trước A1 và A2, nếu không cả hai đều tối ưu cho mô hình sai.*

- [ ] Chạy A/B `RawUdp` so với `QuicDatagram` trên cùng một link — không phải viết code, và nó đo thẳng phần đóng góp của CC quiche
- [ ] Quyết kiến trúc: quiche làm chủ, AIMD làm chủ, hay chia vai rõ ràng
- [ ] Đặt cấu hình tường minh trong `MakeConfig` để lựa chọn đó nằm trong code chứ không nằm trong mặc định CUBIC của thư viện
- [ ] Ghi quyết định vào `docs/ARCHITECTURE.md` §Decisions worth remembering kèm ba bản dịch

### Phase 2 — Dựng harness trên một thành phần làm mẫu

*A1 · FEC.*

- [ ] Tách `IFecScheme` ra khỏi `Packetizer`, giữ XOR hiện tại làm impl đầu tiên — không đổi hành vi
- [ ] Tách độ sâu interleave khỏi `numGroups` để nó thành tham số quét được, mặc định vẫn là `ceil(count/8)`
- [ ] Chuyển `core/tests/transport/FecTests.cpp` thành tham số hoá, chạy qua mọi impl đăng ký
- [ ] Đổi mô hình loss của `LossGoodputTests.cpp` từ "bỏ 6 gói đuôi mỗi frame thứ 50" sang Gilbert-Elliott có seed, xuất CSV
- [ ] Thêm Workload đo CPU từng scheme vào `core/perf/VideoPerf.cpp`
- [ ] Nối `deskhub-cli --fec=…` vào factory — điều kiện cần để giữ được các bản thua

### Phase 3 — Chạy đua FEC

*Quét tham số trước, viết impl mới sau.*

- [ ] **Quét trước khi viết:** depth × group size × tỉ lệ parity trên chính bản XOR đang có. Reed-Solomon chỉ đáng viết nếu lần quét này chạm trần dưới ngưỡng đạt
- [ ] Viết các impl còn thiếu theo đúng thứ tự cần thiết: Reed-Solomon, RS thích ứng, NACK-only
- [ ] Quét scheme × tỉ lệ parity × group size × độ sâu interleave × loss × độ dài burst × RTT
- [ ] Chốt bản thắng theo tiêu chí đã đặt ở Phase 0 — không sửa tiêu chí sau khi thấy số
- [ ] Xác nhận lại trên link thật bằng bài camera 240 fps qua `netem`, kiểm tra sim không nói dối
- [ ] Nối bản thắng vào đường chạy thật; các bản khác ở lại sau `--fec=…` và ra khỏi ma trận fuzz/sanitizer
- [ ] Ghi kết luận **kèm số đo của từng bản** vào `docs/ARCHITECTURE.md` kèm ba bản dịch

### Phase 4 — Bake-off backend encoder

*C1. Rẻ nhất, vì interface và factory đã tồn tại sẵn.*

- [ ] Dựng kịch bản đo: cùng clip, cùng bitrate, VMAF + encode latency p99 + CPU/GPU
- [ ] Chạy trên từng backend: NVENC, Media Foundation, VA-API, VideoToolbox, MediaCodec
- [ ] Ghi lại backend nào hỗ trợ LTR — kết quả này quyết định A4 khả thi tới đâu
- [ ] Thay tiêu chí chọn trong `EncoderFactory` từ "init được trước" sang bảng tra dựng từ số đo

### Phase 5 — Lặp lại trên những thành phần dễ nhất

*A2 → A3 → A5.*

- [ ] A2 — thêm `virtual` vào `Update()`, viết impl delay-gradient và SCReAM
- [ ] A2 — dùng ca overshoot đã ghi trong `docs/ARCHITECTURE.md` làm test hồi quy
- [ ] A3 — chấm điểm bằng chính struct `Stats` đã có, vẽ đường cong đánh đổi độ trễ
- [ ] A5 — so ba bản ước lượng offset, lấy về một con số latency công bố được

### Phase 6 — Hiệu năng đường truyền và nhịp hình

*P2, A6, C2.*

- [ ] P2 — `sendmmsg`/`recvmmsg`, rồi GSO/GRO trên Linux; đo ngưỡng bitrate mà CPU thành nút cổ chai
- [ ] A6 — pacing thích ứng và khớp vsync, đo judder
- [ ] C2 — đo độ trễ capture của WGC, đối chiếu với DXGI Desktop Duplication
- [ ] Tier B — thêm jitter vào backoff của `LinkRecovery`; nhớ đây là đổi chữ ký `ReconnectDelayUs` cộng mọi caller, không phải sửa tại chỗ

### Phase 7 — Những món trải khắp ba lớp

*A4, C3. Đắt nhất, làm sau cùng.*

- [ ] A4 — chính sách phục hồi ở `core/`, thực thi ở từng backend encoder mà Phase 4 xác nhận là có hỗ trợ
- [ ] C3 — dựng bảng khả năng codec và cơ chế thương lượng khi kết nối
- [ ] C3 — thêm 4:4:4 cho use case đọc chữ, giữ 4:2:0 làm baseline phổ quát

### Phase 8 — Công bố

- [ ] Đăng CSV thô, script và cách chạy lại — kèm cả những chỗ Deskhub thua
- [ ] Viết bài từ dữ liệu bake-off: "tôi thử N sơ đồ FEC dưới burst loss, đây là số liệu"
