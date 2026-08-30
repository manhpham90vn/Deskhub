[English](INSTALL.md) · **Tiếng Việt** · [中文](INSTALL.zh.md) · [日本語](INSTALL.ja.md)

# Deskhub — Cài đặt

Toàn bộ các nền tảng gói gọn trong một trang. Không phần nào ở đây cần tải mã nguồn về —
mọi bản phát hành đều đã được build sẵn. Muốn tự biên dịch thì xem
[`BUILD.vi.md`](BUILD.vi.md).

Đây là bản dịch của [`INSTALL.md`](INSTALL.md); khi hai bản khác nhau, bản tiếng Anh là
bản chuẩn.

Mọi file tải về đều nằm ở
[trang Releases](https://github.com/manhpham90vn/Deskhub/releases); riêng hai bản di động
được phát hành qua TestFlight và Google Play.

| Nền tảng | File | Cài trong một dòng |
| --- | --- | --- |
| 🪟 Windows | `deskhub-v*-windows.exe` | Tải về rồi chạy |
| 🍎 macOS | `deskhub-v*-macos.dmg` | Mở file dmg, kéo app sang |
| 🐧 Ubuntu, Kubuntu, Debian, Mint | `deskhub-v*-amd64.deb` | `sudo apt install ./deskhub-v*-amd64.deb` |
| 🐧 Fedora (Workstation & bản KDE) | `deskhub-v*-x86_64.rpm` | `sudo dnf install ./deskhub-v*-x86_64.rpm` |
| 🐧 openSUSE | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Arch và các bản khác | `deskhub-v*-linux-x86_64` | `chmod +x deskhub-v*-linux-x86_64 && ./deskhub-v*-linux-x86_64` |
| 🤖 Android | `deskhub-v*-android.apk` | Cài file apk, hoặc tham gia bản beta trên Play |
| 📱 iOS | — | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

Ngoài ra còn có `deskhub-cli`, vẫn là client đó nhưng không có cửa sổ riêng — xem
[Dòng lệnh](#-dòng-lệnh).

---

## 🪟 Windows

Tải `deskhub-v*-windows.exe` về rồi chạy. Không có trình cài đặt, không dịch vụ chạy nền,
không tài khoản — toàn bộ app chính là một file đó.

Lần đầu dùng sẽ có hai chuyện xảy ra:

- **Xin quyền quản trị, một lần lúc khởi động.** Không có quyền này thì không bơm được
  chuột và bàn phím vào các cửa sổ chạy ở quyền cao.
- **Một luật Windows Firewall**, do chính app thêm vào ở lần chia sẻ đầu tiên.

Muốn gỡ Deskhub thì xoá file exe. Cấu hình và khoá vẫn nằm ở `%USERPROFILE%\.deskhub` cho
tới khi bạn xoá cả thư mục đó.

## 🍎 macOS

Tải `deskhub-v*-macos.dmg`, mở ra, kéo app vào *Applications*. File dmg được ký bằng
Developer ID và đã qua notarize của Apple, nên mở lên không bị Gatekeeper cảnh báo.

Để chia sẻ màn hình cần hai quyền của macOS, cả hai đều được xin ngay từ trang
**Settings** trong app — nơi cũng hiển thị trạng thái hiện thời của chúng kèm nút mở thẳng
đúng mục trong System Settings:

| Quyền | Dùng để |
| --- | --- |
| **Screen Recording** | Thu hình màn hình của máy Mac này |
| **Accessibility** | Cho người xem điều khiển chuột và bàn phím của máy Mac này |

Chỉ xem máy khác thì không cần quyền nào cả.

## 🐧 Linux

**Muốn kết nối và xem thì cài xong là đủ.** App chỉ liên kết tới GTK3, PipeWire và libva —
những thứ mọi desktop mặc định đều có sẵn; bộ giải mã H.264 được biên dịch thẳng vào
trong, nên không dính tới gói FFmpeg nào.

File deb và file rpm có nội dung y hệt nhau — chọn cái nào trình quản lý gói của bạn hiểu.
Cả hai đều kèm luật udev cho `/dev/uinput` nói ở yêu cầu 3 bên dưới, nên điều khiển từ xa
chạy được ngay sau khi cài, không cần đổi nhóm người dùng, không cần đăng nhập lại. Bản
chạy thẳng chạy được trên mọi bản phân phối x86_64 có glibc 2.35+ (Ubuntu 22.04, Fedora
36, openSUSE 15.5, Arch hiện hành).

File deb và rpm cũng cài kèm `deskhub-cli` vào `/usr/bin/deskhub-cli` — xem
[Dòng lệnh](#-dòng-lệnh) bên dưới.

**Để chia sẻ màn hình của máy này**, cần thêm ba thứ nữa.

### 1. Một portal thu hình màn hình

Deskhub luôn thu hình qua `xdg-desktop-portal` — đó chính là thứ hiện hộp thoại "chia sẻ
màn hình nào?". Lựa chọn của bạn ở đó được nhớ lại, nên hộp thoại chỉ xuất hiện ở lần chia
sẻ đầu tiên; nút *Choose screens again* trên trang Host gọi nó ra lại khi bạn muốn đổi màn
hình.

GNOME và KDE có sẵn portal backend trên mọi bản phân phối lớn — **không cần làm gì** trên
Ubuntu, Kubuntu, Fedora Workstation, Fedora KDE, openSUSE hay Arch với GNOME/KDE. Các
trình quản lý cửa sổ độc lập thì cần cài:

```bash
sudo apt install xdg-desktop-portal-wlr      # sway / river / Wayfire trên họ Debian
sudo dnf install xdg-desktop-portal-wlr      # …trên Fedora
sudo pacman -S xdg-desktop-portal-wlr        # …trên Arch
```

sway, river và Wayfire là các compositor Wayland dựng trên thư viện **wlroots**; khác với
GNOME/KDE, chúng không kèm portal backend riêng, và `-wlr` là backend hiện thực việc thu
hình màn hình cho cả ba. Hyprland có backend riêng là `xdg-desktop-portal-hyprland`.

### 2. Một driver VA-API

H.264 được mã hoá trên GPU; không có đường lui bằng phần mềm.

```bash
# Ubuntu / Debian / Mint
sudo apt install va-driver-all vainfo        # NVIDIA cần thêm: nvidia-vaapi-driver

# Fedora — Mesa bản gốc tắt H.264; driver dùng được nằm trong RPM Fusion:
sudo dnf install libva-utils
sudo dnf install mesa-va-drivers-freeworld   # AMD (RPM Fusion)
sudo dnf install intel-media-driver          # Intel (RPM Fusion)
sudo dnf install nvidia-vaapi-driver         # NVIDIA (RPM Fusion)

# openSUSE
sudo zypper install libva-utils              # kèm driver VA-API của hãng GPU bạn dùng

# Arch
sudo pacman -S libva-utils
sudo pacman -S libva-mesa-driver             # AMD · Intel: intel-media-driver · NVIDIA: libva-nvidia-driver

# rồi trên mọi bản phân phối:
vainfo | grep -E 'H264.*Enc'                 # phải in ra ít nhất 1 dòng, không thì máy này không host được
```

### 3. Quyền ghi vào `/dev/uinput`

Đây là đường để bơm chuột và bàn phím. File deb và rpm tự cài luật udev cho bạn — không
cần làm gì. Với bản chạy thẳng, một lệnh là xong, không cần clone mã nguồn, không cần đăng
nhập lại trên desktop:

```bash
curl -fsSL https://raw.githubusercontent.com/manhpham90vn/Deskhub/main/scripts/setup-uinput.sh | sudo bash
```

Muốn đọc trước khi đưa vào sudo? Tải
[`scripts/setup-uinput.sh`](../scripts/setup-uinput.sh) về xem đã — nó chỉ hơn chục dòng.
Từ một bản mã nguồn thì lệnh tương đương là `make setup-linux-permissions`.

Không có quyền uinput thì app vẫn chạy và vẫn xem được — chỉ là không bơm được chuột hay
bàn phím vào máy này.

### Tường lửa

Nếu bạn có bật tường lửa, hãy mở UDP 47777:

```bash
sudo ufw allow 47777/udp                                  # Ubuntu / Debian / Mint
sudo firewall-cmd --add-port=47777/udp --permanent        # Fedora / openSUSE
```

### Gỡ cài đặt

```bash
sudo apt remove deskhub      # hoặc: sudo dnf remove deskhub / sudo zypper remove deskhub
rm -rf ~/.deskhub            # cấu hình, khoá và danh sách máy đã ghép cặp
```

Bản chạy thẳng chỉ là một file — xoá là xong.

## 🤖 Android

Ở vai host, Android chỉ chia sẻ màn hình để xem và cần **Android 10+**. Vai xem thì chạy
được trên các bản cũ hơn.

**Cài thẳng file apk** — tải `deskhub-v*-android.apk` từ
[Releases](https://github.com/manhpham90vn/Deskhub/releases) rồi cài. File này được ký
bằng đúng khoá của bản trên Google Play.

**Bản beta trên Play** — ba bước, tất cả đều dùng **cùng tài khoản Google** với Play Store
trên điện thoại của bạn:

1. Vào nhóm người thử nghiệm: [groups.google.com/g/deskhub-test](https://groups.google.com/g/deskhub-test)
2. Đăng ký làm người thử nghiệm: [play.google.com/apps/testing/com.manhpham.deskhub](https://play.google.com/apps/testing/com.manhpham.deskhub)
3. Cài đặt (chờ Play đồng bộ vài phút): [play.google.com/store/apps/details?id=com.manhpham.deskhub](https://play.google.com/store/apps/details?id=com.manhpham.deskhub)

Xin hãy giữ bản beta trên máy **từ 14 ngày trở lên** — đó là điều kiện Google đặt ra trước
khi app được phát hành công khai.

## 📱 iOS

File ipa không sideload được, nên bản beta chạy qua TestFlight:

1. Cài [TestFlight](https://apps.apple.com/app/testflight/id899247664).
2. Vào bản beta: **[testflight.apple.com/join/7qY7wgpd](https://testflight.apple.com/join/7qY7wgpd)**

Giống Android, iPhone hay iPad ở vai host chỉ chia sẻ để xem — không hệ điều hành di động
nào cho phép một app bơm thao tác điều khiển vào chính thiết bị nó đang chạy.

---

## 💻 Dòng lệnh

`deskhub-cli` vẫn là client đó nhưng không có cửa sổ riêng: nó chia sẻ màn hình, mở shell
từ xa và điều khiển host từ script hay qua SSH. Chạy `deskhub-cli help` để xem danh sách
lệnh. Mọi thứ nó đọc và ghi — cấu hình, máy đã ghép đôi, khoá host đã tin — đều dùng chung
với app, nên hai bên luôn khớp nhau.

| Nền tảng | File |
| --- | --- |
| 🪟 Windows | `deskhub-cli-v*-windows.exe` — tải về rồi chạy, không cần trình cài đặt |
| 🍎 macOS | `deskhub-cli-v*-macos` — một file chạy được cả Apple Silicon lẫn Intel |
| 🐧 Linux | `deskhub-cli-v*-linux-x86_64`, hoặc đã có sẵn nếu bạn cài bản deb hay rpm |

Hai file macOS và Linux tải về chưa có quyền thực thi, nên `chmod +x` một lần. Cả hai đều
không được ký và công chứng như file dmg: trên macOS lần chạy đầu cần
`xattr -d com.apple.quarantine deskhub-cli-v*-macos`, hoặc bấm *Open Anyway* trong System
Settings → Privacy & Security.

Trên Linux nó chia sẻ màn hình qua đúng portal và driver VA-API mà app cần, và cũng dựa
vào luật `/dev/uinput` đó để điều khiển từ xa, nên mọi thứ nói ở phần [Linux](#-linux) đều
áp dụng cho nó. Trên macOS nó chia sẻ màn hình và mở shell được, nhưng không *xem* được
màn hình máy khác — lệnh `connect` cần một lớp cửa sổ mà bản dòng lệnh không có, và nó báo
rõ như vậy; muốn xem thì dùng app.

---

## 🔒 Trước khi chia sẻ màn hình

Mọi thứ một phiên mang theo — hình ảnh, phím gõ, chuột, clipboard và dữ liệu terminal —
đều chạy trên **QUIC/TLS**, và một máy lạ chỉ vào được qua bước ghép cặp: nó phải chứng
minh mình biết mật mã của host bằng **SPAKE2** (bản thân mã đó không bao giờ truyền đi, và
mỗi kết nối chỉ được đoán đúng một lần), hoặc chờ người ngồi tại host trả lời *Let this
machine in?*.

Mã hoá không đồng nghĩa với an toàn trên Internet. Cổng vẫn trả lời các gói dò tìm, và lần
ghép cặp đầu tiên với một máy bạn chưa từng gặp vẫn là một bước nhảy niềm tin. Hãy ưu tiên
**mạng bạn tin tưởng**, hoặc một **VPN** — cài [Tailscale](https://tailscale.com) trên cả
hai máy rồi kết nối tới địa chỉ `100.x.y.z`. **Đừng bao giờ mở port-forward cho UDP
47777.**

[`SECURITY.vi.md`](../SECURITY.vi.md) có mô hình mối đe doạ đầy đủ, cái gì được bảo vệ và
cái gì không, cùng cách báo lỗ hổng.

## 🆘 Khi có gì đó không chạy

- **Không thấy máy nào để kết nối** — hai máy phải cùng một mạng (hoặc cùng một tailnet
  của Tailscale), và UDP 47777 phải được mở ở phía host.
- **Linux: bấm chia sẻ là hỏng ngay** — chạy `vainfo | grep -E 'H264.*Enc'`; không ra dòng
  nào nghĩa là máy này không có bộ mã hoá H.264 dùng được và không thể làm host.
- **Linux: con trỏ không nhúc nhích** — thiếu luật `/dev/uinput` ở yêu cầu 3.
- **macOS: màn hình đen hoặc điều khiển không ăn** — kiểm tra Screen Recording và
  Accessibility trên trang Settings.
- **Vẫn tắc** — mở một [issue](https://github.com/manhpham90vn/Deskhub/issues) kèm model
  thiết bị, phiên bản hệ điều hành và dòng trạng thái đang hiện trên trang Host hoặc
  Client.
