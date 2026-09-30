[English](SPECIFICATION.md) · **Tiếng Việt** · [中文](SPECIFICATION.zh.md) · [日本語](SPECIFICATION.ja.md)

# Deskhub — Đặc tả chức năng

Đặc tả này mô tả những việc người dùng có thể làm với Deskhub và cách app phản hồi.
Hướng dẫn cài đặt, build và bảo mật nằm trong [`INSTALL.vi.md`](INSTALL.vi.md),
[`BUILD.vi.md`](BUILD.vi.md) và [`SECURITY.vi.md`](../SECURITY.vi.md). Chi tiết triển khai
nằm trong [`ARCHITECTURE.vi.md`](ARCHITECTURE.vi.md) và cây source.

Đây là bản dịch của [`SPECIFICATION.md`](SPECIFICATION.md). Nếu hai bản có khác biệt, bản
tiếng Anh là bản chuẩn.

- **Trạng thái:** mô tả hành vi của mã nguồn hiện tại.
- **Đối tượng:** tester, reviewer, người đóng góp và người viết nội dung cho store.

---

## 1. Tóm tắt sản phẩm

Deskhub cho phép bạn share màn hình máy này với một thiết bị khác trên network mà cả hai
đều truy cập được. Viewer cũng có thể điều khiển mouse và keyboard khi host cho phép.
Cùng một app vừa share màn hình, vừa connect tới máy khác. Máy desktop còn có thể share
**terminal**: một shell trên host mà thiết bị đã connect mở trong cửa sổ riêng (mục 4 và 5).

Có bộ cài nhưng không bắt buộc dùng. Deskhub không yêu cầu tài khoản, đăng nhập,
background service hay thành phần cloud. Client connect tới host bằng địa chỉ IP qua
network mà cả hai máy đều truy cập được.

## 2. Thuật ngữ

| Thuật ngữ | Ý nghĩa |
| --- | --- |
| **Host** | Máy đang được share màn hình (hoặc terminal). |
| **Client** / **Viewer** | Máy đang xem một host, và có thể điều khiển host đó. |
| **Source** | Một đối tượng có thể share trên host: một display, hoặc terminal. Một host có thể share nhiều source cùng lúc. |
| **Session** | Một viewer xem một source. Mỗi source mở trong một cửa sổ riêng. |
| **Key của máy** | Key duy nhất mà một máy tạo ra trong lần chạy đầu tiên và không bao giờ tự thay. Nó là danh tính của máy ở cả hai vai: host dùng nó để chứng minh danh tính khi máy khác connect tới, và client dùng nó để đăng nhập khi connect tới một host. Người dùng nhìn thấy nó dưới dạng một fingerprint duy nhất (`SHA256:…`); nửa public của nó là thứ chủ host thêm vào danh sách client được phép. |
| **Client được phép** | Danh sách public key được phép connect tới host (`authorized_keys`), mỗi key kèm một nhãn. Chỉ key có trong danh sách mới được vào (mục 9). |
| **Host đã trust** | Một host mà máy này đã quyết định trust, theo key của nó: fingerprint được ghim ở lần connect đầu tiên hoặc lấy từ mã QR của host, một tên, và địa chỉ gần nhất mà host trả lời. Trust đi theo key, nên host đã trust vẫn giữ được trust khi đổi địa chỉ. |
| **Yêu cầu kết nối** | Thứ host ghi lại khi một thiết bị chưa được cho phép thử connect: tên thiết bị, fingerprint key, địa chỉ và thời điểm. Chủ host có thể **Approve** hoặc **Deny** nó (H-18). |
| **Mã QR** / **lời mời** | Mã mà một host đang share có thể hiển thị — cũng là một link `deskhub://pair/…` — mang các địa chỉ của host, fingerprint, tên của nó và một token dùng một lần. Thiết bị scan hoặc dán mã sẽ trust host và được cho vào ngay, không cần bước approve (C-12). |
| **Tên thiết bị** | Tên duy nhất của máy này — hiển thị cho viewer khi máy làm host và gửi tới các client được phép connect tới nó, gửi tới host khi máy connect, và dùng làm nhãn cho public key được copy từ máy (T-26). |

Một máy có thể đồng thời là host và client.

## 3. Vai trò theo nền tảng

| Nền tảng | Host | Xem | Âm thanh |
| --- | :--: | :--: | :--: |
| Windows | ✅ | ✅ | ✅ |
| macOS | ✅ | ✅ | ✅ |
| Linux | ✅ | ✅ | ✅ |
| Android | ✅ view-only | ✅ | ⚠️ Android 10+ |
| iOS | ✅ view-only | ✅ | ⚠️ chỉ audio của app |

Mọi nền tảng đều có cùng tập tính năng phía client, trừ những điểm nêu ở mục 12. Điện
thoại và tablet host ở chế độ **view-only**: chúng stream màn hình nhưng không nhận remote
input, vì không OS di động nào cho phép app thông thường điều khiển thiết bị.

App được tổ chức thành các phần giống nhau trên mọi nền tảng: **Host**, **Client** và
**Settings**, cùng trang **Devices** chứa các key quyết định ai được connect tới đâu (mục 9).

Ba nền tảng desktop còn có command line client để share, connect, mở remote shell và quản
lý client được phép, yêu cầu kết nối cùng các host đã trust. Nó đọc và ghi cùng settings,
key của máy, client được phép và host đã trust như app.
CLI xem màn hình được trên Windows và Linux; trên macOS, hãy dùng app để xem màn hình.

---

## 4. Host — share màn hình của máy này

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| H-1 | Chọn display | Trước khi share, người dùng chọn những display nào của máy này được hiển thị ra ngoài. Phải chọn ít nhất một. |
| H-2 | Share nhiều display | Có thể share nhiều display đồng thời; mỗi display trở thành một source riêng để viewer lựa chọn. |
| H-3 | Giới hạn source | Tối đa **8** display được share cùng lúc. Nếu máy có nhiều hơn, người dùng được cảnh báo rằng chỉ 8 display đầu tiên được share. |
| H-4 | Bắt đầu và dừng share | Một thao tác bắt đầu share, một thao tác dừng. Khi đã bắt đầu share hoặc đang bắt đầu, một banner hiển thị trạng thái (*Starting share…* / *Sharing*) cùng chi tiết đi kèm; trước đó, nhãn của nút (*Start sharing*) là trạng thái duy nhất được hiển thị nên banner không xuất hiện. |
| H-5 | Dừng một display | Có thể dừng riêng một display đang được share mà không kết thúc toàn bộ phiên share. |
| H-6 | Thông tin kết nối | Trong khi share, app liệt kê các địa chỉ network của máy này cùng port mà viewer cần dùng, để người dùng đọc hoặc copy. Cạnh danh sách đó là **Show QR code**: nó vẽ một mã QR — cùng link `deskhub://pair/…` mà mã mã hoá, kèm nút copy — cho một thiết bị vào ngay khi thiết bị đó scan hoặc dán mã (C-12). Mỗi lần hiển thị, một mã mới được tạo; nút đổi thành *Hide QR code* trong khi mã đang hiển thị, và việc ẩn mã, hoặc dừng share, sẽ thu hồi mã để nó không dùng được nữa. Trên desktop, lựa chọn *Share on network* (T-9) nằm trên màn hình host cạnh danh sách này, và danh sách chỉ hiển thị địa chỉ của network đã chọn; *All networks* hiển thị toàn bộ địa chỉ. Trong khi đang share hoặc đang bắt đầu share, lựa chọn này bị khoá; cần dừng share để thay đổi. |
| H-7 | Bảng session trực tiếp | Với mỗi display đang share, host thấy: tên display, độ phân giải, số lượng viewer, capture rate, send rate, băng thông đang sử dụng và round-trip time. Mỗi viewer đang kết nối là một dòng riêng bên dưới display tương ứng, được nhận diện bằng tên thiết bị và địa chỉ theo dạng "Tên (ip:port)" (C-7), hoặc chỉ bằng địa chỉ nếu viewer không gửi tên. |
| H-8 | Ngắt kết nối một viewer | Host có thể ngắt kết nối bất kỳ viewer nào từ bảng session. |
| H-9 | Giới hạn viewer | Tối đa **5** viewer xem cùng một host tại một thời điểm. Các yêu cầu tiếp theo bị từ chối với lý do bận. |
| H-10 | Báo lỗi | Nếu không thể bắt đầu share, nguyên nhân được hiển thị cho người dùng thay vì thất bại âm thầm. Trường hợp terminal không khởi động được do port đã bị chiếm sẽ được nêu rõ. |
| H-11 | Source terminal (desktop) | Danh sách source còn có mục **Terminal — a shell on this machine**. Mục này được chọn lại từ đầu mỗi lần danh sách hiển thị và không được lưu; share màn hình không kèm terminal, chỉ terminal không kèm màn hình, hoặc cả hai, đều hợp lệ. Tất cả dùng chung một UDP port (T-4), cùng danh sách client được phép (S-2) và cùng lựa chọn network của app. |
| H-12 | Các shell session trong bảng | Khi terminal đang được share, bảng trực tiếp hiển thị một dòng *Terminal* kèm port của nó, và mỗi shell đang mở là một dòng bên dưới, được nhận diện như một viewer (C-7), kèm nút *Disconnect*. Nút *Stop* trên dòng *Terminal* chỉ kết thúc phần share terminal. Tối đa **8** shell mở đồng thời. Mỗi lần một shell được mở, đóng hay reattach đều được ghi vào session log (G-3) kèm địa chỉ, tên và key của client. |
| H-13 | Shell được giữ lại trên host | Một shell bị mất kết nối — network gián đoạn, hoặc cửa sổ client bị đóng — được giữ lại trên host cùng nguyên nội dung và scrollback: không có giới hạn thời gian. Chính client đó tự reattach: nó thử lại với khoảng chờ tăng dần trong hai phút, hiển thị trạng thái đang reattach, và nhận lại đúng shell đó. Mọi client đã được chấp nhận cũng có thể hỏi host đang giữ những shell nào và reattach một shell theo id của nó thay vì mở shell mới. Một shell chỉ kết thúc khi tiến trình shell của nó thoát, host ngừng share terminal, hoặc nó bị đóng khỏi bảng session; việc đóng cửa sổ client không bao giờ kết thúc shell. Bất kỳ client nào đã được nhận vào cũng có thể kết thúc một shell đang được giữ theo id, ngay từ danh sách đó; máy đang gõ trong shell ấy được báo là shell đã kết thúc. |
| H-15 | Shell không hoạt động khác với shell mất kết nối | Một kết nối terminal không có lưu lượng vẫn được client giữ sống, nên một shell đang dừng tại dấu nhắc không bị hiểu nhầm là đứt kết nối và bị đóng. |
| H-16 | Source file transfer (desktop) | Danh sách source còn có mục **File transfer — files viewers send**, được chọn lại từ đầu mỗi lần danh sách hiển thị và không được lưu. Giống terminal (H-11), mục này dùng chung một UDP port (T-4), danh sách client được phép (S-2) và lựa chọn network của app. File được lưu vào thư mục ghi trên dòng *File transfer* của bảng trực tiếp khi bắt đầu share (H-17, T-25). Một batch chứa tối đa **32** file, **8 GiB** mỗi file và **32 GiB** tổng cộng; phần vượt quá, hoặc tên file không lưu được, bị từ chối kèm lý do. Mỗi file được ghi cạnh tên cuối cùng với hậu tố `.deskhub-part` và chỉ được đổi tên khi đã nhận đủ và checksum khớp; file hỏng bị loại bỏ và làm dừng cả batch. Không có file nào bị ghi đè: tên đã tồn tại trong thư mục sẽ được thêm số thứ tự. Nếu không ghi được vào thư mục, host không nhận file nào và thông báo rõ thay vì thất bại âm thầm. |
| H-17 | Các phiên truyền file trong bảng | Khi file transfer đang được share, bảng trực tiếp hiển thị một dòng *File transfer* ghi tên thư mục, kèm nút **Open folder** bên cạnh để mở thư mục đó trong trình quản lý file của hệ điều hành, và mỗi máy đang gửi là một dòng bên dưới, được nhận diện như một viewer (C-7), kèm file đang nhận, vị trí của nó trong batch và tiến độ, hoặc lý do batch dừng lại. Nút *Stop* trên dòng *File transfer* chỉ kết thúc phần file transfer. Mỗi batch được đề nghị, chấp nhận, từ chối hoặc hoàn tất đều được ghi vào session log (G-3) kèm địa chỉ, tên và key của máy gửi. |
| H-14 | Stop & attach (desktop) | Mọi dòng shell, dù đang hoạt động hay đang chờ reattach, đều có thêm **Stop & attach**: client từ xa bị ngắt kết nối (cửa sổ của nó báo shell đã kết thúc) và chính shell đó được mở trong một cửa sổ terminal trên host, giữ nguyên nội dung và scrollback. Từ thời điểm đó, shell thuộc về máy host: client cũ không reattach được nữa, đằng nào cũng không có giới hạn thời gian (H-13), bảng đánh dấu dòng đó là *attached on this machine*, và việc đóng cửa sổ trên host, hoặc nhấn *Stop* trên dòng tương ứng, sẽ kết thúc shell. Việc chuyển giao này được ghi vào session log (G-3). |
| H-18 | Yêu cầu kết nối | Thiết bị connect khi chưa nằm trong danh sách client được phép không bị đuổi đi: host ghi lại một **yêu cầu kết nối** và trang Host liệt kê nó, giữa danh sách địa chỉ và bảng session trực tiếp, trên mọi nền tảng. Mỗi dòng hiển thị tên thiết bị (hoặc *(unnamed)*), phần đầu fingerprint key của nó, địa chỉ của nó, cùng **Approve** và **Deny**. *Approve* thêm key của thiết bị vào danh sách client được phép với tên thiết bị làm nhãn, nên lần thử tiếp theo của nó — nó tự thử lại (C-5) — được vào; *Deny* bỏ yêu cầu và không làm gì khác, nên thiết bị chỉ được báo rằng không ai approve kịp thời. Một yêu cầu tự hết hạn sau **10 phút**, host giữ tối đa **16**, và thiết bị xin lại sẽ làm mới dòng của chính nó thay vì thêm dòng mới. Khi không có gì đang chờ, mục này nói vậy trong một dòng. Cùng danh sách này có trên trang Devices (S-3), nên có thể trả lời yêu cầu mà không cần mở trang Host, và mỗi yêu cầu mới cũng phát một **thông báo hệ thống** — *Connection request*: ‹tên› (‹địa chỉ›) wants to connect to this machine — khi app đang chạy, một lần cho mỗi thiết bị, trên mọi nền tảng; không có thông báo cho yêu cầu tới khi app đã đóng, và thông báo không được gửi đi đâu ngoài trung tâm thông báo của chính máy này. CLI liệt kê chúng bằng `access requests` và trả lời bằng `access approve` / `access deny --fingerprint SHA256:…`; một `share` đang chạy cũng in ra từng yêu cầu mới khi nó tới. |

## 5. Connect — xem một máy khác

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| C-1 | Connect bằng địa chỉ | Người dùng nhập địa chỉ IP của host vào một ô và UDP port vào ô còn lại, với giá trị mặc định `47777`. Dán `192.168.1.10:47777` vào ô địa chỉ cũng hợp lệ; port ghi trong chuỗi được ưu tiên hơn ô port. Ô này cũng nhận một link `deskhub://pair/…` dán từ mã QR của host (C-12): Connect khi đó gọi tới các địa chỉ trong link, và ô được điền bằng địa chỉ đã trả lời. Dữ liệu nhập không hợp lệ — kể cả văn bản không phải địa chỉ cũng không phải link Deskhub — tạo ra gợi ý giải thích, không phải một lỗi. |
| C-2 | Lần connect đầu tiên tới một host | Connect tới một host mà máy này chưa từng trust sẽ mở hộp thoại **New host**, hiển thị fingerprint key của host và yêu cầu người dùng đối chiếu với fingerprint trên trang Devices của host (S-3). *Cancel* không lưu gì và không connect; **Trust and connect** lưu host vào danh sách host đã trust (S-8) rồi connect. Nếu địa chỉ đó từng thuộc về một host đã trust khác, hộp thoại nói rõ — nêu tên host đó và fingerprint của nó — vì một key khác ở địa chỉ đã biết là một máy khác. Host đã được trust thì connect mà không hỏi gì, dù được tới qua địa chỉ nào. Host được tới qua mã QR của nó (C-12) được trust mà không có hộp thoại: mã đã mang sẵn fingerprint của host, và kết nối chỉ tiếp tục nếu máy trả lời chứng minh được nó giữ key đó. |
| C-3 | Bỏ chọn control | Trước khi connect, viewer có thể bỏ chọn *control the remote machine* để chỉ xem mà không gửi input. Một host không nhận input — điện thoại hoặc tablet (P-4), hoặc một desktop share với input tắt — sẽ thông báo điều này khi được hỏi về nội dung đang share, và các client desktop hiển thị ghi chú thường trực rằng control và terminal không có tác dụng với host như vậy. |
| C-4 | Chọn source | Nếu host đang share nhiều hơn một display, viewer được hỏi muốn xem display nào. Chọn nhiều display sẽ mở nhiều cửa sổ. Nếu host chỉ share một display, display đó mở ngay. |
| C-5 | Thông báo lỗi rõ ràng | Viewer được thông báo chính xác lý do kết nối thất bại — không truy cập được host, host không share, host chưa được trust, host từ chối thiết bị này, hoặc chủ host không approve thiết bị này kịp thời (S-2) — kèm địa chỉ trong nội dung thông báo. Host chưa cho phép thiết bị này không làm nó thất bại ngay: client hiển thị *Waiting for the owner of ‹host› to approve this device…* kèm nút **Cancel**, và tự gọi lại vài giây một lần trong tối đa **2 phút**; ngay khi chủ host bấm *Approve* (H-18), lần thử tiếp theo được vào và trang tiếp tục như chưa có gì xảy ra. Chỉ sau hai phút đó nó mới thất bại, báo rằng chủ host không approve kịp thời và hãy nhờ họ bấm *Approve* dưới *Connection requests* trên trang Host hoặc Devices của họ, rồi connect lại — yêu cầu vẫn nằm trên host trong 10 phút, nên một lần approve muộn chỉ cần thêm một lần Connect. Thông báo lỗi cũng nêu hai cách vào còn lại: scan mã QR của host (C-12), hoặc copy public key của thiết bị này từ *Devices → Copy public key* để chủ host dán vào *Devices allowed to connect to this machine*; CLI bổ sung các lệnh `deskhub-cli access approve --fingerprint SHA256:…`, `deskhub-cli key public` và `deskhub-cli access add --stdin` để làm việc tương tự. |
| C-6 | Thông báo kết thúc session | Khi một session kết thúc, từ bất kỳ phía nào, viewer được thông báo nguyên nhân. |
| C-7 | Tên viewer | Mỗi kết nối luôn mang theo tên thiết bị của máy này (T-26); trang connect không có ô nhập tên riêng. Host hiển thị tên này cạnh địa chỉ của máy để phân biệt các viewer. |
| C-8 | Mở một shell | *Terminal — open a shell* là nút xuất hiện sau khi host đã phản hồi (C-10), trên mọi client. Shell mở trong cửa sổ riêng, gồm lưới ký tự, scrollback, dòng status, và trên điện thoại có thêm một hàng phím phụ (Esc, Tab, Ctrl/Alt có thể khoá, các phím mũi tên, ^C). Cửa sổ này cũng nêu nguyên nhân khi không mở được shell (thiết bị này không được approve kịp thời, bị từ chối, không truy cập được). Việc vừa xem màn hình vừa dùng một shell là bình thường. Mọi client đều xác định host share những gì trước khi mở bất kỳ cửa sổ nào: với host không có terminal — điện thoại, tablet, hoặc desktop không share terminal — nút này ở trạng thái disabled và không có cửa sổ terminal nào được mở. Client cũng có thể hỏi host đang giữ những shell nào và reattach một shell theo id của nó thay vì mở shell mới. Khi host đang giữ sẵn shell, nhấn nút này sẽ mở **danh sách các shell đó** trước —— mỗi dòng nêu id, kích thước và máy đã mở nó —— và shell chỉ mở ra sau khi chọn reattach một shell hoặc chọn *New shell*; nếu host không giữ shell nào, một shell mới mở ngay. Shell mà máy khác đang gõ, hoặc shell đã được host tiếp quản (H-14), vẫn hiện trong danh sách nhưng không thể reattach. Mỗi dòng mà client được phép kết thúc còn có *Close shell*, hỏi lại trước rồi kết thúc shell đó trên host, kể cả khi máy khác đang giữ. |
| C-9 | Gửi file | Mọi client đều có thể gửi file tới một host đang nhận file. *File transfer — send files to it* là nút xuất hiện sau khi host đã phản hồi (C-10), trên mọi client, và mở màn hình **Send files**. Trên Android và iOS, file được chọn từ photo picker hoặc trình duyệt file của hệ thống, và một bản sao được chuẩn bị trong cache riêng của app trước khi gửi. Mỗi lần chỉ một batch: khi một batch đang chạy, các picker bị vô hiệu hoá và đề nghị thứ hai bị từ chối với lý do bận. Phần tiến độ nêu tên file đang gửi, vị trí của nó trong batch và tỷ lệ hoàn tất; phiên truyền có thể dừng bất cứ lúc nào. Khi kết thúc, từng file trong batch được liệt kê là đã gửi hay chưa, kèm lý do. Mọi client đều xác định host share những gì trước khi mở bất kỳ cửa sổ nào: với host không nhận file, nút này ở trạng thái disabled và không có cửa sổ nào được mở. |
| C-10 | Connect trước, chọn sau | Connect chỉ thực hiện việc authenticate: nó kết nối tới host, kiểm tra key của host (S-8), chứng minh key của chính máy này (S-2), sau đó truy vấn nội dung host đang share. Một host đã phản hồi cung cấp cùng một tập nội dung trên mọi nền tảng: địa chỉ của nó, nút **Disconnect**, dòng trạng thái trực tiếp ở V-7, và một nút cho mỗi mục *Remote desktop — view its screen*, *Terminal — open a shell* và *File transfer — send files to it*, trong đó chỉ các mục host thực sự share mới được bật. Mỗi session mở sau đó lại đăng nhập bằng cùng key đó; người dùng tại host không bao giờ bị hỏi gì. Vị trí hiển thị các nội dung này khác nhau theo nền tảng (C-11). |
| C-11 | Mỗi host một cửa sổ (desktop) | Trên Windows, Linux và macOS, một host phản hồi sẽ mở một **cửa sổ kết nối** riêng, tiêu đề là địa chỉ của host, chứa toàn bộ nội dung liệt kê ở C-10. Bản thân trang connect không đổi trạng thái: các ô địa chỉ và port, nút Connect cùng danh sách gần đây vẫn giữ nguyên, nên có thể kết nối tới host tiếp theo trong khi host đầu vẫn đang mở, và một máy có thể kết nối tới nhiều host cùng lúc. Connect lại tới một host đã có cửa sổ sẽ đưa cửa sổ đó lên trước thay vì mở cửa sổ thứ hai. Đóng một cửa sổ kết nối, hoặc nhấn **Disconnect** trong đó, chỉ ngắt host tương ứng và không ảnh hưởng các host khác; thoát app sẽ đóng toàn bộ. Các session đã mở từ một cửa sổ (V-1, C-8, C-9) là những cửa sổ riêng và tồn tại độc lập với nó. Trên Android và iOS, mỗi lần chỉ có một kết nối và kết nối này nằm trên trang connect: cho tới khi Connect thành công, trang chỉ gồm các ô nhập, nút Connect và danh sách gần đây; khi host phản hồi, các thành phần đó được thay bằng nội dung liệt kê ở C-10, và Disconnect, hoặc việc chỉnh sửa địa chỉ hay port, sẽ đưa trang về trạng thái ban đầu. |
| C-12 | Connect bằng mã QR | Lần connect đầu tiên nhanh nhất, không cần đối chiếu fingerprint và không cần approve. Trên điện thoại hoặc tablet, trang Client có nút **Scan QR code** mở camera; lần đầu, hệ điều hành xin permission camera, permission mà Deskhub chỉ xin ở đây và không dùng cho việc gì khác — nếu từ chối, trang hướng dẫn cách dán link thay thế. Mở một link `deskhub://pair/…` từ app khác (camera hệ thống, một tin nhắn) cũng dẫn tới đúng chỗ đó. Mọi client, kể cả desktop, cũng có thể dán link vào ô địa chỉ (C-1). Mã mang tối đa bốn địa chỉ của host cùng port, fingerprint key của host, tên của nó, và một token ngẫu nhiên dùng một lần. Client gọi lần lượt các địa chỉ và, trước khi gửi bất cứ thứ gì khác, kiểm tra rằng máy trả lời giữ key được nêu trong mã: nếu không, kết nối dừng lại với *The machine that answered is not the one that made this QR code* và token không bao giờ được gửi. Nếu có, host được trust mà không có hộp thoại *New host* (C-2), token được gửi cùng key và tên của thiết bị này, và host thêm key vào danh sách client được phép, gắn nhãn bằng tên thiết bị, rồi cho vào — trang Client chuyển thẳng tới nội dung liệt kê ở C-10. Mỗi mã dùng được một lần và tối đa **5 phút**; ẩn mã trên host, hoặc dừng share, sẽ thu hồi nó (H-6). Thiết bị mà host đã cho phép có thể scan mã chỉ để lấy địa chỉ hiện tại của host: nó vào như bình thường và token không được dùng. Khung hình camera được giải mã trên thiết bị và không bao giờ được lưu hay gửi đi đâu. |

## 6. Tìm máy

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| D-1 | Không scan network | Deskhub không bao giờ scan network và host không trả lời packet discovery nào. Client tới được một host qua địa chỉ người dùng nhập (C-1), một địa chỉ gần đây (D-5), một host đã trust (S-8), hoặc các địa chỉ trong mã QR mà chủ host cho họ xem (C-12) — mã đi bằng mắt hoặc bằng thao tác dán, không bao giờ qua network, nên đó không phải discovery. |
| D-4 | Nhấn để connect | Chọn một địa chỉ gần đây, hoặc *Connect* trên một host đã trust, sẽ bắt đầu kết nối tới đó. |
| D-5 | Thiết bị gần đây | Các host từng kết nối được giữ trong danh sách gần đây trên trang Client, tối đa **10** host, mỗi mục hiển thị tên của host (tên thiết bị của nó, T-26, mà host chỉ gửi sau khi máy này đã authenticate; là địa chỉ nếu host không gửi tên), địa chỉ và thời điểm kết nối gần nhất. Một host chỉ được thêm vào sau khi đã trả lời. Danh sách gần đây của các phiên bản cũ không được chuyển sang. |
| D-8 | Xoá một thiết bị | Một địa chỉ gần đây có thể được gỡ khỏi danh sách. |

## 7. Xem một session

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| V-1 | Vừa khung cửa sổ | Màn hình từ xa được scale vừa cửa sổ, giữ nguyên tỷ lệ khung hình, và cửa sổ lấy kích thước theo source khi mở. Trên desktop, khi hình dạng của stream thay đổi giữa phiên — host là điện thoại hoặc tablet xoay màn hình, hoặc chuyển sang một display có hình dạng khác — cửa sổ tự điều chỉnh theo hình dạng mới. Việc thay đổi quality khi hình dạng không đổi không làm thay đổi cửa sổ. |
| V-2 | Zoom và pan | Khung xem có thể zoom tới **5×** và pan. Mức zoom được hiển thị và có thể đặt lại bằng một thao tác. |
| V-3 | Trạng thái session | Cửa sổ hiển thị một dòng status trực tiếp: frame rate, băng thông, round-trip time và latency end-to-end. |
| V-4 | Cửa sổ có tiêu đề | Mỗi cửa sổ viewer có tiêu đề gồm source đang hiển thị và trạng thái hiện thời, nên nhiều session vẫn phân biệt được. |
| V-5 | Disconnect | Viewer có thể kết thúc session bất cứ lúc nào. |
| V-6 | Âm thanh | Khi cả hai máy đều hỗ trợ (mục 3), viewer nghe được nội dung máy được share đang phát, đồng bộ với hình ảnh trong khoảng một frame. Âm thanh đi trên channel riêng: mất một packet chỉ mất một phần nhỏ của giây và không ảnh hưởng tới hình ảnh, còn một máy không phát gì thì gần như không tiêu tốn băng thông. Âm thanh bị tắt với viewer đã tắt nó (T-23) và không được gửi bởi host đã tắt nó (T-22). |
| V-7 | Tình trạng kết nối | Chỉ số này nằm ở nơi host đã phản hồi — cửa sổ kết nối trên desktop, trang connect trên Android và iOS (C-11) — chứ không nằm trong cửa sổ session. Nó hiển thị địa chỉ của host, nút **Disconnect** (V-5) và một dòng trực tiếp báo trạng thái đang kết nối kèm ping, chuyển sang màu đỏ ngay khi host ngừng phản hồi. Số liệu lấy từ ping mỗi giây một lần qua kết nối đã authenticate, nên nó đã có trước khi mở session và tiếp tục hiển thị khi nhiều session đang chạy; trên desktop, mỗi host đang mở có chỉ số riêng. Cửa sổ session mất kết nối với host vẫn báo trạng thái đang reattach (V-8). |
| V-8 | Tự kết nối lại | Một session mất host — network gián đoạn, stream ngừng dữ liệu — không kết thúc ngay. Cửa sổ giữ nguyên khung hình cuối, báo trạng thái đang reattach, và kết nối lại với backoff trong tối đa một phút; khi host phản hồi trở lại, hình ảnh tiếp tục. Chỉ sau khoảng thời gian đó, hoặc khi host chủ động kết thúc hay từ chối session, cửa sổ mới đóng kèm lý do. Cửa sổ shell tự thử lại trong hai phút rồi báo mất kết nối; bản thân shell vẫn ở trên host không giới hạn thời gian, sẵn sàng để nhận lại (H-13). |

## 8. Điều khiển máy từ xa

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| I-1 | Mouse | Chuyển động, các nút trái, phải, giữa, back và forward, cùng bánh xe cuộn đều được gửi tới host. |
| I-2 | Keyboard | Các sự kiện nhấn và nhả phím đều được gửi, bao gồm cả tổ hợp phím modifier. |
| I-3 | Pointer lock (desktop) | `F9` khoá mouse vào màn hình từ xa, phục vụ game và các phần mềm cần chuyển động thô; `F9` hoặc `Esc` giải phóng. Trạng thái hiện thời được hiển thị trên tiêu đề cửa sổ. |
| I-4 | An toàn khi mất focus | Khi mất focus, pointer lock và mọi phím đang được giữ đều được giải phóng, nên không phím nào bị kẹt trên host. |
| I-5 | Trackpad cảm ứng (mobile) | Trên điện thoại và tablet, khung video hoạt động như một trackpad: kéo để di chuyển con trỏ, chạm để nhấn chuột trái, chạm hai lần để nhấn chuột phải, giữ và kéo để drag, kéo dọc bằng hai ngón để cuộn. |
| I-6 | Chế độ pointer và pan (mobile) | Một công tắc chuyển giữa việc di chuyển con trỏ từ xa và pan khung hình đang zoom. |
| I-7 | Keyboard trên màn hình (mobile) | Keyboard của thiết bị có thể hiển thị hoặc ẩn theo yêu cầu và nhập trực tiếp vào máy từ xa. |
| I-8 | Thanh hotkey (mobile) | Các nút tắt cho những phím khó nhập trên keyboard cảm ứng: `Esc`, `Tab`, `Enter`, bốn phím mũi tên, `Del`, `Ctrl+C`, `Ctrl+V`. |
| I-9 | Host được ưu tiên | Input từ người đang ngồi tại máy host được ưu tiên hơn mọi viewer từ xa. |
| I-10 | Mỗi lúc một viewer điều khiển | Chỉ một viewer điều khiển mouse và keyboard tại một thời điểm. Khi có tranh chấp, viewer tham gia sớm hơn được ưu tiên; input của các viewer khác bị bỏ qua cho tới khi viewer đang điều khiển không thao tác trong **1 giây**. |
| I-11 | Bắt buộc view-only | Khi host đã tắt control, hoặc viewer chọn chỉ xem, không input nào tới được host và cửa sổ viewer hiển thị trạng thái view-only. |

## 9. Kiểm soát truy cập và an toàn

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| S-1 | Encrypt | Session chạy trên một transport đã encrypt (QUIC/TLS). Mọi dữ liệu một session mang theo — video, control, input, clipboard và lưu lượng terminal — đều được encrypt giữa hai máy. Host không trả lời bất cứ thứ gì ở dạng không encrypt: mọi packet chưa encrypt gửi tới port đều bị loại bỏ, và host không tiết lộ gì — kể cả nội dung đang share — trước khi client authenticate. [`SECURITY.vi.md`](../SECURITY.vi.md) mô tả đầy đủ. |
| S-2 | Client được phép kiểm soát việc chấp nhận | Quyền truy cập hoạt động như SSH. Client chứng minh nó giữ nửa private của key của máy mình; host chỉ chấp nhận khi public key đó nằm trong danh sách client được phép. Một key vào được danh sách đó theo một trong ba cách, mỗi cách đều là hành động có chủ ý của chủ host: họ bấm **Approve** trên yêu cầu kết nối của thiết bị (H-18); thiết bị scan hoặc dán mã QR mà chủ host hiển thị trong lúc share (C-12), với token dùng một lần thay cho cú click; hoặc chủ host dán public key của thiết bị (copy trên thiết bị bằng *Copy public key*) trên trang Devices hoặc bằng CLI (`access add --stdin`). Không có passcode và không có công tắc nào cho máy lạ vào: không ai vào được nếu không có token còn hiệu lực hoặc một lần Approve. Token dùng được một lần, trong 5 phút, và bị thu hồi khi mã bị ẩn hoặc share dừng; token sai được tính như một chữ ký sai (S-4). Khi danh sách client được phép trống, không ai vào được. Nhãn của key chỉ là tên hiển thị, không bao giờ là quyền. |
| S-3 | Trang Devices | Trang **Devices** là một danh sách gồm bốn mục, theo thứ tự sau. **This machine**: tên thiết bị (S-11 — nhãn mà các máy khác nhìn thấy) và fingerprint `SHA256:` của máy kèm nút *Copy*, để người connect đối chiếu, cùng **Copy public key**, copy dòng mà chủ của một host khác có thể dán để cho phép máy này; cùng key đó là thứ máy này dùng để đăng nhập khi làm client. **Connection requests**: cùng danh sách như trên trang Host (H-18), kèm *Approve* và *Deny*, nên có thể trả lời một yêu cầu ở đây ngay cả khi không share gì; danh sách được làm mới mỗi giây khi trang đang mở. **Devices allowed to connect to this machine**: mỗi thiết bị hiển thị nhãn và fingerprint kèm *Remove*, cùng *Allow* để dán public key mới và *Remove every client*. Gỡ một thiết bị sẽ ngắt ngay các session đang chạy của nó. **Trusted hosts**: mỗi host hiển thị tên, địa chỉ gần nhất nó trả lời và fingerprint key, kèm *Connect* và *Remove*. Không còn chia host/client và không có danh sách key nào để quản lý: key duy nhất của máy phục vụ cả hai vai (S-9). |
| S-4 | Khoá sau nhiều lần thất bại | **3** chữ ký sai từ một client key và một địa chỉ trong vòng một phút sẽ chặn key đó từ địa chỉ đó trong **10 giây**; token QR sai được tính vào cùng giới hạn đó cho địa chỉ nó đến từ, nên không thể đoán token. Host giữ tối đa **8** kết nối đang chờ authenticate, và loại bỏ kết nối nào chưa authenticate xong trong **10 giây**. Nó cũng giữ tối đa **16** yêu cầu kết nối, mỗi yêu cầu trong **10 phút** (H-18). Các client khác không bị ảnh hưởng. |
| S-5 | Công tắc control | Host có thể share với *viewers can control this machine* ở trạng thái tắt, khiến mọi session trở thành view-only bất kể viewer yêu cầu gì. |
| S-6 | Đồng ý cho capture | Trên các nền tảng yêu cầu, hệ thống sử dụng chính hộp thoại permission và hộp chọn màn hình của hệ điều hành; Deskhub không capture được nếu người dùng không cấp quyền. |
| S-7 | Chỉ share khi được yêu cầu | Không nội dung nào được share cho tới khi người dùng bắt đầu một phiên share. Việc đóng app hoặc dừng share sẽ kết thúc mọi session. |
| S-8 | Host đã trust đi theo key | Client ghim key của mọi host mà nó trust — ở lần connect đầu tiên sau khi người dùng xác nhận fingerprint (C-2), hoặc từ mã QR của host (C-12) — và kiểm tra key đó trước khi gửi bất cứ thứ gì. Trust thuộc về key, không thuộc về địa chỉ: host đã trust chuyển sang địa chỉ mới vẫn được nhận ra ở đó và connect mà không có hộp thoại nào, và *Trusted hosts* ghi nhớ địa chỉ mới làm địa chỉ gần nhất của nó. Một key khác ở địa chỉ đã biết chỉ đơn giản là một host mà máy này chưa từng gặp: hộp thoại *New host* xuất hiện như với mọi lần connect đầu tiên, kèm cảnh báo rằng địa chỉ đó từng thuộc về một host đã trust khác, host này vẫn nằm trong danh sách cho tới khi người dùng gỡ nó. CLI từ chối host mà nó chưa biết, in fingerprint của host đó, và chỉ lưu khi được chạy lại với `--accept-new-host-key`; host cũng có thể được ghim trước (`host add ALIAS --address IP[:PORT] --host-key-stdin`), và `trust forget` nhận một fingerprint, một alias hoặc một địa chỉ gần nhất. |
| S-9 | Key của máy | Mỗi máy có đúng một key, được tạo ở lần chạy đầu tiên và không bao giờ tự thay; nó là danh tính ở cả hai vai và không có gì để tạo, import hay chọn. *Copy public key* trên trang Devices (CLI: `key public`) copy một dòng, `ecdsa-sha2-nistp256 AAAA… <device name>`, gắn nhãn bằng tên thiết bị của máy này (T-27); dòng đó là thứ chủ host dán để cho phép máy này, và là thứ host lưu khi approve yêu cầu kết nối của máy này hoặc cho nó vào bằng mã QR. Private key không bao giờ rời khỏi máy. Xoá file key sẽ cho máy một danh tính mới: các host đã cho phép nó phải cho phép lại, và các client đã trust nó sẽ thấy lại hộp thoại *New host*. |
| S-10 | Không mang quyền truy cập cũ sang | Các thiết lập truy cập từ phiên bản cũ không được chuyển đổi. Khi lên từ 7.0.x, mọi client phải được cho vào thêm một lần — bằng approve hoặc mã QR, hoặc bằng dán key của nó — vì key mà client dùng để đăng nhập giờ là key của máy; các file `client_key*.pem` và `host_cert.pem` còn sót lại bị bỏ qua, không bao giờ được đọc hay migrate. Phiên bản cũ không connect được tới phiên bản này và được thông báo rằng phiên bản không khớp: 7.0.x và 7.1 không nói chuyện được với nhau, nên hãy cập nhật mọi thiết bị. |

## 10. Settings

Settings áp dụng theo từng máy, được lưu qua các lần khởi động lại, và có hiệu lực từ lần
share tiếp theo. Điện thoại và tablet chỉ hiển thị tên thiết bị (T-26), network port (T-4),
clipboard sync (T-17) và keep awake (T-19), thêm network dùng để share (T-9) trên màn hình
share. Các thiết lập còn lại sử dụng
giá trị mặc định dựng sẵn.

Trên desktop, trang Settings chia chúng thành ba khung: **Host** cho những gì chỉ việc share
dùng tới (T-1 – T-3, T-6, T-11, T-22 và thư mục của T-25), **Client** cho những gì chỉ
việc xem dùng tới (T-23), và **General** cho những gì cả hai phía đều dùng (T-26, T-4, T-13,
T-15, T-17, T-19).

| ID | Setting | Khoảng giá trị | Mặc định |
| --- | --- | --- | --- |
| T-1 | Frame rate | 1 – 240 fps | 60 |
| T-2 | Bitrate | 1 – 1000 Mbps | 20 |
| T-3 | Quality | 720p · 1080p · 1440p · Native | 1080p |
| T-4 | Network port | 1 – 65535 | 47777 |
| T-6 | Viewer được control máy này | bật / tắt | bật |
| T-9 | Share on network | All networks · một trong các địa chỉ của máy này | All networks |
| T-11 | Bắt đầu share khi mở app | bật / tắt | tắt |
| T-13 | Khởi động Deskhub khi đăng nhập | bật / tắt | tắt |
| T-15 | Tiếp tục chạy dưới nền | bật / tắt | tắt |
| T-17 | Sync clipboard dạng văn bản | bật / tắt | tắt |
| T-19 | Giữ thiết bị này không sleep trong session | bật / tắt | bật |
| T-22 | Share âm thanh của thiết bị này cho viewer | bật / tắt | bật |
| T-23 | Phát âm thanh của thiết bị đang xem | bật / tắt | bật |
| T-26 | Tên thiết bị | tối đa 64 byte văn bản | trống — tên mà hệ điều hành đặt cho máy này |

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| T-7 | Quality tự động | Quality của stream tự thích ứng theo dung lượng network hiện có, trong giới hạn đã cấu hình; người dùng không phải thao tác khi điều kiện thay đổi. |
| T-8 | Kiểm tra giá trị | Giá trị ngoài khoảng cho phép hoặc không phải số bị từ chối và giá trị trước đó được giữ lại, thay vì được áp dụng. |
| T-10 | Dự phòng network | Khi một network cụ thể được chọn (T-9), host chỉ truy cập được qua địa chỉ đó. Nếu địa chỉ này không còn tồn tại khi bắt đầu share, host chuyển sang share trên mọi network và nêu rõ điều đó trong status. Một địa chỉ đã lưu nhưng hiện không khả dụng vẫn được liệt kê, kèm chú thích *not connected*. |
| T-12 | Tự động share khi khởi động | Chỉ desktop. Khi T-11 bật, mở app sẽ chuyển thẳng tới trang Host và bắt đầu share với settings đã lưu, tương đương với việc người dùng nhấn Share. Khi app được khởi động lúc đăng nhập (T-13), desktop có thể chưa có display nào; khi đó việc share sẽ chờ, kiểm tra lại mỗi nửa giây trong tối đa 30 giây, và bắt đầu ngay khi có display xuất hiện. Trong thời gian chờ, trang Host hiển thị trạng thái đang chờ. Nếu không có display nào xuất hiện, app share các nội dung khác đang được chọn (terminal) hoặc dừng lại kèm lý do trên trang Host. Một phiên share tự động không bao giờ mở hộp thoại, vì tại thời điểm đăng nhập cửa sổ có thể đang ẩn trong tray và người dùng không nhìn thấy. Các quy tắc của từng nền tảng vẫn áp dụng: Linux hiển thị hộp thoại chia sẻ màn hình của desktop trong lần đầu rồi dùng lại lựa chọn đã lưu (P-3), còn macOS vẫn yêu cầu các permission tương ứng (P-2). |
| T-14 | Khởi động khi đăng nhập | Chỉ desktop. Khi T-13 bật: Linux ghi một mục autostart vào `~/.config/autostart`; Windows đăng ký một scheduled task tên *Deskhub* khởi động app ở quyền cao khi đăng nhập, nên không xuất hiện prompt UAC; macOS đăng ký một Login Item mà người dùng cũng thấy trong System Settings. Khi tắt, mục tương ứng bị gỡ bỏ. Checkbox luôn hiển thị trạng thái do hệ điều hành báo về, không chỉ là giá trị đã lưu lần cuối. |
| T-16 | Chế độ nền | Chỉ desktop. Khi T-15 bật, một icon xuất hiện ở tray hoặc menu bar với các mục *Show/Hide window*, *Start/Stop sharing* và *Quit*; đóng cửa sổ sẽ ẩn app thay vì thoát, và việc share tiếp tục dưới nền. Cửa sổ luôn hiển thị khi khởi động và chỉ ẩn khi người dùng đóng nó, nên kết hợp T-13, T-11 và T-15 sẽ bắt đầu share khi đăng nhập với cửa sổ hiển thị cho tới khi được đóng. Trên Windows, nhấn chuột trái vào icon tray sẽ hiển thị hoặc ẩn cửa sổ. Trên macOS, icon Dock biến mất khi cửa sổ đang ẩn. Trên Linux, tray cần một StatusNotifier host (có sẵn trên KDE; GNOME cần extension AppIndicator); nếu không có, đóng cửa sổ vẫn là thoát app, để app không rơi vào trạng thái không thể truy cập. Trên Windows và Linux, khi đang share, đóng cửa sổ luôn thu về tray kể cả khi T-15 tắt (nếu có tray), để không ngắt các viewer đang kết nối. Trên macOS, đóng cửa sổ không thoát app, nên việc share tiếp tục trong mọi trường hợp. |
| T-18 | Sync clipboard | Khi T-17 bật, văn bản thuần được copy trên bất kỳ máy nào trong session sẽ xuất hiện trên các máy còn lại trong vài giây, theo cả hai chiều; host chuyển tiếp nội dung copy của một viewer tới các viewer khác. Văn bản bị giới hạn ở 32 KiB (phần dài hơn bị cắt tại ranh giới một ký tự trọn vẹn); ảnh, file và định dạng không được truyền. Công tắc của host chi phối toàn bộ session: khi tắt, host bỏ qua và không gửi dữ liệu clipboard. Mỗi máy cũng cần bật công tắc của chính nó để đọc hoặc ghi clipboard cục bộ. Trên Android và iOS, hệ điều hành có các giới hạn riêng: thiết bị Android chỉ nhận được nội dung copy của chính nó khi Deskhub đang ở tiền cảnh, dù văn bản nhận từ ngoài luôn được áp dụng; viewer trên iOS có thể thấy prompt dán của hệ thống khi Deskhub đọc nội dung copy mới; thiết bị iOS đang host không tham gia, vì broadcast của nó chạy trong một process riêng không có quyền truy cập clipboard. |
| T-20 | Giữ thiết bị không sleep | Khi T-19 bật, máy không chuyển sang sleep và display không tắt trong khi đang share hoặc đang xem; hạn chế này được gỡ bỏ ngay khi session kết thúc, và không thiết lập sleep nào của hệ thống bị thay đổi. Trên Windows, macOS và Linux, điều này áp dụng cho cả display sleep và system sleep, với cả host lẫn viewer (trên Linux cần systemd-logind và một desktop tuân thủ interface screensaver của freedesktop, có sẵn trên KDE và GNOME). Hệ điều hành vẫn được ưu tiên ở những trường hợp bắt buộc: gập nắp laptop, nhấn nút nguồn, hoặc macOS chạy bằng pin vẫn có thể đưa máy vào sleep. Trên Android và iOS, công tắc này giữ màn hình sáng khi đang xem một stream; việc share từ điện thoại vốn đã hoạt động khi màn hình tắt (P-5), nên ở vai host công tắc không giữ màn hình. |
| T-25 | Nơi lưu file nhận được | Chỉ desktop. File do viewer gửi được ghi vào một thư mục do máy này chọn, mặc định là `Deskhub` trong thư mục home của người dùng. Thư mục đã chọn được ghi trên dòng *File transfer* của bảng trực tiếp khi đang share (H-17), kèm nút **Open folder** bên cạnh — không hiển thị trước khi share, vì chưa có tác dụng gì cho tới lúc đó. Thư mục được tạo nếu chưa tồn tại, và lựa chọn được lưu cùng các settings khác. Không có nội dung nào được ghi ra ngoài thư mục đó: tên do bên gửi cung cấp bị cắt còn phần cuối của đường dẫn và loại bỏ mọi ký tự mà filesystem cục bộ không lưu được. |
| T-27 | Tên thiết bị | Settings → General → *Device name* đặt tên cho máy này ở mọi nơi: viewer thấy tên khi máy làm host (H-7), các client được máy cho vào thấy tên trong danh sách gần đây của chúng (D-5), host thấy tên khi máy connect (C-7), và tên được dùng làm nhãn cho public key máy copy cũng như cho key mà host lưu khi approve thiết bị này hoặc cho nó vào bằng mã QR (S-9). Khi để trống, tên mà hệ điều hành đặt cho máy được sử dụng: hostname trên Windows và Linux, tên máy trên macOS, tên thiết bị trên iOS, model thiết bị trên Android. Các ký tự điều khiển bị loại bỏ. |
| T-24 | Nội dung âm thanh được share | Khi T-22 bật, host share nội dung mà loa của chính máy đang phát, tức bản mix của mọi ứng dụng trên máy đó. Deskhub không capture microphone và không có audio hai chiều. Android là nền tảng duy nhất có liên quan tới một permission: API playback-capture của nó nằm sau permission mà hệ thống gọi là *Microphone*, được app xin khi bắt đầu share và không dùng cho mục đích nào khác; nếu từ chối, phiên share vẫn tiếp tục nhưng không có âm thanh. Không nền tảng nào khác xin permission microphone. Viewer chỉ nhận âm thanh nếu đã bật tuỳ chọn tương ứng (T-23), nên host bật T-22 cũng không gửi dữ liệu tới viewer không nghe; cả hai công tắc có hiệu lực từ session tiếp theo. |

## 11. Trạng thái và xử lý sự cố

| ID | Tính năng | Mô tả |
| --- | --- | --- |
| G-1 | Thống kê host trực tiếp | Số liệu theo từng display và từng viewer: capture rate, send rate, băng thông và round-trip time. |
| G-2 | Thống kê client trực tiếp | Theo từng session: frame rate, băng thông, round-trip time và latency end-to-end. |
| G-3 | Session log | Trên Windows, macOS và Linux, mỗi lần chạy ghi một file log vào thư mục Deskhub của người dùng, phục vụ việc đính kèm khi báo lỗi. Android và iOS ghi thông tin chẩn đoán vào luồng log của hệ điều hành và không để lại file. |
| G-4 | Phiên bản và liên kết dự án | App hiển thị phiên bản và liên kết tới trang dự án. |

## 12. Hành vi riêng theo nền tảng

| ID | Nền tảng | Hành vi |
| --- | --- | --- |
| P-1 | Windows | App xin quyền administrator một lần khi khởi động; đây là điều kiện để nó nhập được vào các cửa sổ chạy ở quyền cao. App tự thêm rule firewall khi bắt đầu share. |
| P-2 | macOS | Hiển thị panel **Permissions** với trạng thái cấp quyền hiện thời của *Screen Recording* (cần để share) và *Accessibility* (cần để nhận remote input), nút xin từng quyền, và lối tắt sang System Settings. Một số phím bị macOS chặn âm thầm nếu chưa cấp Accessibility. |
| P-3 | Linux | Trang Host liệt kê các display của máy này cùng terminal dưới dạng ô chọn, giống các nền tảng desktop khác, và chỉ những display được chọn mới được share. Sau khi nhấn Share, desktop vẫn xác nhận việc screen capture bằng hộp thoại chia sẻ màn hình của riêng nó; xác nhận này được lưu ở những desktop có hỗ trợ (ScreenCast portal phiên bản 4 trở lên), nên các lần share sau dùng lại một cách âm thầm, kể cả sau khi khởi động lại, và hộp thoại chỉ xuất hiện trong lần đầu. Nếu nội dung desktop cấp không khớp với danh sách display đã chọn, xác nhận đã lưu bị xoá và hộp thoại xuất hiện lại để người dùng cấp đúng display. Nếu desktop từ chối hoặc xác nhận đã hết hạn — sau khi nâng cấp compositor hoặc thay đổi màn hình — hộp thoại xuất hiện lại, và việc huỷ hộp thoại không kích hoạt thử lại. Nếu desktop cấp những display mà app không khớp được với danh sách đã chọn, toàn bộ nội dung desktop cấp sẽ được share thay vì không share gì. Khi chỉ chọn terminal, hộp thoại của desktop được bỏ qua hoàn toàn. Ngoài ra, việc share yêu cầu hệ thống cho phép inject input. |
| P-4 | Android / iOS | Khi làm host, chế độ hoạt động là **view-only**: thiết bị stream màn hình và loại bỏ mọi packet control, vì cả hai OS đều không cho phép app inject input ở phạm vi toàn hệ thống. Thiết bị không share terminal và thông báo điều này khi được truy vấn, nên không client nào mở cửa sổ terminal với điện thoại (C-8); các client desktop nhờ đó thông báo được rằng tuỳ chọn control sẽ không có tác dụng (C-3). Thiết bị nhận file ngay khi app hiển thị trên màn hình, theo các quy tắc batch ở H-16, không cần bật công tắc nào, và tiếp tục nhận trong khi màn hình đang được share, nên một viewer có thể vừa xem màn hình vừa gửi file. Trên iOS, broadcast extension giữ một port duy nhất trong suốt phiên broadcast và phục vụ cả hai chức năng từ đó. Ảnh và video nhận được sẽ được thêm vào thư viện ảnh của thiết bị: trên iOS thông qua permission Photos dạng chỉ-thêm của hệ thống, được xin trong lần đầu có file tới và không cấp cho Deskhub quyền đọc; nếu bị từ chối, file được lưu vào Documents. Trên Android, ảnh và video vào `Pictures/Deskhub` và `Movies/Deskhub` qua media store của hệ thống. Các file khác được lưu vào nơi trình duyệt file của hệ thống truy cập được (thư mục Documents của app trên iOS, `Download/Deskhub` trên Android), kèm một notification nêu tên file vừa tới. Media store của Android dùng ở đây yêu cầu Android 10: trên Android 9 trở xuống, file tới nơi được lưu trong thư mục riêng của app, không xuất hiện trong gallery và trong Downloads. Toàn bộ màn hình được share như một source duy nhất, nên phần chọn display, share nhiều display và dừng từng display (H-1, H-2, H-3, H-5) không áp dụng. Trong khi làm host, điện thoại hiển thị cùng nút **Show QR code** (H-6) và danh sách **Connection requests** kèm *Approve* / *Deny* (H-18) như desktop. Khi xoay thiết bị, stream xoay theo: nội dung viewer nhìn thấy luôn đúng chiều, và cửa sổ của họ điều chỉnh theo hình dạng mới (V-1). Giao diện session ưu tiên thao tác cảm ứng: cử chỉ trackpad, điều khiển zoom, thanh hotkey, keyboard trên màn hình, nút chuyển display, và một nút đóng ở góc dùng chung với màn hình shell và file transfer. |
| P-5 | Android | Việc share yêu cầu hộp thoại đồng ý ghi màn hình của hệ thống, được cấp theo từng phiên share và không thể lưu lại. Share âm thanh cần thêm permission mà Android gọi là *Microphone*, do API playback-capture nằm sau permission này; permission được xin khi bắt đầu share, và nếu bị từ chối thì màn hình vẫn được share nhưng không có âm thanh. Trong khi share, một notification thường trực được hiển thị và stream tiếp tục khi app chuyển xuống nền hoặc màn hình tắt. Dừng share từ notification của hệ thống sẽ kết thúc session. Permission `CAMERA` chỉ được xin khi người dùng bấm *Scan QR code* (C-12) và không dùng cho việc gì khác; khung hình được giải mã trên thiết bị. |
| P-6 | iOS | Việc share được bắt đầu từ nút **Start sharing** trong app; nút này mở broadcast sheet của hệ thống, vì iOS yêu cầu mọi phiên broadcast phải được xác nhận qua đó. Phiên broadcast chạy trong một process riêng nên tiếp tục sau khi app đóng. Màn hình share báo số viewer đang kết nối, kèm tên của những viewer đã đặt tên (C-7), và mức bộ nhớ hiện thời của process broadcast, do iOS kết thúc broadcast vượt quá giới hạn bộ nhớ. Màn hình này không có bảng theo từng viewer như H-7, và không hỗ trợ ngắt kết nối từng viewer (H-8). Nó có hiển thị mã QR (H-6) và các yêu cầu kết nối (H-18): process broadcast ghi từng yêu cầu vào thư mục hai process dùng chung, app liệt kê nó, và một lần *Approve* trong app là thứ process broadcast đọc ở lần thử tiếp theo của thiết bị. Một sự kiện hệ thống làm kết thúc broadcast, chẳng hạn cuộc gọi đến, sẽ kết thúc session. Một session đang mở khi app rời khỏi màn hình — stream, shell hoặc phiên truyền file — được giữ trong khoảng thời gian iOS cho phép với một app đã rời màn hình, khoảng nửa phút, nên việc chuyển sang app khác trong thời gian ngắn không làm mất session. Quá khoảng thời gian đó, hệ thống tạm dừng app và session tự reattach khi app quay lại (V-8). Permission camera chỉ được xin khi người dùng bấm *Scan QR code* (C-12) và không dùng cho việc gì khác. |

## 13. Nằm ngoài phạm vi

Deskhub **không** cung cấp, và đặc tả này không đề cập:

- Capture microphone, audio hai chiều, hay bất kỳ channel thoại nào. Âm thanh chỉ truyền
  một chiều, từ máy được share tới những người đang xem (V-6).
- In từ xa.
- Sync clipboard ngoài văn bản thuần (ảnh, file, văn bản có định dạng).
- Bất kỳ hệ thống tài khoản, thư mục người dùng hay presence nào, và bất kỳ lời mời nào đi
  qua một dịch vụ: mã QR mà host hiển thị (C-12) được đọc từ màn hình của nó hoặc dán bằng
  tay, và không có gì chuyển nó giữa các máy thay cho người dùng.
- Dịch vụ discovery, relay, rendezvous hay NAT-traversal. Việc truy cập một host qua
  internet thuộc trách nhiệm của người dùng, ví dụ thông qua VPN.
- Ghi lại session.
- Truy cập khi không có người tại máy, wake-on-LAN, hay điều khiển nguồn từ xa.
- Quản trị nhiều người dùng, phân quyền theo vai trò, hay audit trail.
