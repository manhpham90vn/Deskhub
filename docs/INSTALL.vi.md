[English](INSTALL.md) · **Tiếng Việt** · [中文](INSTALL.zh.md) · [日本語](INSTALL.ja.md)

# Deskhub — Cài đặt

Chọn nền tảng của bạn bên dưới để cài bản release đã build sẵn. Bạn không cần checkout
source. Nếu muốn tự compile Deskhub, xem [`BUILD.vi.md`](BUILD.vi.md).

Bộ cài desktop và file APK Android nằm trên
[trang Releases](https://github.com/manhpham90vn/Deskhub/releases). Bản beta mobile cũng
có trên TestFlight và Google Play.

Đây là bản dịch của [`INSTALL.md`](INSTALL.md). Nếu hai bản có khác biệt, bản tiếng Anh là
bản chuẩn.

| Nền tảng | File | Cách cài |
| --- | --- | --- |
| 🪟 Windows | `deskhub-v*-windows-setup.exe` | Tải và cài; mở DeskHub từ Start hoặc Desktop |
| 🍎 macOS | `deskhub-v*-macos.dmg` | Mở dmg rồi kéo Deskhub vào Applications |
| 🐧 Ubuntu, Kubuntu, Debian, Mint | `deskhub-v*-amd64.deb` | `sudo apt install ./deskhub-v*-amd64.deb` |
| 🐧 Fedora (Workstation và KDE spin) | `deskhub-v*-x86_64.rpm` | `sudo dnf install ./deskhub-v*-x86_64.rpm` |
| 🐧 openSUSE | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Arch và các distro khác | `deskhub-v*-linux-x86_64` | `chmod +x deskhub-v*-linux-x86_64 && ./deskhub-v*-linux-x86_64` |
| 🤖 Android | `deskhub-v*-android.apk` | Cài apk, hoặc tham gia bản beta trên Play |
| 📱 iOS | — | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

Bạn cũng có thể dùng `deskhub-cli` cho các lệnh terminal và script. Trên Windows và
Linux, CLI vẫn có thể mở cửa sổ xem màn hình từ xa. Xem [Command line](#-command-line).

Bạn có thể cài qua `winget` trên Windows, Homebrew trên macOS hoặc
[apt repository](#-linux) trên Ubuntu, Kubuntu, Debian và Mint. Khi muốn lên bản mới,
hãy dùng lệnh nâng cấp của package manager tương ứng.

---

## 🪟 Windows

Tải `deskhub-v*-windows-setup.exe` và chạy. Bộ cài thêm DeskHub vào Start Menu và tạo
shortcut trên Desktop theo mặc định; bạn cũng có thể mở app ngay khi cài xong. Không cần
tài khoản hay background service. File `deskhub-v*-windows.exe` vẫn có cho ai muốn chạy portable.

Hoặc để winget cài bộ cài đó và giữ nó luôn mới: `winget upgrade` lấy bản release mới,
`winget uninstall ManhPham.Deskhub` gỡ nó đi:

```powershell
winget install ManhPham.Deskhub
```

Nếu đã cài bản winget portable trước đây, chạy `winget uninstall ManhPham.Deskhub` một
lần rồi cài lại. Settings và key trong `%USERPROFILE%\.deskhub` vẫn được giữ.

Lần sử dụng đầu tiên có hai điểm cần lưu ý:

- **Quyền Administrator, xin một lần khi khởi động.** Không có quyền này thì không thể
  inject mouse và keyboard vào các cửa sổ chạy ở quyền cao.
- **Rule Windows Firewall** do app tự thêm trong lần share đầu tiên.

Gỡ DeskHub trong Windows Settings hoặc chạy `winget uninstall ManhPham.Deskhub`. Với bản
portable, chỉ cần xoá file exe. Settings và key vẫn nằm trong `%USERPROFILE%\.deskhub`
cho đến khi bạn xoá thư mục đó.

## 🍎 macOS

Tải `deskhub-v*-macos.dmg`, mở và kéo app vào *Applications*. File dmg được sign bằng
Developer ID và do Apple notarize, nên mở lên không bị Gatekeeper cảnh báo.

Hoặc cài qua Homebrew, sau đó `brew upgrade` sẽ giữ nó luôn mới:

```bash
brew install --cask manhpham90vn/tap/deskhub
```

Để host màn hình cần hai permission của macOS. Cả hai đều xin được ngay trên trang
**Settings** của app; trang này cũng hiển thị trạng thái hiện thời của chúng, kèm nút mở
thẳng mục tương ứng trong System Settings.

| Permission | Dùng cho |
| --- | --- |
| **Screen Recording** | Capture display của máy Mac này |
| **Accessibility** | Cho phép viewer điều khiển mouse và keyboard của máy Mac này |

Nếu chỉ xem máy khác thì không cần permission nào.

## 🐧 Linux

**Nếu chỉ cần connect và xem, hãy bắt đầu bằng gói app.** H.264 decoder đã nằm trong app,
nên bạn không phải cài thêm gói FFmpeg. Distro của bạn vẫn cần cung cấp các thư viện mà
app sử dụng, gồm GTK3, PipeWire và libva.

Bản deb và rpm có nội dung giống nhau; chọn bản mà package manager của hệ thống hỗ trợ.
Cả hai đều cài udev rule cho `/dev/uinput` mô tả ở mục 3 bên dưới, nên remote input hoạt
động ngay sau khi cài, không cần đổi group và không cần đăng nhập lại. Bản portable chạy
được trên mọi distro x86_64 có glibc 2.35 trở lên (Ubuntu 22.04, Fedora 36,
openSUSE 15.5, Arch bản hiện tại).

CLI có gói deb và rpm riêng. Chỉ cài `deskhub-cli` khi cần lệnh terminal; gói này chạy
độc lập hoặc song song với app. Cả hai đặt lệnh trong `/usr/bin`, còn app desktop thêm
launcher vào menu ứng dụng.

Trên Ubuntu, Kubuntu, Debian và Mint, apt repository của Deskhub cài đúng bản deb đó và
để `sudo apt upgrade` mang về mọi bản release sau này. Repository hỗ trợ Ubuntu 22.04 trở
lên và Debian 12 trở lên:

```bash
sudo install -d /etc/apt/keyrings
curl -fsSL https://manhpham90vn.github.io/Deskhub/apt/deskhub.gpg | sudo tee /etc/apt/keyrings/deskhub.gpg >/dev/null
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/deskhub.gpg] https://manhpham90vn.github.io/Deskhub/apt stable main" \
  | sudo tee /etc/apt/sources.list.d/deskhub.list
sudo apt update && sudo apt install deskhub
```

Cài CLI riêng bằng `sudo apt install deskhub-cli`. Nếu nâng cấp từ bản `deskhub` cũ từng
chứa CLI, hãy cài thêm `deskhub-cli` để giữ lệnh đó.

**Để share màn hình của máy này**, cần thêm ba điều kiện.

### 1. Screen-capture portal

Deskhub luôn capture thông qua `xdg-desktop-portal`. Đây là thành phần hiển thị hộp thoại
chọn màn hình cần share. Lựa chọn được lưu lại, nên hộp thoại chỉ xuất hiện ở lần share
đầu tiên. Để chọn màn hình khác, dùng *Choose screens again* trên trang Host.

GNOME và KDE đã có sẵn portal backend trên mọi distro lớn, nên **không cần làm gì** trên
Ubuntu, Kubuntu, Fedora Workstation, Fedora KDE, openSUSE hay Arch chạy GNOME/KDE. Các
window manager độc lập thì cần cài thêm:

```bash
sudo apt install xdg-desktop-portal-wlr      # sway / river / Wayfire trên họ Debian
sudo dnf install xdg-desktop-portal-wlr      # …trên Fedora
sudo pacman -S xdg-desktop-portal-wlr        # …trên Arch
```

sway, river và Wayfire là các Wayland compositor dựng trên thư viện **wlroots**. Khác với
GNOME/KDE, chúng không đi kèm portal backend, và `-wlr` là backend cung cấp screen capture
cho cả ba. Hyprland có `xdg-desktop-portal-hyprland` riêng.

### 2. VA-API driver

H.264 được encode trên GPU; không có software fallback.

```bash
# Ubuntu / Debian / Mint
sudo apt install va-driver-all vainfo        # NVIDIA cần thêm: nvidia-vaapi-driver

# Fedora — Mesa mặc định tắt H.264; driver dùng được nằm trong RPM Fusion:
sudo dnf install libva-utils
sudo dnf install mesa-va-drivers-freeworld   # AMD (RPM Fusion)
sudo dnf install intel-media-driver          # Intel (RPM Fusion)
sudo dnf install nvidia-vaapi-driver         # NVIDIA (RPM Fusion)

# openSUSE
sudo zypper install libva-utils              # kèm VA-API driver của hãng GPU tương ứng

# Arch
sudo pacman -S libva-utils
sudo pacman -S libva-mesa-driver             # AMD · Intel: intel-media-driver · NVIDIA: libva-nvidia-driver

# sau đó, trên mọi distro:
vainfo | grep -E 'H264.*Enc'                 # phải in ra ít nhất một dòng, nếu không máy này không host được
```

### 3. Quyền ghi vào `/dev/uinput`

Đây là cơ chế inject mouse và keyboard. Bản deb và rpm đã cài sẵn udev rule, không cần
thao tác thêm. Với bản portable, một lệnh là đủ; không cần clone repo và không cần
đăng nhập lại:

```bash
curl -fsSL https://raw.githubusercontent.com/manhpham90vn/Deskhub/main/scripts/setup-uinput.sh | sudo bash
```

Nếu muốn xem nội dung script trước khi chạy bằng sudo, tải
[`scripts/setup-uinput.sh`](../scripts/setup-uinput.sh) về trước; script này chỉ dài hơn
mười dòng. Nếu đã checkout source, lệnh tương đương là
`make setup-linux-permissions`.

Không có quyền uinput thì app vẫn chạy và vẫn xem được, chỉ không inject được mouse hay
keyboard vào máy này.

### Firewall

Nếu đã bật firewall, cần mở UDP 47777:

```bash
sudo ufw allow 47777/udp                                  # Ubuntu / Debian / Mint
sudo firewall-cmd --add-port=47777/udp --permanent        # Fedora / openSUSE
```

### Gỡ cài đặt

```bash
sudo apt remove deskhub      # hoặc: sudo dnf remove deskhub / sudo zypper remove deskhub
rm -rf ~/.deskhub            # settings, key và danh sách máy đã pair
sudo rm -f /etc/apt/sources.list.d/deskhub.list /etc/apt/keyrings/deskhub.gpg   # apt repository, nếu đã thêm
```

Bản portable chỉ gồm một file; xoá file đó là gỡ xong.

## 🤖 Android

Khi làm host, Deskhub chỉ share màn hình ở chế độ view-only và yêu cầu **Android 10+**.
Chức năng xem máy khác vẫn chạy trên các bản Android cũ hơn.

**Cài apk trực tiếp** — tải `deskhub-v*-android.apk` từ
[Releases](https://github.com/manhpham90vn/Deskhub/releases) và cài. File này được sign
bằng cùng key với bản trên Google Play.

**Beta trên Play** — ba bước, đều dùng **cùng tài khoản Google** với Play Store trên thiết
bị:

1. Tham gia nhóm tester: [groups.google.com/g/deskhub-test](https://groups.google.com/g/deskhub-test)
2. Đăng ký làm tester: [play.google.com/apps/testing/com.manhpham.deskhub](https://play.google.com/apps/testing/com.manhpham.deskhub)
3. Cài đặt (Play cần vài phút để sync): [play.google.com/store/apps/details?id=com.manhpham.deskhub](https://play.google.com/store/apps/details?id=com.manhpham.deskhub)

Vui lòng giữ bản beta trên thiết bị **từ 14 ngày trở lên**; đây là điều kiện Google yêu
cầu trước khi app được phát hành công khai.

## 📱 iOS

ipa không sideload được, nên bản beta phát hành qua TestFlight:

1. Cài [TestFlight](https://apps.apple.com/app/testflight/id899247664).
2. Tham gia bản beta: **[testflight.apple.com/join/7qY7wgpd](https://testflight.apple.com/join/7qY7wgpd)**

Tương tự Android, iPhone và iPad chỉ host được ở chế độ view-only: không OS di động nào
cho phép app inject input vào chính thiết bị đang chạy nó.

---

## 💻 Command line

`deskhub-cli` cung cấp lệnh để share màn hình, mở remote shell và chạy từ script hoặc qua
SSH. Trên Windows và Linux, `connect` mở cửa sổ xem màn hình từ xa; trên macOS, hãy dùng
app desktop để xem màn hình. Chạy `deskhub-cli help` để xem danh sách lệnh. CLI và app dùng
chung settings, danh sách máy đã pair và các host key đã trust.

| Nền tảng | File |
| --- | --- |
| 🪟 Windows | `deskhub-cli-v*-windows-setup.exe` — cài `deskhub-cli` vào `PATH` của người dùng |
| 🍎 macOS | `deskhub-cli-v*-macos` — một binary cho cả Apple Silicon và Intel |
| 🐧 Linux | `deskhub-cli-v*-amd64.deb`, `deskhub-cli-v*-x86_64.rpm`, hoặc file portable `deskhub-cli-v*-linux-x86_64` |

Với gói Linux tải trực tiếp, chạy `sudo apt install ./deskhub-cli-v*-amd64.deb` trên
Ubuntu/Debian hoặc `sudo dnf install ./deskhub-cli-v*-x86_64.rpm` trên Fedora. Lệnh được
cài vào `/usr/bin` mà không cần cài app desktop.

Chỉ binary portable trên macOS và Linux có thể cần `chmod +x` sau khi tải về; gói `.deb`
và `.rpm` được cài bằng package manager. Binary macOS portable không được sign và notarize
như file dmg, nên lần chạy đầu tiên có thể cần
`xattr -d com.apple.quarantine deskhub-cli-v*-macos`, hoặc chọn *Open Anyway* trong System
Settings → Privacy & Security.

Bộ cài Windows thêm CLI vào `PATH` của người dùng mà không cần quyền Administrator.
File `deskhub-cli-v*-windows.exe` vẫn có cho ai muốn chạy portable. Cài qua package
manager cũng đưa lệnh vào `PATH`:
`winget install ManhPham.DeskhubCLI` trên Windows, `brew install manhpham90vn/tap/deskhub-cli`
trên macOS — Homebrew cài mà không gắn cờ quarantine. Trên Ubuntu hoặc Debian, sau khi thêm
apt repository ở trên, chạy `sudo apt install deskhub-cli`.

Trên Windows, sau khi cài bằng winget hãy mở cửa sổ PowerShell mới và chạy
`deskhub-cli help`. Chạy `Get-Command deskhub-cli` để xem Windows tìm thấy file nào.
File exe portable tải trực tiếp không tự thêm vào `PATH`.

Trên Linux, `deskhub-cli` share màn hình qua cùng portal và VA-API driver mà app cần, đồng
thời dựa vào cùng rule `/dev/uinput` cho remote input, nên toàn bộ mục [Linux](#-linux) áp
dụng cho nó. Trên macOS, nó share được màn hình và mở được shell nhưng không xem được màn
hình máy khác: `connect` cần một window layer mà bản command line không có, và sẽ báo lỗi
tương ứng. Trường hợp này cần dùng app.

---

## 🔒 Trước khi share màn hình

Mọi dữ liệu một session mang theo — video, phím gõ, mouse, clipboard và lưu lượng terminal
— đều chạy trên **QUIC/TLS**. Một máy lạ chỉ được chấp nhận qua pairing handshake: nó phải
chứng minh được mình biết passcode của host thông qua **SPAKE2** (passcode không bao giờ
được truyền đi, và mỗi connection chỉ được thử một lần), hoặc chờ người dùng tại host trả
lời *Let this machine in?*.

Hãy dùng Deskhub trên **network tin cậy** hoặc qua **VPN**. **Không port-forward UDP
47777.** Encrypt giúp bảo vệ nội dung session, nhưng port vẫn trả lời các probe discovery;
lần pair đầu tiên cũng chưa có danh tính đã biết để đối chiếu. Để truy cập từ xa, bạn có
thể cài [Tailscale](https://tailscale.com) trên cả hai máy và connect tới địa chỉ `100.x.y.z`.

[`SECURITY.vi.md`](../SECURITY.vi.md) mô tả đầy đủ threat model, phạm vi được bảo vệ và
cách báo lỗ hổng.

## 🆘 Khi gặp sự cố

- **Không tìm thấy máy nào để connect** — hai máy phải nằm trên cùng network (hoặc cùng
  tailnet Tailscale), và UDP 47777 phải được mở ở phía host.
- **Linux: share thất bại ngay lập tức** — chạy `vainfo | grep -E 'H264.*Enc'`. Kết quả
  rỗng nghĩa là máy này không có H.264 encoder dùng được và không thể host.
- **Linux: con trỏ không di chuyển** — thiếu rule `/dev/uinput` ở mục 3.
- **macOS: màn hình đen hoặc input không hoạt động** — kiểm tra Screen Recording và
  Accessibility trên trang Settings.
- **Các trường hợp khác** — mở một [issue](https://github.com/manhpham90vn/Deskhub/issues)
  kèm model thiết bị, phiên bản OS và nội dung dòng status trên trang Host hoặc Client.
