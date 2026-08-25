[English](SPECIFICATION.md) · **Tiếng Việt**

# Deskhub — Đặc tả chức năng

Tài liệu này mô tả Deskhub **làm được gì**, dưới góc nhìn của người dùng. Đây là đặc tả
sản phẩm, không phải tài liệu thiết kế: không có chi tiết cài đặt, không mô tả giao thức,
không hướng dẫn build. Những nội dung đó nằm ở [`INSTALL.vi.md`](INSTALL.vi.md),
[`BUILD.vi.md`](BUILD.vi.md), [`SECURITY.vi.md`](../SECURITY.vi.md) và trong mã nguồn.

Đây là bản dịch của [`SPECIFICATION.md`](SPECIFICATION.md); khi hai bản khác nhau, bản
tiếng Anh là bản chuẩn.

- **Trạng thái:** mô tả hành vi của mã nguồn hiện tại.
- **Đối tượng đọc:** người cần biết sản phẩm phải làm gì — người kiểm thử, người review,
  người đóng góp, nội dung mô tả trên store.

---

## 1. Tóm tắt sản phẩm

Deskhub cho phép một máy hiển thị màn hình của nó cho các máy khác trong cùng mạng, và
cho phép các máy đó điều khiển chuột và bàn phím của nó. Chỉ có một ứng dụng duy nhất:
cùng một app vừa chia sẻ màn hình, vừa xem màn hình của máy khác. Máy để bàn còn chia sẻ
được cả **terminal**: một shell thật trên host mà máy khác mở trong cửa sổ riêng (mục 4
và 5).

Không bắt buộc cài đặt, không có tài khoản, không đăng nhập, không chạy nền, không có
thành phần đám mây. Hai máy tìm thấy nhau bằng địa chỉ IP trong một mạng mà cả hai đều
truy cập được.

## 2. Thuật ngữ

| Thuật ngữ | Ý nghĩa |
| --- | --- |
| **Host** | Máy đang được chia sẻ màn hình (hoặc terminal). |
| **Client** / **Viewer** | Máy đang xem host, và có thể điều khiển host. |
| **Source** (nguồn) | Một thứ chia sẻ được trên host: một màn hình, hoặc terminal. Một host có thể chia sẻ nhiều nguồn cùng lúc. |
| **Session** (phiên) | Một viewer đang xem một nguồn. Mỗi nguồn mở trong một cửa sổ riêng. |
| **Key** (khoá) | Danh tính mật mã mà mỗi máy tự sinh ở lần chạy đầu, hiển thị cho người dùng dưới dạng dấu vân tay (`SHA256:…`). |
| **Pairing** (ghép đôi) | Việc host cho một máy vào lâu dài. Máy đã ghép đôi được nhận diện bằng khoá của nó và kết nối không cần passcode, cho tới khi bị quên đi (mục 9). |
| **Passcode** (mã) | Mã 4 chữ số tuỳ chọn mà host có thể yêu cầu trước khi một máy lạ được ghép đôi. |

Một máy có thể vừa là host vừa là client cùng lúc.

## 3. Vai trò theo nền tảng

| Nền tảng | Chia sẻ được | Xem được | Âm thanh |
| --- | :--: | :--: | :--: |
| Windows | ✅ | ✅ | ✅ |
| macOS | ✅ | ✅ | ✅ |
| Linux | ✅ | ✅ | ✅ |
| Android | ✅ chỉ xem | ✅ | ⚠️ Android 10+ |
| iOS | ✅ chỉ xem | ✅ | ⚠️ chỉ tiếng của app |

Mọi nền tảng đều có cùng bộ tính năng phía client, trừ những khác biệt nêu ở mục 12.
Điện thoại và máy tính bảng chia sẻ ở chế độ **chỉ xem**: chúng phát màn hình nhưng không
bao giờ nhận điều khiển từ xa, vì không hệ điều hành di động nào cho một ứng dụng thường
điều khiển máy.

Ứng dụng được chia thành các mục cùng tên trên mọi nền tảng: **Host**, **Client** và
**Settings** — cộng thêm trang **Devices** liệt kê các máy đã ghép đôi với máy này
(mục 9).

Ba nền tảng để bàn còn có một client dòng lệnh. Nó làm đúng những việc trên mà không có
trang nào cả: làm host, kết nối, mở shell từ xa, tìm máy, và đọc ghi đúng những cấu hình,
máy đã ghép đôi và khoá host đã tin mà app dùng. Nó là một khuôn mặt khác của những hành vi
mô tả trong tài liệu này, không bao giờ là hành vi khác.

---

## 4. Host — chia sẻ màn hình máy này

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| H-1 | Chọn màn hình | Trước khi chia sẻ, người dùng tích chọn những màn hình của máy sẽ được chia sẻ. Phải chọn ít nhất một. |
| H-2 | Chia sẻ nhiều màn hình | Có thể chia sẻ nhiều màn hình cùng lúc; mỗi màn hình thành một nguồn riêng để viewer chọn. |
| H-3 | Giới hạn nguồn | Tối đa **8** màn hình được chia sẻ cùng lúc. Nếu máy có nhiều hơn, người dùng được cảnh báo rằng chỉ 8 màn hình đầu được chia sẻ. |
| H-4 | Bật / tắt chia sẻ | Một thao tác để bắt đầu, một thao tác để dừng. Trạng thái hiện tại luôn được hiển thị (*Không chia sẻ* / *Đang bắt đầu…* / *Đang chia sẻ*). |
| H-5 | Dừng một màn hình | Có thể dừng riêng một màn hình đang chia sẻ mà không kết thúc toàn bộ phiên chia sẻ. |
| H-6 | Thông tin kết nối | Khi đang chia sẻ, ứng dụng liệt kê các địa chỉ mạng của máy và cổng mà viewer cần dùng, để đọc hoặc sao chép cho người khác. Trên desktop, mục *Chia sẻ trên mạng* (T-9) nằm ngay trên màn host cạnh danh sách này, và danh sách chỉ hiện địa chỉ của mạng đang được chọn — chọn *Mọi mạng* thì hiện tất cả. Khi đang chia sẻ (hoặc đang khởi động chia sẻ), mục chọn này bị khoá; dừng chia sẻ để đổi. |
| H-7 | Bảng phiên trực tiếp | Với mỗi màn hình đang chia sẻ, host thấy: tên màn hình, độ phân giải, số viewer, tốc độ thu hình, tốc độ gửi, băng thông đang dùng và độ trễ khứ hồi. Mỗi viewer đang kết nối hiện thành một dòng riêng dưới màn hình tương ứng, nhận diện bằng tên hiển thị kèm địa chỉ — dạng "Tên (ip:port)" — nếu viewer đã đặt tên (C-7), hoặc chỉ bằng địa chỉ nếu chưa. |
| H-8 | Ngắt một viewer | Host có thể ngắt bất kỳ viewer nào từ bảng phiên. |
| H-9 | Giới hạn viewer | Tối đa **5** viewer xem một host cùng lúc. Các kết nối thêm bị từ chối với lý do máy đang bận. |
| H-10 | Báo lỗi | Nếu không bắt đầu chia sẻ được, lý do được hiển thị cho người dùng thay vì thất bại âm thầm. Terminal không khởi động được vì cổng đã bị chiếm sẽ báo đúng điều đó. |
| H-11 | Nguồn Terminal (desktop) | Danh sách nguồn có thêm mục **Terminal — a shell on this machine**. Nó được tích sẵn mỗi lần danh sách hiện ra và không bao giờ được lưu; chia sẻ màn hình không kèm terminal, terminal không kèm màn hình, hay cả hai, đều hợp lệ. Mọi thứ dùng chung một cổng UDP duy nhất của app (T-4), passcode và lựa chọn mạng. |
| H-12 | Phiên shell trong bảng | Khi terminal đang được chia sẻ, bảng phiên trực tiếp có một dòng *Terminal* (kèm cổng), và mỗi shell đang mở là một dòng bên dưới, nhận diện như viewer (C-7), với nút *Disconnect*; nút *Stop* của dòng *Terminal* chỉ dừng chia sẻ terminal. Tối đa **8** shell mở cùng lúc. Mọi lần shell được mở, đóng, gắn lại hay hết hạn đều được ghi vào nhật ký phiên (G-3) kèm địa chỉ, tên và khoá của máy client. |
| H-13 | Shell sống sót khi rớt mạng | Shell bị mất kết nối được giữ sống trong **2 phút** để đúng máy đó gắn lại với nội dung phiên còn nguyên; quá hạn thì bị huỷ. Client tự gắn lại: nó thử lại với khoảng chờ giãn dần trong suốt thời gian host còn giữ shell, báo là đang gắn lại trong lúc đó, và lấy lại đúng shell cũ với nội dung lẫn scrollback. Chỉ khi hết 2 phút nó mới báo mất kết nối và mời làm lại từ đầu. |
| H-15 | Shell rảnh không phải shell đứt | Kết nối terminal không có lưu lượng vẫn được client giữ sống, nên một shell để yên ở dấu nhắc không bị nhầm là đứt link rồi đóng đi. |
| H-16 | Nguồn truyền tệp (desktop) | Danh sách nguồn có thêm mục **File transfer — files viewers send**, được tích sẵn mỗi lần danh sách hiện ra và không bao giờ được lưu; nó dùng chung một cổng UDP duy nhất của app (T-4), passcode và lựa chọn mạng như terminal (H-11). Tệp rơi vào thư mục được nêu ngay dưới bộ chọn và trong trạng thái chia sẻ (T-25). Một lô mang tối đa **32** tệp, **8 GiB** mỗi tệp và **32 GiB** tổng cộng; lớn hơn thế, hoặc tên không lưu được, sẽ bị từ chối kèm lý do. Mỗi tệp được ghi cạnh tên cuối cùng của nó với đuôi `.deskhub-part` và chỉ được đổi tên khi đã tới đủ và khớp checksum; tệp hỏng bị bỏ đi và làm lô dừng lại. Không bao giờ có gì bị ghi đè: tên đã có trong thư mục sẽ được thêm số vào sau. Nếu không ghi được vào thư mục đó thì không nhận tệp nào cả, và host nói rõ thay vì thất bại lặng lẽ. |
| H-17 | Lượt truyền trong bảng | Khi truyền tệp đang được chia sẻ, bảng trực tiếp hiện một dòng *File transfer* nêu tên thư mục, và mỗi máy đang gửi là một dòng bên dưới, nhận diện như một viewer (C-7), kèm tệp đang nhận, vị trí của nó trong lô và phần đã xong — hoặc lý do lô dừng lại. Nút *Stop* trên dòng *File transfer* chỉ kết thúc riêng phần truyền tệp. Mọi lô được chào, được nhận, bị từ chối hay đã xong đều được ghi vào nhật ký phiên (G-3) kèm địa chỉ, tên và khóa của máy gửi. |
| H-14 | Stop & attach (desktop) | Mỗi dòng shell — đang sống hay đang chờ gắn lại — còn có nút **Stop & attach**: client từ xa bị ngắt (cửa sổ của họ báo shell đã kết thúc) và đúng shell đó mở ra trong một cửa sổ terminal ngay trên máy host, nội dung lẫn scrollback còn nguyên. Từ đó shell thuộc về máy host: client cũ không gắn lại được, giới hạn 2 phút (H-13) không còn áp dụng, bảng đánh dấu dòng đó là *attached on this machine*, và đóng cửa sổ của host — hoặc bấm *Stop* trên dòng đó — sẽ kết thúc shell. Lần tiếp quản được ghi vào nhật ký phiên (G-3). |

## 5. Client — kết nối và xem máy khác

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| C-1 | Kết nối theo địa chỉ | Người dùng nhập địa chỉ IP của host vào một ô và cổng UDP vào ô riêng, được điền sẵn giá trị mặc định `47777`. Dán `192.168.1.10:47777` vào ô địa chỉ vẫn hoạt động — cổng ghi rõ trong địa chỉ được ưu tiên hơn ô cổng. Nhập sai định dạng sẽ hiện gợi ý giải thích chứ không báo lỗi cụt. |
| C-2 | Nhập passcode | Ô passcode được phép để trống. Mã đã gõ phải đúng 4 chữ số, nếu không kết nối bị chặn trước khi gửi đi bất cứ thứ gì. Ô hiện gì thì đúng cái đó được dùng — không có gì được điền hộ sau lưng người dùng. Với ô trống, host là bên quyết định: máy đã ghép đôi được vào thẳng; máy chưa ghép thì đứng chờ (khoảng một phút) trong lúc người ở host được hỏi có cho vào không (S-2). Mã đã gõ mà host từ chối thì báo lỗi nêu rõ lý do passcode. Hộp thoại mở ra từ danh sách thiết bị hiển thị cổng UDP của thiết bị và mã đã nhớ (D-7), đều điền sẵn và sửa được. |
| C-3 | Tuỳ chọn chỉ xem | Trước khi kết nối, viewer có thể bỏ tích *điều khiển máy từ xa* để chỉ xem mà không gửi bất kỳ thao tác nào. Một host hoàn toàn không nhận thao tác — điện thoại hay máy tính bảng (P-4), hoặc một desktop chia sẻ với tuỳ chọn nhận thao tác tắt — sẽ nói rõ điều đó khi được hỏi nó đang chia sẻ những gì, và các client desktop luôn hiển thị một dòng lưu ý rằng ô điều khiển và ô terminal không có tác dụng với host như vậy. |
| C-4 | Chọn nguồn | Nếu host chia sẻ nhiều hơn một màn hình, viewer được hỏi muốn xem màn hình nào. Chọn nhiều thì mở nhiều cửa sổ. Nếu host chỉ chia sẻ một màn hình, cửa sổ mở ngay. |
| C-5 | Lỗi rõ ràng | Nếu không tới được host, host không chia sẻ, hoặc passcode sai, viewer được cho biết chính xác là trường hợp nào — kèm địa chỉ trong thông báo. |
| C-6 | Thông báo kết thúc | Khi một phiên kết thúc, từ phía nào cũng vậy, viewer được cho biết lý do. |
| C-7 | Tên người xem | Ô *Your name* trên trang kết nối dùng để đặt tên cho thiết bị này. Khi người dùng chưa từng đặt tên, ô được điền sẵn giá trị mặc định theo nền tảng: hostname của máy trên Windows và Linux (tên đăng nhập nếu không lấy được hostname), tên máy tính trên macOS, tên thiết bị trên iOS, và model thiết bị trên Android. Ô có thể sửa, và nội dung của ô lúc kết nối chính là thứ được lưu và gửi đi. Ô không bao giờ rơi vào trạng thái không có tên: kết nối khi ô bị xoá trắng sẽ quay về giá trị mặc định theo nền tảng ở trên — giá trị đó được điền lại vào ô, được lưu và gửi đi — nên mỗi kết nối luôn kèm theo một cái tên. Host hiển thị tên đó bên cạnh địa chỉ của máy để phân biệt các viewer với nhau. Tên được nhớ trên thiết bị, dài tối đa **64** byte, và các ký tự điều khiển bị loại bỏ. Host chạy phiên bản cũ đơn giản là không hiển thị tên. |
| C-8 | Mở shell | *Terminal — open a shell* là một nút xuất hiện khi host đã trả lời (C-10), trên mọi client. Shell mở trong cửa sổ riêng — lưới ký tự, cuộn lại lịch sử, dòng trạng thái, và trên điện thoại có thêm hàng phím phụ (Esc, Tab, Ctrl/Alt giữ trạng thái, mũi tên, ^C) — và cửa sổ đó nêu rõ vì sao shell không mở được (sai passcode, bị từ chối, không tới được máy). Vừa xem màn hình vừa chạy shell là chuyện bình thường. Mọi client đều biết host chia sẻ những gì trước khi mở bất cứ thứ gì: với host không có terminal — điện thoại, máy tính bảng, hoặc một desktop không chia sẻ terminal — nút này không bật, nên không cửa sổ terminal nào được mở ra cả. |
| C-9 | Gửi tệp | Mọi client đều có thể gửi tệp tới host đang nhận tệp. *File transfer — send files to it* là một nút xuất hiện khi host đã trả lời (C-10), trên mọi client, và mở màn hình **Send files**. Trên Android và iOS, tệp được chọn từ bộ chọn ảnh của hệ thống hoặc trình duyệt tệp của hệ thống, và một bản sao được chuẩn bị trong cache riêng của app trước khi gửi. Mỗi lần một lô: khi một lô đang chạy thì các bộ chọn bị khóa và lời chào thứ hai bị từ chối vì đang bận. Tiến độ nêu tên tệp đang gửi, vị trí của nó trong lô và phần đã xong, và có thể dừng lượt truyền bất cứ lúc nào. Khi lô kết thúc, từng tệp được liệt kê là đã gửi hay chưa, kèm lý do. Mọi client đều biết host chia sẻ những gì trước khi mở bất cứ thứ gì: với host không nhận tệp thì nút này không bật, nên không cửa sổ nào được mở ra cả. |
| C-10 | Kết nối rồi mới chọn | Nút Connect chỉ làm việc xác thực: nó gọi tới host, hoàn tất ghép đôi hoặc passcode (S-2), và hỏi host đang chia sẻ những gì. Một host đã trả lời thì đưa ra cùng những thứ đó ở mọi nơi: địa chỉ của nó, nút **Ngắt kết nối**, dòng trực tiếp của V-7, và mỗi khả năng một nút: *Remote desktop — view its screen*, *Terminal — open a shell* và *File transfer — send files to it*, mỗi nút chỉ bật nếu host có chia sẻ khả năng tương ứng. Mở một phiên dùng lại chính lần ghép đôi vừa xong, nên không ai ở phía host bị hỏi lần thứ hai. Những thứ đó nằm ở đâu thì tuỳ nền tảng (C-11). |
| C-11 | Mỗi host một cửa sổ (desktop) | Trên Windows, Linux và macOS, host nào trả lời thì mở một **cửa sổ kết nối** riêng, lấy địa chỉ làm tiêu đề và chứa mọi thứ C-10 liệt kê. Bản thân trang kết nối không đổi trạng thái: các ô địa chỉ, cổng, passcode và tên, nút Connect và danh sách thiết bị vẫn nguyên đó, nên có thể gọi host tiếp theo trong khi host đầu vẫn đang mở, và một máy có thể kết nối tới nhiều host cùng lúc. Kết nối lại tới host đã có cửa sổ thì cửa sổ đó được đưa lên trước chứ không mở thêm cái thứ hai. Đóng một cửa sổ kết nối, hoặc bấm **Ngắt kết nối** của nó, chỉ bỏ host đó và không đụng tới các host khác; thoát app thì đóng hết. Những phiên đã mở từ một cửa sổ (V-1, C-8, C-9) là cửa sổ riêng và sống lâu hơn nó. Trên Android và iOS thì mỗi lúc chỉ một kết nối và nó nằm ngay trên trang kết nối: cho tới khi Connect thành công, trang chỉ gồm các ô, nút Connect và danh sách thiết bị — không có gì khác; khi host trả lời, những thứ đó nhường chỗ cho những gì C-10 liệt kê, và Ngắt kết nối, hoặc sửa địa chỉ, cổng hay passcode, đưa trang về trạng thái đầu. |

## 6. Tìm máy để kết nối

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| D-1 | Quét mạng | Client quét mạng nội bộ để tìm các máy đang chia sẻ và liệt kê chúng, có hiển thị tiến độ trong lúc quét ("đã kiểm tra *n* trên *m* địa chỉ"). Khi quét xong mà không tìm thấy gì, người dùng được giải thích vì sao một máy có thể vắng mặt: máy chỉ xuất hiện khi đang chia sẻ. |
| D-2 | Phạm vi quét | Mỗi lần quét kiểm tra tối đa **512** địa chỉ trong mạng nội bộ. Nếu máy không có địa chỉ mạng nội bộ, người dùng được báo là không quét được. |
| D-3 | Tự quét lại | Việc quét lặp lại định kỳ, và có thể chạy lại ngay bằng *Refresh now*. |
| D-4 | Bấm để kết nối | Bấm vào một thiết bị tìm được sẽ bắt đầu kết nối tới thiết bị đó. |
| D-5 | Thiết bị gần đây | Các máy đã từng kết nối được lưu trong danh sách *Devices* — tối đa **10** — được đánh dấu *Recent* ở cột *Where*, kèm địa chỉ, trạng thái, ping và thời điểm kết nối gần nhất. |
| D-6 | Trạng thái trực tiếp | Mỗi thiết bị gần đây hiển thị **Online**, **Offline** hoặc **Checking…** kèm độ trễ khứ hồi, tự làm mới mỗi **30 giây** và làm mới được theo yêu cầu. |
| D-7 | Nhớ passcode | Passcode đã dùng cho một thiết bị được lưu cùng thiết bị đó và điền sẵn vào hộp thoại khi kết nối từ danh sách thiết bị — hiện rõ trong ô sửa được, không bao giờ dùng ngầm. Kết nối mà không gõ mã sẽ không xoá mã đã nhớ; gõ mã mới thì thay mã cũ. Mã được lưu ở dạng che đi — đây là tiện lợi, không phải bảo vệ (xem mục 9). Khi hai máy đã ghép đôi thì mã không còn vai trò: chúng nhận nhau bằng khoá. |
| D-8 | Xoá thiết bị | Có thể xoá một thiết bị khỏi danh sách gần đây. |

## 7. Xem một phiên

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| V-1 | Vừa khung | Màn hình từ xa được co giãn vừa cửa sổ, giữ nguyên tỉ lệ; cửa sổ mở ra với kích thước theo nguồn. Trên desktop, khi hình dạng luồng thực sự thay đổi giữa phiên — host điện thoại/máy tính bảng xoay màn hình, hoặc chuyển sang màn hình có tỉ lệ khác — cửa sổ tự chỉnh lại theo hình dạng mới; thay đổi chất lượng cùng tỉ lệ thì không đụng tới cửa sổ. |
| V-2 | Phóng to và kéo | Có thể phóng to tới **5×** và kéo để di chuyển vùng nhìn. Mức phóng được hiển thị và đặt lại được bằng một thao tác. |
| V-3 | Trạng thái phiên | Cửa sổ hiển thị dòng trạng thái trực tiếp: tốc độ khung hình, băng thông, độ trễ khứ hồi và độ trễ đầu-cuối. |
| V-4 | Cửa sổ có tiêu đề | Mỗi cửa sổ xem có tiêu đề gồm tên nguồn đang xem và trạng thái hiện tại, để phân biệt được khi mở nhiều phiên. |
| V-5 | Ngắt kết nối | Viewer có thể kết thúc phiên bất cứ lúc nào. |
| V-6 | Âm thanh | Ở những nơi cả hai máy đều hỗ trợ (mục 3), viewer nghe được thứ máy đang chia sẻ phát ra, lệch hình chừng một khung. Âm thanh đi trên kênh riêng: mất một gói chỉ mất một phần nhỏ của giây tiếng và không bao giờ làm hỏng hình, còn máy không phát gì thì gần như không tốn băng thông. Viewer tắt (T-23) thì không nghe, host tắt (T-22) thì không gửi. |
| V-7 | Sức khỏe kết nối | Số đo nằm ở nơi host được trả lời — cửa sổ kết nối trên desktop, trang kết nối trên Android và iOS (C-11) — chứ không phải cửa sổ phiên: nó hiện địa chỉ của host, nút **Ngắt kết nối** (V-5) và một dòng trực tiếp báo đang kết nối, kèm ping bên cạnh, chuyển đỏ ngay khi host đó ngừng trả lời. Số đo đến từ chính nhịp dò mỗi giây một lần nuôi danh sách thiết bị, nên nó có mặt trước khi mở phiên và ở lại trong lúc nhiều phiên đang chạy, và trên desktop mỗi host đang mở mang số đo của riêng nó. Cửa sổ phiên mất host vẫn báo đang nối lại (V-8). |
| V-8 | Tự nối lại | Phiên mất host — mạng rơi, luồng hình câm lặng — không kết thúc. Cửa sổ giữ khung hình cuối, báo đang nối lại, và quay số lại theo backoff trong tối đa một phút; host trả lời lại là hình tự chạy tiếp. Chỉ sau một phút đó, hoặc khi host chủ động kết thúc hay từ chối phiên, cửa sổ mới đóng kèm lý do. Cửa sổ shell giữ nguyên hai phút ân hạn nối lại như trước. |

## 8. Điều khiển máy từ xa

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| I-1 | Chuột | Di chuyển, các nút trái / phải / giữa / lùi / tiến, và con lăn đều được gửi tới host. |
| I-2 | Bàn phím | Nhấn và nhả phím được gửi đi, bao gồm cả tổ hợp phím bổ trợ. |
| I-3 | Khoá con trỏ (desktop) | `F9` khoá chuột vào màn hình từ xa, phục vụ game và các phần mềm cần chuyển động chuột thô; `F9` hoặc `Esc` để nhả. Trạng thái hiện tại hiển thị trên tiêu đề cửa sổ. |
| I-4 | An toàn khi mất focus | Khi cửa sổ mất focus, con trỏ được nhả khoá và mọi phím đang giữ được thả ra, nên không có phím nào bị kẹt trên host. |
| I-5 | Trackpad cảm ứng (di động) | Trên điện thoại và máy tính bảng, khung hình hoạt động như trackpad: kéo để di chuyển con trỏ, chạm để bấm, chạm hai lần để bấm chuột phải, giữ rồi kéo để rê, kéo hai ngón theo chiều dọc để cuộn. |
| I-6 | Chế độ con trỏ / kéo (di động) | Một nút chuyển giữa điều khiển con trỏ từ xa và kéo vùng nhìn khi đang phóng to. |
| I-7 | Bàn phím ảo (di động) | Bàn phím của thiết bị có thể hiện/ẩn theo yêu cầu và gõ thẳng vào máy từ xa. |
| I-8 | Thanh phím tắt (di động) | Nút tắt cho những phím khó gõ trên bàn phím cảm ứng: `Esc`, `Tab`, `Enter`, bốn phím mũi tên, `Del`, `Ctrl+C`, `Ctrl+V`. |
| I-9 | Host luôn thắng | Thao tác của người đang ngồi trực tiếp tại máy host được ưu tiên hơn mọi viewer từ xa. |
| I-10 | Mỗi lúc một người điều khiển | Chỉ một viewer điều khiển chuột và bàn phím tại một thời điểm. Người vào sớm nhất thắng khi tranh chấp; thao tác của các viewer còn lại bị bỏ qua cho tới khi người đang điều khiển ngừng thao tác **1 giây**. |
| I-11 | Bắt buộc chỉ xem | Khi host tắt quyền điều khiển, hoặc viewer chọn chỉ xem, không thao tác nào tới được host và cửa sổ viewer ghi rõ đang ở chế độ chỉ xem. |

## 9. Kiểm soát truy cập và an toàn

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| S-1 | Mã hoá | Phiên làm việc chạy trên kênh truyền có mã hoá (QUIC/TLS). Mọi thứ một phiên chuyên chở — video, điều khiển, thao tác chuột phím, clipboard và terminal — đều đi trong kênh mã hoá giữa hai máy. Gói beacon phục vụ dò tìm là bản rõ có chủ đích và không mang bí mật nào; mọi gói không mã hoá khác tới cổng đều bị bỏ. Xem bức tranh đầy đủ trong [`SECURITY.vi.md`](../SECURITY.vi.md). |
| S-2 | Ghép đôi quyết định việc vào | Lần đầu một máy kết nối, host là bên quyết định cho vào hay không. Máy đưa ra passcode của host thì chứng minh nó biết mã bằng mật mã — bản thân mã không bao giờ đi qua mạng. Máy không đưa mã nào — hoặc host không đặt mã để đối chiếu — thì câu hỏi được chuyển cho người ngồi tại host: *Let this machine in?*, với **Allow** và **Deny**, và một câu trả lời áp cho mọi thứ máy đó đang mở (cả màn hình lẫn shell). Cho vào tức là **ghép đôi** hai máy: từ đó nó được nhận diện bằng khoá và kết nối không cần passcode, cho tới khi bị quên đi. Nhưng mã đã gõ thì luôn bị kiểm — máy đã ghép đôi mà đưa mã sai cũng bị chặn. |
| S-3 | Passcode tuỳ chọn, danh sách máy đã ghép | Passcode là tuỳ chọn và mặc định để trống; khi trống, không gì vào được nếu người ở host chưa phê duyệt. Các máy đã ghép đôi được liệt kê trên trang **Devices** — tên, khoá, ngày ghép, lần gặp cuối — với *Forget* và *Forget every machine*, một công tắc *allow new pairings* mà khi tắt thì chỉ máy đã ghép mới vào được, cùng khoá của chính máy này để đọc đối chiếu. Host chỉ tiết lộ đang chia sẻ những gì cho máy đã được cho vào. |
| S-4 | Khoá khi sai nhiều lần | Sai passcode **3** lần sẽ khoá phần ghép đôi của host trong **30 giây**, và máy đang thử được báo là phải chờ. Máy đã ghép đôi không bị ảnh hưởng. |
| S-5 | Công tắc điều khiển | Host có thể chia sẻ với tuỳ chọn *viewer được điều khiển máy này* tắt đi, khiến mọi phiên đều là chỉ xem bất kể viewer yêu cầu gì. |
| S-6 | Đồng ý thu hình | Trên các nền tảng yêu cầu, hệ điều hành tự hiện hộp thoại xin quyền và hộp thoại chọn màn hình; Deskhub không thu hình được nếu người dùng chưa cấp quyền. |
| S-7 | Chỉ chia sẻ khi được yêu cầu | Không có gì được chia sẻ cho tới khi người dùng bấm bắt đầu. Đóng ứng dụng hoặc dừng chia sẻ sẽ kết thúc mọi phiên. |
| S-8 | Cảnh báo khoá đổi | Client ghi nhớ khoá của mọi host nó đã tin. Nếu khoá đó đổi — đúng hình dạng của một máy chen giữa — một cảnh báo lớn hiện dấu vân tay mới và kết nối bị từ chối cho tới khi người dùng chấp nhận rõ ràng. Khoá chưa gặp bao giờ thì được chính cuộc bắt tay ghép đôi phân xử, không hỏi gì thêm. |

## 10. Cài đặt

Cài đặt thuộc về từng máy, được lưu lại qua các lần khởi động, và có hiệu lực từ lần bắt
đầu chia sẻ kế tiếp. Điện thoại và máy tính bảng chỉ hiện cổng mạng (T-4) — cũng chính là
cổng mà việc quét mạng gõ vào — đồng bộ clipboard (T-17) và giữ máy thức (T-19), cùng mã
passcode (T-5) và mạng để chia sẻ (T-9) trên màn hình chia sẻ; mọi thứ còn lại chúng dùng
giá trị mặc định dựng sẵn.

| ID | Cài đặt | Khoảng giá trị | Mặc định |
| --- | --- | --- | --- |
| T-1 | Tốc độ khung hình | 1 – 240 fps | 60 |
| T-2 | Bitrate | 1 – 1000 Mbps | 20 |
| T-3 | Chất lượng | 720p · 1080p · 1440p · Native | 1080p |
| T-4 | Cổng mạng | 1 – 65535 | 47777 |
| T-5 | Passcode | trống, hoặc đúng 4 chữ số | trống (xem S-2, S-3) |
| T-6 | Viewer được điều khiển máy này | bật / tắt | bật |
| T-9 | Chia sẻ trên mạng | Mọi mạng · một trong các địa chỉ của máy | Mọi mạng |
| T-11 | Bắt đầu chia sẻ khi mở app | bật / tắt | tắt |
| T-13 | Khởi động Deskhub khi đăng nhập | bật / tắt | tắt |
| T-15 | Tiếp tục chạy trong nền | bật / tắt | tắt |
| T-17 | Đồng bộ văn bản clipboard | bật / tắt | tắt |
| T-19 | Giữ thiết bị này thức trong phiên | bật / tắt | bật |
| T-21 | Cho máy mới ghép đôi (trang Devices) | bật / tắt | bật |
| T-22 | Chia sẻ tiếng của máy này cho viewer | bật / tắt | bật |
| T-23 | Phát tiếng của máy đang xem | bật / tắt | bật |

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| T-7 | Chất lượng tự động | Chất lượng luồng tự điều chỉnh theo băng thông khả dụng, trong giới hạn đã cấu hình; người dùng không phải làm gì khi điều kiện mạng thay đổi. |
| T-8 | Kiểm tra giá trị | Giá trị ngoài khoảng hoặc không phải số bị từ chối và giữ nguyên giá trị cũ, thay vì được áp dụng. |
| T-10 | Quay về mọi mạng | Khi đã chọn một mạng cụ thể (T-9), host chỉ tiếp cận được qua địa chỉ đó. Nếu địa chỉ đó không còn tồn tại lúc bắt đầu chia sẻ, host chia sẻ trên mọi mạng và nói rõ điều đó trong dòng trạng thái chia sẻ. Địa chỉ đã lưu nhưng hiện không khả dụng vẫn được liệt kê, đánh dấu *not connected*. |
| T-12 | Tự chia sẻ khi mở app | Chỉ desktop. Khi bật T-11, mở app sẽ vào thẳng trang Host và bắt đầu chia sẻ với cài đặt đã lưu, đúng như khi người dùng bấm Share. Khi app được khởi chạy lúc đăng nhập (T-13), desktop có thể chưa có màn hình nào; lúc đó việc chia sẻ sẽ chờ, kiểm tra lại mỗi nửa giây trong tối đa 30 giây, và bắt đầu ngay khi một màn hình xuất hiện. Trong lúc chờ, trang Host báo là đang chờ. Nếu không màn hình nào xuất hiện, app chia sẻ những gì còn được tick (terminal) hoặc đứng yên với lý do hiển thị trên trang Host — một lần chia sẻ tự động không bao giờ mở hộp thoại, vì lúc đăng nhập cửa sổ có thể đang ẩn trong khay hệ thống, không ai thấy. Các quy tắc nền tảng vẫn áp dụng: Linux hiện hộp thoại chia sẻ màn hình của desktop lần đầu rồi dùng lại lựa chọn đã nhớ từ đó về sau (P-3), macOS vẫn yêu cầu các quyền của nó (P-2). |
| T-14 | Khởi động cùng hệ điều hành | Chỉ desktop. Khi bật T-13: Linux ghi một mục autostart vào `~/.config/autostart`; Windows đăng ký một scheduled task tên *Deskhub* khởi động app với quyền cao lúc đăng nhập, nên không hiện hộp thoại UAC; macOS đăng ký một Login Item mà người dùng cũng thấy được trong System Settings. Tắt đi sẽ gỡ bỏ đúng thứ đã tạo. Ô chọn luôn hiển thị trạng thái mà hệ điều hành báo, không chỉ là giá trị đã lưu lần cuối. |
| T-16 | Chế độ chạy nền | Chỉ desktop. Khi bật T-15, một biểu tượng khay / thanh menu xuất hiện với *Hiện/Ẩn cửa sổ*, *Bắt đầu/Dừng chia sẻ* và *Thoát*; đóng cửa sổ sẽ ẩn app thay vì thoát, và việc chia sẻ tiếp tục trong nền. Cửa sổ luôn hiện ra lúc khởi động và chỉ ẩn khi người dùng đóng nó, nên T-13 + T-11 + T-15 kết hợp sẽ tự chia sẻ ngay khi đăng nhập với cửa sổ hiện cho tới khi được đóng. Trên Windows, bấm chuột trái vào biểu tượng khay sẽ hiện hoặc ẩn cửa sổ. Trên macOS biểu tượng Dock biến mất khi cửa sổ đang ẩn. Trên Linux khay cần một StatusNotifier host (mặc định có trên KDE; GNOME cần extension AppIndicator) — nếu không có, đóng cửa sổ vẫn thoát app, để app không bao giờ trở nên không với tới được. Trên Windows và Linux, khi đang chia sẻ, đóng cửa sổ luôn ẩn về khay kể cả khi T-15 tắt (nếu khay khả dụng), nên các viewer đang kết nối không bị ngắt; trên macOS đóng cửa sổ không bao giờ thoát app, nên việc chia sẻ vẫn tiếp tục dù T-15 bật hay tắt. |
| T-18 | Đồng bộ clipboard | Khi bật T-17, văn bản thuần copy trên một máy trong phiên sẽ xuất hiện trên các máy còn lại trong vòng vài giây, theo cả hai chiều; host chuyển tiếp bản copy của một viewer tới các viewer khác. Văn bản giới hạn 32 KiB (bản dài hơn bị cắt tại ranh giới ký tự); ảnh, file và định dạng không bao giờ được truyền. Công tắc của host quyết định cả phiên: tắt thì host bỏ qua và không bao giờ gửi dữ liệu clipboard. Mỗi máy cũng cần bật công tắc của chính nó để đọc/ghi clipboard cục bộ. Trên Android và iOS, hệ điều hành giới hạn việc này: thiết bị Android chỉ nhặt được bản copy của chính nó khi Deskhub là ứng dụng đang ở nền trước, còn văn bản gửi tới thì được áp dụng bất cứ lúc nào; viewer trên iOS có thể thấy hộp thoại dán của hệ thống khi Deskhub đọc một bản copy mới; và thiết bị iOS đang làm host hoàn toàn không tham gia, vì broadcast của nó chạy trong một process riêng không truy cập được clipboard. |
| T-20 | Giữ máy thức | Khi bật T-19, máy không đi ngủ và màn hình không tắt trong lúc đang chia sẻ hoặc đang xem; khóa được nhả ngay khi phiên kết thúc, và không cài đặt ngủ nào của hệ thống bị thay đổi. Trên Windows, macOS và Linux, điều này chặn cả ngủ màn hình lẫn ngủ hệ thống, cho cả host lẫn viewer (trên Linux cần systemd-logind và một desktop tôn trọng giao diện screensaver của freedesktop — mặc định có trên KDE và GNOME). Hệ điều hành vẫn thắng ở những chỗ nó cương quyết: gập nắp laptop, bấm nút nguồn, hoặc macOS chạy pin vẫn có thể đưa máy vào giấc ngủ. Trên Android và iOS, công tắc này giữ màn hình sáng khi đang xem một stream; việc chia sẻ từ điện thoại vốn đã sống sót khi màn hình tắt (P-5), nên khi làm host điện thoại không giữ màn hình sáng. |
| T-25 | Tệp rơi vào đâu | Chỉ desktop. Tệp viewer gửi tới được ghi vào một thư mục do máy này chọn — `Deskhub` trong thư mục nhà của người dùng nếu không chọn thư mục khác. Thư mục đã chọn được hiện cạnh ô tích truyền tệp trước khi chia sẻ và trong trạng thái chia sẻ khi đang chia sẻ, được tạo ra nếu chưa có, và được lưu cùng các thiết lập khác. Không có gì được ghi ra ngoài thư mục đó: tên do bên gửi đưa sang bị cắt còn thành phần cuối của đường dẫn và loại bỏ mọi ký tự hệ tệp cục bộ không lưu được. |
| T-24 | Tiếng nào được chia sẻ | Khi bật T-22, host chia sẻ đúng thứ loa của nó đang phát — bản trộn của mọi ứng dụng trên máy. Nó không bao giờ thu micro: Deskhub không có âm thanh hai chiều, và không xin quyền micro trên bất kỳ nền tảng nào. Viewer chỉ nhận tiếng nếu chính nó xin (T-23), nên một host bật T-22 vẫn không gửi gì cho viewer không nghe; cả hai công tắc có hiệu lực từ phiên kế tiếp. |

## 11. Trạng thái và chẩn đoán

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| G-1 | Thống kê phía host | Số liệu theo từng màn hình và từng viewer: tốc độ thu hình, tốc độ gửi, băng thông và độ trễ khứ hồi. |
| G-2 | Thống kê phía client | Theo từng phiên: tốc độ khung hình, băng thông, độ trễ khứ hồi và độ trễ đầu-cuối. |
| G-3 | Nhật ký phiên | Trên Windows, macOS và Linux, mỗi lần chạy ghi một tệp log vào thư mục Deskhub của người dùng, để đính kèm khi báo lỗi. Android và iOS thay vào đó ghi chẩn đoán vào luồng log của chính hệ điều hành và không để lại tệp nào. |
| G-4 | Phiên bản và liên kết dự án | Ứng dụng hiển thị phiên bản của nó và liên kết tới trang dự án. |

## 12. Khác biệt theo nền tảng

| ID | Nền tảng | Hành vi |
| --- | --- | --- |
| P-1 | Windows | Ứng dụng xin quyền quản trị một lần lúc khởi động, đây là điều kiện để gõ được vào các cửa sổ chạy với quyền cao. Khi bắt đầu chia sẻ, ứng dụng tự thêm luật tường lửa của mình. |
| P-2 | macOS | Hiển thị mục **Permissions** với trạng thái cấp quyền theo thời gian thực của *Screen Recording* (cần để chia sẻ) và *Accessibility* (cần để nhận thao tác từ xa), nút xin từng quyền, và lối tắt mở System Settings. Một số phím bị macOS chặn âm thầm nếu chưa cấp Accessibility. |
| P-3 | Linux | Trang Host liệt kê các màn hình của máy này và terminal dưới dạng các ô tick, giống các desktop khác, và chỉ những màn hình được tick mới được chia sẻ. Môi trường desktop vẫn xác nhận việc thu màn hình trong hộp thoại chia sẻ màn hình của chính nó sau khi bấm Share; xác nhận đó được ghi nhớ khi desktop hỗ trợ (ScreenCast portal phiên bản 4 trở lên): các lần chia sẻ sau dùng lại nó trong im lặng, kể cả sau khi khởi động lại, nên hộp thoại chỉ hiện lần đầu tiên. Nếu những gì desktop đã cấp không khớp chút nào với các màn hình được tick, xác nhận đã nhớ sẽ bị quên và hộp thoại hiện lại một lần nữa để người dùng cấp đúng màn hình; nếu desktop từ chối hoặc đã hết hạn xác nhận — sau khi nâng cấp compositor hay thay đổi màn hình — hộp thoại đơn giản là hiện lại, và bấm hủy sẽ không bị hỏi lại. Nếu desktop cấp những màn hình mà ứng dụng không khớp được với danh sách đã tick, toàn bộ những gì desktop cấp sẽ được chia sẻ thay vì không chia sẻ gì. Chỉ tick terminal thì bỏ qua hoàn toàn hộp thoại của desktop. Việc chia sẻ còn cần hệ thống cho phép mô phỏng thao tác nhập liệu. |
| P-4 | Android / iOS | Chia sẻ ở chế độ **chỉ xem**: thiết bị phát màn hình và lặng lẽ bỏ qua mọi gói điều khiển, vì cả hai hệ điều hành đều không cho ứng dụng bơm thao tác vào toàn hệ thống. Nó không chia sẻ terminal — và nói rõ điều đó khi được hỏi, nên không client nào mở cửa sổ terminal nhắm vào điện thoại (C-8) — và các client desktop có thể nói trước rằng ô điều khiển sẽ không có tác dụng (C-3). Nó nhận tệp ngay từ lúc app hiện trên màn hình, theo đúng các quy tắc lô của H-16, không phải đi tìm công tắc nào, và tiếp tục nhận tệp trong lúc đang chia sẻ màn hình, nên một viewer có thể vừa xem màn hình vừa gửi tệp sang; trên iOS phần mở rộng broadcast giữ cổng duy nhất đó suốt thời gian broadcast và phục vụ cả hai việc từ nó. Ảnh và video nhận được sẽ được thêm vào thư viện ảnh của thiết bị; mọi tệp khác nằm ở nơi trình duyệt tệp của hệ thống thấy được (thư mục Documents của app trên iOS, Downloads trên Android), và một thông báo nêu tên những gì vừa tới. Toàn bộ màn hình được chia sẻ như một nguồn duy nhất, nên bộ chọn màn hình, chia sẻ nhiều màn hình và dừng từng màn hình (H-1, H-2, H-3, H-5) không áp dụng. Xoay thiết bị thì luồng xoay theo: hình người xem thấy vẫn đúng chiều, và cửa sổ của họ tự chỉnh lại theo hình dạng mới (V-1). Giao diện phiên ưu tiên cảm ứng: cử chỉ trackpad, nút phóng to, thanh phím tắt, bàn phím ảo, nút đổi màn hình, và một nút đóng ở góc dùng chung với màn hình shell và gửi tệp. |
| P-5 | Android | Muốn chia sẻ phải qua hộp thoại xin quyền quay màn hình của hệ thống, cấp cho từng lần và không nhớ được. Trong lúc chia sẻ luôn có một thông báo thường trực, và luồng vẫn chạy khi ứng dụng xuống nền hoặc màn hình tắt. Tắt chia sẻ từ thông báo hệ thống sẽ kết thúc phiên. |
| P-6 | iOS | Chia sẻ được khởi động từ nút **Start sharing** trong ứng dụng, nút này mở bảng broadcast của hệ thống vì iOS bắt buộc phải qua bảng đó để xác nhận mọi lần phát, và chạy trong một tiến trình broadcast riêng nên vẫn tiếp tục sau khi đóng ứng dụng. Màn hình chia sẻ báo số người xem đang kết nối — liệt kê tên của những người xem đã đặt tên (C-7) — và mức bộ nhớ hiện tại của tiến trình broadcast — iOS sẽ chấm dứt buổi phát nào dùng quá giới hạn bộ nhớ — không có bảng chi tiết từng người như H-7, và không thể ngắt riêng từng người xem (H-8). Một sự kiện hệ thống làm dừng broadcast — ví dụ cuộc gọi đến — sẽ kết thúc phiên. |

## 13. Nằm ngoài phạm vi

Deskhub **không** cung cấp, và đặc tả này không bao gồm:

- Thu micro, âm thanh hai chiều, hay bất kỳ kênh thoại nào. Tiếng chỉ đi một chiều, từ máy
  đang chia sẻ tới những người đang xem nó (V-6).
- In từ xa.
- Đồng bộ clipboard ngoài văn bản thuần (ảnh, tệp, văn bản có định dạng).
- Bất kỳ hệ thống tài khoản, danh bạ, hiện diện hay lời mời nào.
- Dịch vụ trung chuyển, điểm hẹn hay xuyên NAT — việc tiếp cận host qua internet là trách
  nhiệm của người dùng (ví dụ bằng VPN).
- Ghi lại phiên làm việc.
- Truy cập khi không có người tại máy, wake-on-LAN, hay điều khiển nguồn điện từ xa.
- Quản trị nhiều người dùng, phân quyền, hay nhật ký kiểm toán.
