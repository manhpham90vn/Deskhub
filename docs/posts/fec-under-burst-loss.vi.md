[English](fec-under-burst-loss.md) · **Tiếng Việt** · [中文](fec-under-burst-loss.zh.md) · [日本語](fec-under-burst-loss.ja.md)

> Bản tiếng Anh là bản gốc có giá trị pháp lý; bản dịch này đi sau nó.

# Tôi cho năm cách sống sót qua mất gói chạy đua. Không cách nào đạt.

*Deskhub là một công cụ điều khiển máy tính từ xa. Đây là bài viết lại cuộc bake-off A1 của nó:
sửa lỗi tiến (FEC) dưới burst loss. Kết luận chính là một kết quả âm tính, nên phần lớn số liệu
dưới đây là những chỗ mặc định của chính tôi thua.*

---

## Đọc phần này trước mọi bảng

Đây là số của **mô phỏng**. Mọi dòng đến từ một mô hình tất định có seed, chạy bên trong
`core_tests`, không card mạng và không GPU nào ở gần. Cái đó mua được tính tái lập — hai lần chạy
trên hai máy cho ra CSV giống nhau từng byte — và trả giá bằng tính hiện thực, theo đúng bốn cách
mà bạn phải mang theo vào mọi bảng bên dưới:

| Mô hình không có… | Nghĩa là |
| --- | --- |
| **jitter** | độ trễ một chiều là hằng số 20 ms (2 ms cho những dòng RTT ngắn). Mọi gói không bị bỏ đều tới đúng giờ, nên vòng đi-về của một NACK đúng bằng `2 × delay` và không bao giờ tệ hơn. Link thật kéo dài cái đuôi ấy, và NACK chính là sơ đồ chịu thiệt khi điều đó xảy ra. |
| **đảo thứ tự** | gói được giao đúng thứ tự gửi. Nhánh overtaken/reorder của reassembler vì thế không bao giờ bị đụng tới, và sơ đồ nào lẽ ra bị đảo thứ tự làm rối thì ở đây trông sạch hơn thực tế. |
| **tắc nghẽn** | gói chỉ bị bỏ bởi mô hình loss. Không có hàng đợi, nên không có cú bỏ nào do chính lưu lượng sửa lỗi gây ra. Đây là điểm lớn nhất: overhead parity trong các bảng dưới đây là **miễn phí** trong mô phỏng và **không miễn phí** trên link thật. |
| **video thật** | một frame là byte `Rnd()` — 8 gói cho delta frame, 40 cho IDR, luôn luôn. Không cắt cảnh, không phân phối kích thước, không hư hại phụ thuộc nội dung. "Frame hỏng" nghĩa là "ít nhất một gói không phải parity của nó bị bỏ", và không nói gì về việc nó xấu tới đâu. |

Và một điều nữa, là lỗi quy trình chứ không phải lỗi mô hình:

> **Ngưỡng đạt/không đạt trong bài này được đặt *sau* khi một phần dữ liệu đã hiện ra.**
> Luật của cuộc bake-off này là chốt hàm mục tiêu và chốt ngưỡng trước khi nhìn thấy bất kỳ con số
> nào. Luật ấy giữ được với hàm mục tiêu và vỡ với cái ngưỡng: lần quét tham số trước đó và các
> phép đo trên phần cứng đã chạy xong khi ngưỡng được viết ra. Hãy đọc chúng như ngưỡng hậu
> nghiệm, không phải như dự đoán. Kết quả âm tính bên dưới sống sót qua chuyện đó — không gì đạt
> nổi một cái bar mà tôi đặt *khi đã biết dữ liệu* — nhưng một kết quả dương tính thì không.

## Câu hỏi

Deskhub gửi video bằng datagram UDP. Khi một gói mất, reassembler phía viewer không dựng xong được
frame, tham chiếu của frame ấy biến mất, và viewer xin host một keyframe. Một keyframe là 40 gói
trong khi delta frame là 8, nên một link đang mất gói bị phạt hai lần: một lần bởi chính cú mất, và
một lần nữa bởi cơn bão IDR mà cú mất ấy kích hoạt.

Vậy hàm mục tiêu là **số lần xin IDR mỗi phút**, không phải tỉ lệ cứu được. Tỉ lệ cứu được là thứ
người ta hay đem ra cãi nhau; IDR/phút mới là thứ người dùng cảm thấy.

Cái bar, viết ra sau khi đã thấy một phần dữ liệu:

- **≤ 2 IDR/phút** ở điểm vận hành đã đo được thật trên WiFi nhà (0,1% loss, burst 1)
- **≤ 20 IDR/phút** ở một điểm cố tình khắc nghiệt (5% loss, burst 4)
- và bản thắng được phép tốn nhiều nhất **3×** CPU của bộ mã hoá XOR, trừ khi nó cứu được **≥ 2×**
  số frame

Năm ứng viên: XOR với độ sâu interleave suy ra từ số group (bản đang ship), XOR với depth đặt độc
lập, Reed–Solomon trên GF(256), Reed–Solomon với số hàng parity chạy theo loss đo được, và
NACK-only phát lại. `fec-sweep.csv` bắt chéo chúng với số hàng parity (1/2/3), độ sâu interleave
(suy ra/4/8), mô hình loss (đều / Gilbert–Elliott), tỉ lệ loss (0,1 / 1 / 5%) và RTT (4 / 40 ms) —
180 dòng. `nack-hybrid.csv` tách riêng câu hỏi chế độ sửa lỗi, chạy qua RTT 4 / 20 / 40 / 80 ms và
một cửa sổ giữ frame phía viewer — 72 dòng.

## Kết quả 1: ở 5% loss và burst 4, không gì đạt

Bar là 20 IDR/phút. Bản quét có 36 dòng ở điểm vận hành đó. Dòng tốt nhất là 36 IDR/phút — và nó
mua được con số ấy bằng cách gửi **143 gói parity cho mỗi 100 gói dữ liệu**.

| scheme | parity rows | depth | overhead | rescued | **IDR/min** |
| --- | ---: | ---: | ---: | ---: | ---: |
| xor (bản ship) | 1 | derived | 12.5% | 33.87% | **171.00** |
| xor | 1 | 8 | 90.9% | 78.69% | **81.00** |
| rs | 2 | 8 | 188.7% | 86.67% | **45.00** |
| rs | 3 | 4 | 142.9% | 90.57% | **36.00** ← tốt nhất trong bản quét |

> **Một cảnh báo phải nằm cạnh bảng này, không phải ở cuối bài:** cột overhead là lý do đây là kết
> quả âm tính chứ không phải danh sách mua hàng. Mô phỏng không có tắc nghẽn, nên 142,9% overhead ở
> đó không tốn gì. Trên một link 20 Mbps nó có nghĩa là khoảng 8 Mbps video và 12 Mbps parity —
> điều đó sẽ đẩy bộ điều khiển bitrate xuống, làm frame nhỏ lại, và thay đổi mọi con số khác trong
> chính dòng ấy. Mô hình không cho thấy được điều đó. Nên dòng cuối cùng không phải "bản thắng"; nó
> là điểm mà mô hình hết khả năng trả lời.

Tìm những dòng đó:

```sh
awk -F, 'NR==1 || ($6=="5.00" && $7=="4.0")' docs/data/bake-off/fec-sweep.csv
```

## Kết quả 2: Reed–Solomon và XOR cho ra số giống hệt nhau trên cả 40 điểm chung

`fec-sweep.csv` giữ 40 điểm tham số mà ở đó một dòng `xor` và một dòng `rs` chỉ khác nhau đúng cái
tên scheme. Trên cả 40, **từng cột một trong 29 cột còn lại đều bằng nhau tới từng ký tự**. Không
phải gần bằng — bằng.

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

Đây không phải bug, và nó hết bất ngờ ngay khoảnh khắc bạn phát biểu nó ra: **cả 40 điểm ấy đều có
`parity_rows=1`, mà một mã Reed–Solomon với một hàng parity *chính là* XOR.** Ma trận Cauchy suy
biến thành một hàng toàn số một. Tôi đã viết một trường GF(256), một ma trận Cauchy và một bộ giải
bằng khử Gauss, và ở đúng thiết lập parity đang ship, tất cả những thứ đó tính lại đúng cái byte
parity mà một vòng lặp XOR hai dòng đã tạo ra.

Cái nó không tái tạo được là chi phí. Trên máy này, dòng `video/fec-encode-rs` của `core_perf` tốn
**143 µs/frame** so với **22,7 µs/frame** của `video/fec-encode-xor` — 6,3×. *(Cặp đó là một phép
đo thời gian, không phải mô phỏng: nó đến từ `core_perf` trên một máy để bàn Intel và không tái lập
từng byte theo cách các file CSV làm được. Đây là con số duy nhất trong bài không phải một dòng
CSV, và nó được gọi tên ngay tại đây chứ không trộn vào bảng.)*

Vậy Reed–Solomon bắt đầu đáng đồng tiền từ 2 hàng parity, còn ở 1 hàng parity nó là một khoản thuế
6,3× cho một kết quả giống nhau tới từng bit. Mặc định đang ship ở lại với XOR.

## Kết quả 3: độ sâu interleave không phải cái cần gạt rẻ tiền, và tôi đã viết rằng nó rẻ

Bản nháp đầu của phân tích này gọi độ sâu interleave là "cái cần gạt rẻ nhất trong lưới", với lý do
trải một frame ra nhiều nhóm FEC hơn thì không tốn thêm CPU. Điều đó đúng, và điều đó không liên
quan.

| scheme | parity rows | depth | overhead | rescued | IDR/min |
| --- | ---: | ---: | ---: | ---: | ---: |
| xor | 1 | derived | **12.5%** | 33.87% | 171.00 |
| xor | 1 | 4 | **43.9%** | 62.90% | 117.00 |
| xor | 1 | 8 | **90.9%** | 78.69% | 81.00 |

Cùng scheme, cùng số hàng parity, cùng loss, cùng seed. Depth 8 giảm một nửa tỉ lệ IDR — và đưa
overhead từ 12,5% lên 90,9%, bởi vì một delta frame là 8 gói: ở depth 8 mỗi nhóm giữ đúng một gói,
nên mỗi gói mang parity cho chính nó. Đó không phải mã hoá, đó là gửi mọi thứ hai lần. Trên ngân
sách 20 Mbps nó nghĩa là bỏ ra một nửa bức hình để giảm một nửa số lần xin keyframe.

> **Cảnh báo:** delta frame 8 gói là một hằng số trong mô hình, không phải một phép đo. Delta frame
> thật có kích thước thay đổi, và trường hợp suy biến ở depth 8 một phần là hệ quả của cái hằng số
> ấy. Thứ sống sót qua hệ quả đó là cái hình dạng: depth tốn băng thông tỉ lệ với việc nó vượt số
> gói bao xa, và nó không bao giờ miễn phí.

```sh
awk -F, 'NR==1 || ($1=="xor" && $2=="1" && $3=="0" && $6=="5.00" && $7=="4.0" && $29=="40")' \
    docs/data/bake-off/fec-sweep.csv | sort -u
```

## Kết quả 4: chính sách bật FEC đang ship thua cả việc để FEC luôn bật — và thua cả việc không có nó

Đây là cái bảng mà mặc định của chính Deskhub là lựa chọn tệ nhất trong đó.

FEC không phải lúc nào cũng bật. Một chính sách theo dõi feedback và bật FEC khi loss báo về đủ xấu.
Ở đúng điểm vận hành mà WiFi nhà thật sự tạo ra — 0,1% loss, burst 1, trên 1800 frame — đây là
chính sách ấy đọ với ba phương án khác:

| cấu hình | FEC bật | overhead | frame cứu được | **IDR/min** |
| --- | ---: | ---: | ---: | ---: |
| không sửa lỗi gì cả | — | 0.0% | 0/16 | 32.00 |
| FEC luôn bật | 100.0% | 12.5% | 16/16 | **0.00** |
| **chính sách bật đang ship** | 30.0% | 4.1% | 6/16 | **20.00** |
| chỉ NACK, RTT 4 ms | — | **0.0%** | 15/16 | **2.00** |
| chỉ NACK, RTT 40 ms | — | 0.0% | 0/16 | 32.00 |

Bar ở điểm vận hành này là ≤ 2 IDR/phút. Cấu hình đang ship trượt nó 10×. Chính sách ấy dành 70%
thời gian ở trạng thái tắt vì loss báo về bị làm tròn trước khi ngưỡng đem ra so, và 0,1% làm tròn
thành 0.

Dòng đạt được là dòng **không có sửa lỗi tiến nào cả**: NACK-only trên link ngắn, không tốn băng
thông, cứu 15 trên 16 frame hỏng. Điều đó lật ngược một kết luận trước đó của tôi — ghi trong cùng
tài liệu làm việc ấy — rằng phát lại không đáng để viết.

> **Cảnh báo, và là một cảnh báo lớn:** dòng NACK ấy là giả định không-jitter của mô hình đang trả
> cổ tức. Ở RTT 4 ms bản phát lại luôn về kịp trước hạn của frame, bởi vì mô hình bảo đảm nó về
> đúng trong 4 ms. Dòng RTT 40 ms ngay bên dưới — cứu 0 trên 16, 32 IDR/phút — là chuyện xảy ra khi
> vòng đi-về không còn lọt vào trong hạn. Một link 4 ms thật với cái đuôi jitter nằm đâu đó giữa
> hai dòng ấy, không nằm trên dòng đẹp.

```sh
awk -F, 'NR==1 || ($6=="0.10" && $7=="1.0" && $9=="1800")' docs/data/bake-off/fec-sweep.csv
```

## Kết quả 5: chế độ sửa lỗi đọ với RTT — mỗi cái gãy ở đâu

`nack-hybrid.csv` chạy ba chế độ sửa lỗi qua bốn mức RTT và một cửa sổ giữ frame phía viewer, ở 1%
và 5% loss với burst 4. Phần thú vị không phải trường hợp tốt nhất; nó là hình dạng cú gãy của từng
chế độ.

Ở **5% loss, RTT 4 ms**, không cửa sổ giữ:

| repair | overhead | rescued | IDR/min |
| --- | ---: | ---: | ---: |
| fec-only | 12.5% | 29.51% | 180.00 |
| nack-only | **0.0%** | 57.69% | 126.00 |
| fec+nack | 12.5% | **68.42%** | 126.00 |

Ở **5% loss, RTT 80 ms**, cũng ba dòng ấy trên một link dài:

| repair | overhead | rescued | IDR/min |
| --- | ---: | ---: | ---: |
| fec-only | 12.5% | **31.25%** | 153.00 |
| nack-only | 0.0% | **7.69%** | 180.00 |
| fec+nack | 12.5% | 25.42% | 153.00 |

Tỉ lệ cứu của NACK rơi từ 57,69% xuống 7,69% khi vòng đi-về dài ra, và nó kéo bản lai xuống theo:
`fec+nack` ở RTT 80 *tệ hơn* `fec-only` ở RTT 80, bởi vì những frame mà nó tiêu ngân sách NACK vào
thì đã quá hạn. Một cửa sổ giữ frame phía viewer lấy lại được phần lớn chỗ đó — ở RTT 80 với
`hold_frames=8`, `fec+nack` quay lại 67,21% cứu được và 63,00 IDR/phút, ngang với mức IDR/phút thấp
nhất mà bất kỳ dòng 5% loss nào trong file chạm tới — nhưng giữ 8 frame hiện ra thành 116 ms trong
`longest_stall_ms`, trên một công cụ mà toàn bộ điểm bán hàng là cảm giác tức thì.

> **Cảnh báo:** `hold_frames` là cột duy nhất ở đây đánh đổi một con số tốt đo được lấy một con số
> xấu không đo được. Độ trễ thêm vào ở phía viewer hoàn toàn không nằm trong hàm mục tiêu, nên bảng
> này xếp hạng được các cửa sổ giữ và không nói được người dùng chịu nổi cửa sổ nào.

```sh
awk -F, 'NR==1 || ($4=="5.00" && $3=="0")' docs/data/bake-off/nack-hybrid.csv
```

## Cái gì được ship

Không gì cả. Mặc định vẫn ở đúng chỗ nó bắt đầu: XOR, một hàng parity, depth suy ra, bật bằng chính
sách sẵn có. Reed–Solomon, parity thích ứng và NACK ở lại sau `--fec=`, `--fec-depth`,
`--fec-parity` và `--nack` như những cờ để đo — sẵn cho vòng sau, giữ ngoài ma trận fuzz và
sanitizer để CI không trả tiền cho code không ai chạy, và không đưa ra cho người dùng cuối, những
người không có dữ liệu nào để chọn.

Ba thứ cuộc bake-off này thật sự tạo ra:

1. Một kết quả âm tính đủ mạnh để chặn một cuộc viết lại: RS ở 1 hàng parity là XOR với hoá đơn CPU
   6,3×, nên cái scheme chưa bao giờ là nút cổ chai.
2. Một bảng xếp hạng cho thứ đúng là nút cổ chai: **chính sách bật FEC** tốn 20 IDR/phút ở điểm vận
   hành đo được, nhiều hơn giá trị của bất kỳ lựa chọn scheme nào trong bản quét. Đó là thứ phải
   sửa tiếp theo.
3. Một cú lật: NACK-only từng bị gạt đi mà không đo, và trên link ngắn nó là cấu hình duy nhất
   trong file đạt bar — với overhead bằng không.

Tóm tắt trung thực là: câu hỏi tôi đặt ra để trả lời ("XOR hay Reed–Solomon?") là câu hỏi sai, và
bản quét đáng chạy chủ yếu vì nó nói ra điều đó.

## Chạy lại toàn bộ

```sh
git clone https://github.com/manhpham90vn/Deskhub
cd Deskhub
make test                  # build và chạy core_tests offline
scripts/bake-off-csv.sh    # tách các dòng [csv] ra out/bake-off/*.csv
diff out/bake-off/fec-sweep.csv docs/data/bake-off/fec-sweep.csv
diff out/bake-off/nack-hybrid.csv docs/data/bake-off/nack-hybrid.csv
```

Hai dòng `diff` phải không in gì cả. Mọi dòng đều do một mô phỏng có seed tạo ra — seed là
`0x5DEECE66D`, in trong cột `seed` của mọi dòng `fec-sweep.csv` — không mạng và không GPU, nên các
file tái lập từng byte trên bất kỳ máy nào build được test.

- Dữ liệu thô: [`docs/data/bake-off/fec-sweep.csv`](../data/bake-off/fec-sweep.csv) (180 dòng) ·
  [`docs/data/bake-off/nack-hybrid.csv`](../data/bake-off/nack-hybrid.csv) (72 dòng)
- Script sinh ra chúng: [`scripts/bake-off-csv.sh`](../../scripts/bake-off-csv.sh)
- Chính phần mô phỏng:
  [`core/tests/transport/LossGoodputTests.cpp`](../../core/tests/transport/LossGoodputTests.cpp)
- Từng bảng kết luận gì, nói theo ngôn ngữ của dự án:
  [`docs/ARCHITECTURE.vi.md`](../ARCHITECTURE.vi.md)

Ý nghĩa các cột nằm trong dòng header của CSV. `rescue_pct` là `frames_rescued / frames_damaged`;
`overhead_pct` là số gói parity trên số gói dữ liệu; `idr_per_min` là số lần xin keyframe quy đổi
theo `frames_sent × 16 667 µs`; `longest_stall_ms` là khoảng cách lớn nhất giữa hai frame được giao.
