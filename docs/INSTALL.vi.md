[English](INSTALL.md) · **Tiếng Việt** · [中文](INSTALL.zh.md) · [日本語](INSTALL.ja.md)

# Deskhub — Cài đặt

Chọn nền tảng của bạn bên dưới để cài bản release đã build sẵn. Bạn không cần checkout
source. Nếu muốn tự compile Deskhub, xem [`BUILD.vi.md`](BUILD.vi.md).

Bộ cài desktop nằm trên
[trang Releases](https://github.com/manhpham90vn/Deskhub/releases). Bản Android có trên
Google Play, còn bản beta iOS trên TestFlight.

Đây là bản dịch của [`INSTALL.md`](INSTALL.md). Nếu hai bản có khác biệt, bản tiếng Anh là
bản chuẩn.

| Nền tảng | File | Cách cài |
| --- | --- | --- |
| 🪟 Windows | `deskhub-v*-windows-setup.exe` | Tải và cài; mở DeskHub từ Start hoặc Desktop |
| 🍎 macOS | `deskhub-v*-macos.dmg` | Mở dmg rồi kéo Deskhub vào Applications |
| 🐧 Ubuntu, Kubuntu, Debian, Mint | `deskhub-v*-amd64.deb` | `sudo apt install ./deskhub-v*-amd64.deb` |
| 🐧 Fedora (Workstation và KDE spin) | `deskhub-v*-x86_64.rpm` | `sudo dnf install ./deskhub-v*-x86_64.rpm` |
| 🐧 openSUSE Tumbleweed | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Distro Linux khác | — | Build từ source: [BUILD.vi.md](BUILD.vi.md) |
| 🤖 Android | — | [Google Play](https://play.google.com/store/apps/details?id=com.manhpham.deskhub) |
| 📱 iOS | — | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

Mọi gói desktop còn cài kèm `deskhub-cli` cho các lệnh terminal và script. Trên Windows
và Linux, CLI vẫn có thể mở cửa sổ xem màn hình từ xa (trên Linux là cửa sổ X11, nên
desktop Wayland cần XWayland). Xem [Command line](#-command-line).

Bạn có thể cài qua Homebrew trên macOS hoặc [apt repository](#-linux) trên Ubuntu,
Kubuntu, Debian và Mint. Khi muốn lên bản mới, hãy dùng lệnh nâng cấp của package manager
tương ứng. Windows không có kênh package manager: tải file setup từ Releases, và chạy bản
mới đè lên bản cũ để nâng cấp.

---

## 🪟 Windows

Tải `deskhub-v*-windows-setup.exe` và chạy. Bộ cài xin quyền administrator, cài DeskHub
cho mọi người dùng vào `Program Files` — `Deskhub.exe`, `deskhub-cli.exe` và service
*DeskHub Secure Desktop* cùng nhau — thêm nó vào Start Menu, đưa thư mục đó vào `PATH` hệ
thống để `deskhub-cli` chạy được trong mọi terminal mở mới, và tạo shortcut trên Desktop
theo mặc định; bạn cũng có thể mở app ngay khi cài xong. Không cần tài khoản. Bản DeskHub
hay DeskHub CLI mà phiên bản trước cài riêng cho người dùng của bạn sẽ được gỡ trước, và
*Start Deskhub when you log in* được trỏ sang vị trí mới; settings và key của bạn vẫn nằm
nguyên chỗ cũ. Không có bản exe portable: file setup là cách duy nhất để cài app Windows,
vì service mà nó cài chính là thứ cho viewer đi qua hộp thoại UAC, màn hình khoá và màn
hình đăng nhập.

Muốn nâng cấp, chạy file setup mới đè lên bản cũ; nó thay file tại chỗ, còn settings và
key trong `%USERPROFILE%\.deskhub` vẫn được giữ.

Có ba điểm cần lưu ý:

- **Quyền Administrator, mỗi lần khởi động.** Windows hiện hộp thoại UAC ở mỗi lần chạy,
  vì không có quyền này thì không thể inject mouse và keyboard vào các cửa sổ chạy ở quyền
  cao. Khi bật *Start Deskhub when you log in*, app khởi động lúc đăng nhập dưới dạng một
  logon task chạy quyền cao, không hiện hộp thoại.
- **Rule Windows Firewall** do app tự thêm trong lần share đầu tiên.
- **Một system service, *DeskHub Secure Desktop***, chạy dưới tài khoản LocalSystem. Mặc
  định nó không làm gì cho tới khi DeskHub hoặc `deskhub-cli` share màn hình. Khi đó, lúc Windows chuyển sang
  secure desktop — hộp thoại UAC của một trình cài đặt, màn hình khoá, Ctrl+Alt+Del — viewer
  vẫn thấy màn hình và click, gõ được ở đó, nên họ có thể trả lời hộp thoại UAC thay bạn,
  và bất kỳ ai biết mật khẩu của bạn đều mở khoá được máy. Tick *Allow access before anyone
  signs in* trong Settings → Host — hoặc chạy `deskhub-cli settings set unattended=on` từ
  một terminal administrator — thì service còn tự làm host bất cứ khi nào cả app lẫn CLI
  đều không share — từ màn hình đăng nhập ngay sau khi máy khởi động — cho các thiết bị đã có trong
  danh sách được phép của bạn; xem [đặc tả](SPECIFICATION.vi.md) (T-28) để biết nó làm gì
  và không làm gì. Bản build từ source chạy không có service thì các màn hình đó đứng yên
  với viewer cho tới khi chúng đóng lại, và ô chọn của nó bị mờ đi.

Gỡ DeskHub trong Windows Settings; CLI và service cũng được gỡ theo, cùng với
`%ProgramData%\DeskHub`, nơi service giữ log và các bản copy mà
*Allow access before anyone signs in* đã tạo.
Settings và key vẫn nằm trong `%USERPROFILE%\.deskhub` cho đến khi bạn xoá thư mục đó, còn
file máy khác gửi tới vẫn nằm trong `%USERPROFILE%\Deskhub` (hoặc thư mục bạn đã chọn) cho
đến khi bạn xoá.

## 🍎 macOS

Deskhub cần macOS 14 Sonoma trở lên.

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

Nếu chỉ xem máy khác thì không cần permission nào. Sau khi bật Screen Recording trong
System Settings, hãy thoát rồi mở lại Deskhub — macOS chỉ áp dụng quyền này cho lần chạy
mới, và trang Settings sẽ nhắc bạn.

macOS còn tự hỏi thêm hai quyền: **Local Network**, cần để Deskhub tới được máy kia dù đang
share hay đang connect, và quyền hiện **thông báo**, được hỏi ở lần đầu có yêu cầu kết nối
cần báo.

**Allow access before anyone signs in** (mặc định tắt) nằm trên cùng trang Settings. Tick vào
sẽ đăng ký `DeskhubDaemon`, một helper nằm trong app, làm mục chạy nền: macOS hỏi bạn một
lần ở System Settings → General → Login Items & Extensions, và app mời mở đúng pane đó.
Helper cần **Screen Recording** và **Accessibility** của riêng nó — hãy cấp cho
`DeskhubDaemon` trong hai pane đó; trang Settings hiện trạng thái nó thấy ở lần chạy gần
nhất — rồi từ đó các thiết bị đã có trong danh sách được phép thấy được login window và gõ
mật khẩu bất cứ khi nào không ai đăng nhập: sau khi sign out, và ngay sau khi khởi động lại
nếu FileVault tắt. Khi FileVault bật, máy Mac khởi động lại sẽ chờ ở màn hình mở khoá đĩa,
nơi không có gì chạy, rồi đăng nhập thẳng người vừa mở khoá, nên chỉ trường hợp sign out
được bao phủ. Khi có người đăng nhập, host của helper dừng và chỉ còn app share. Bỏ tick sẽ
xoá các bản copy helper giữ trong `/Library/Application Support/Deskhub` và huỷ đăng ký nó;
`brew uninstall --zap deskhub` xoá thư mục đó, còn gỡ bằng cách kéo vào Thùng rác thì bạn tự
xoá.

## 🐧 Linux

**Nếu chỉ cần connect và xem, hãy bắt đầu bằng gói app.** H.264 decoder đã nằm trong app,
nên bạn không phải cài thêm gói FFmpeg. Distro của bạn vẫn cần cung cấp các thư viện mà
app sử dụng, gồm GTK3, PipeWire và libva.

Bản deb và rpm có nội dung giống nhau; chọn bản mà package manager của hệ thống hỗ trợ.
Cả hai đều cài udev rule cho `/dev/uinput` mô tả ở mục 3 bên dưới, nên remote input hoạt
động ngay sau khi cài, không cần đổi group và không cần đăng nhập lại. Cả hai đều cần
x86_64 với glibc 2.35 trở lên (Ubuntu 22.04, Debian 12, Fedora 36, openSUSE Tumbleweed).
openSUSE Leap 15.5 dùng glibc 2.31 nên không chạy được.

Mỗi gói cài app desktop thành `/usr/bin/deskhub`, thêm launcher vào menu ứng dụng, và cài
`/usr/bin/deskhub-cli` cạnh đó. Gói `deskhub-cli` riêng của các bản trước không còn: gói
này thay thế nó, nên bản cũ được gỡ khi nâng cấp.

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

**Để share màn hình của máy này**, cần thêm ba điều kiện.

### 1. Screen-capture portal

Deskhub luôn capture thông qua `xdg-desktop-portal`. Đây là thành phần hiển thị hộp thoại
chọn màn hình cần share. Lựa chọn được lưu lại, nên hộp thoại chỉ xuất hiện ở lần share
đầu tiên. Khi bạn tick một màn hình trên trang Host mà lựa chọn đã lưu không bao gồm,
Deskhub tự quên lựa chọn đó và hộp thoại hiện lại. Để tự xoá lựa chọn, chạy
`deskhub-cli displays --forget`.

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

### 2. H.264 encoder phần cứng

H.264 được encode trên GPU; không có software fallback. Host dùng encoder nào tuỳ vào GPU
đang vẽ desktop:

- **NVIDIA với driver độc quyền** — NVENC, qua chính thư viện của driver, miễn là driver hỗ
  trợ NVENC API 13.0 trở lên. Không cần cài thêm gì, và trên máy như vậy `vainfo` có thể
  không liệt kê H.264 encoder nào mà máy vẫn host được. `nvidia-vaapi-driver` và
  `libva-nvidia-driver` chỉ decode: chúng giúp xem nhanh hơn, không giúp share.
- **AMD, Intel và mọi GPU khác** — VA-API, cần driver có H.264 encoder:

```bash
# Ubuntu / Debian / Mint
sudo apt install va-driver-all vainfo        # NVIDIA, chỉ decode: nvidia-vaapi-driver

# Fedora — Mesa mặc định tắt H.264; driver dùng được nằm trong RPM Fusion:
sudo dnf install libva-utils
sudo dnf install mesa-va-drivers-freeworld   # AMD (RPM Fusion)
sudo dnf install intel-media-driver          # Intel (RPM Fusion)
sudo dnf install nvidia-vaapi-driver         # NVIDIA, chỉ decode (RPM Fusion)

# openSUSE
sudo zypper install libva-utils              # kèm VA-API driver của hãng GPU tương ứng

# Arch
sudo pacman -S libva-utils
sudo pacman -S libva-mesa-driver             # AMD · Intel: intel-media-driver · NVIDIA, chỉ decode: libva-nvidia-driver

# sau đó, trên AMD hoặc Intel:
vainfo | grep -E 'H264.*Enc'                 # phải in ra ít nhất một dòng, nếu không máy này không host được
```

### 3. Quyền ghi vào `/dev/uinput`

Đây là cơ chế inject mouse và keyboard. Bản deb và rpm đã cài sẵn udev rule, không cần
thao tác thêm. Với bản build từ source, một lệnh là đủ; không cần clone repo và không cần
đăng nhập lại:

```bash
curl -fsSL https://raw.githubusercontent.com/manhpham90vn/Deskhub/main/scripts/setup-uinput.sh | sudo bash
```

Nếu muốn xem nội dung script trước khi chạy bằng sudo, tải
[`scripts/setup-uinput.sh`](../scripts/setup-uinput.sh) về trước; script này chỉ dài hơn
mười dòng. Nếu đã checkout source, lệnh tương đương là
`make setup-linux-permissions`. Ngoài rule, script còn thêm user đã chạy nó bằng sudo vào
nhóm `input`, có hiệu lực từ lần đăng nhập tiếp theo: session SSH hoặc headless cần nhóm
này để truy cập `/dev/uinput`, và nó cũng bật host wins (bên dưới).

Không có quyền uinput thì app vẫn chạy và vẫn xem được, chỉ không inject được mouse hay
keyboard vào máy này.

<a id="host-wins"></a>

### Để người ngồi tại máy được ưu tiên

Khi bạn dùng mouse hoặc keyboard của chính máy này, Deskhub tạm dừng remote input ("host
wins"). Để nhận ra bạn, nó đọc `/dev/input/event*`, việc chỉ thành viên nhóm `input` làm
được — và bản deb lẫn rpm không thêm ai vào nhóm đó. Thiếu nhóm này, share và remote input
vẫn chạy, nhưng input của viewer không bị tạm dừng khi bạn gõ. Để bật:

```bash
sudo usermod -aG input "$USER"               # rồi đăng xuất và đăng nhập lại
```

Là thành viên nhóm `input` thì mọi chương trình bạn chạy, không riêng Deskhub, đều đọc được
mọi keyboard và mouse của máy, nên hãy bỏ qua bước này nếu điều đó quan trọng hơn host wins.

### Firewall

Nếu đã bật firewall, cần mở UDP 47777:

```bash
sudo ufw allow 47777/udp                                  # Ubuntu / Debian / Mint
sudo firewall-cmd --add-port=47777/udp --permanent        # Fedora / openSUSE
```

### Gỡ cài đặt

```bash
sudo apt remove deskhub      # hoặc dnf remove / zypper remove
rm -rf ~/.deskhub            # settings, key, client được phép và host đã trust
sudo rm -f /etc/apt/sources.list.d/deskhub.list /etc/apt/keyrings/deskhub.gpg   # apt repository, nếu đã thêm
```

File máy khác gửi tới vẫn nằm trong `~/Deskhub` (hoặc thư mục bạn đã chọn) cho đến khi bạn
xoá. `setup-uinput.sh` để lại udev
rule, file module-load và việc bạn là thành viên nhóm `input`; hãy tự gỡ nếu không còn gì
cần đến chúng:

```bash
sudo rm -f /etc/udev/rules.d/60-deskhub-uinput.rules /etc/modules-load.d/deskhub.conf
sudo gpasswd -d "$USER" input
```

## 🤖 Android

Deskhub cần **Android 8.0** trở lên. Khi làm host, Deskhub chỉ share màn hình ở chế độ
view-only và yêu cầu **Android 10+**.

Cài từ
**[play.google.com/store/apps/details?id=com.manhpham.deskhub](https://play.google.com/store/apps/details?id=com.manhpham.deskhub)**.
Mỗi bản release vào track internal của Google Play trước, và lên store khi được promote,
nên store có thể chậm hơn trang Releases một chút.

Các bản release đến 9.3.1 còn đính kèm apk; các bản mới hơn thì không. Máy đang chạy apk
vẫn dùng được, nhưng Android sẽ không cài đè bản Google Play lên nó: hãy gỡ apk trước —
việc này xoá key của bản đó, nên các host phải cho thiết bị vào lại.

## 📱 iOS

Deskhub cần iOS hoặc iPadOS 17 trở lên. ipa không sideload được, nên bản beta phát hành
qua TestFlight:

1. Cài [TestFlight](https://apps.apple.com/app/testflight/id899247664).
2. Tham gia bản beta: **[testflight.apple.com/join/7qY7wgpd](https://testflight.apple.com/join/7qY7wgpd)**

Tương tự Android, iPhone và iPad chỉ host được ở chế độ view-only: không OS di động nào
cho phép app inject input vào chính thiết bị đang chạy nó.

---

## 💻 Command line

`deskhub-cli` cung cấp lệnh để share màn hình, mở remote shell và chạy từ script hoặc qua
SSH. Trên Windows và Linux, `connect` mở cửa sổ xem màn hình từ xa — trên Linux là cửa sổ
X11, mà desktop Wayland hiển thị qua XWayland; trên macOS, hãy dùng app desktop để xem màn
hình. Chạy `deskhub-cli help` để xem danh sách lệnh. CLI và app dùng
chung settings, key của máy, client được phép, yêu cầu kết nối và các host đã trust.

Không có bản tải riêng: mọi gói desktop đều cài nó cạnh app.

| Nền tảng | Nằm ở đâu |
| --- | --- |
| 🪟 Windows | `deskhub-cli.exe` trong thư mục cài DeskHub, thư mục mà bộ cài đã thêm vào `PATH` của máy |
| 🍎 macOS | `Deskhub.app/Contents/MacOS/deskhub-cli`, một binary cho cả Apple Silicon và Intel; cask Homebrew link nó vào `PATH` |
| 🐧 Linux | `/usr/bin/deskhub-cli`, từ cùng gói deb hay rpm với app |

Trên Windows, sau khi bộ cài chạy xong hãy mở cửa sổ PowerShell mới và chạy
`deskhub-cli help`; `Get-Command deskhub-cli` cho biết Windows tìm thấy file nào. Trên
macOS không dùng Homebrew, chạy bằng đường dẫn đầy đủ hoặc tự link:
`ln -s /Applications/Deskhub.app/Contents/MacOS/deskhub-cli /usr/local/bin/deskhub-cli`.

**Trên Windows, CLI dùng service *DeskHub Secure Desktop* như app.** Bộ cài đặt
`deskhub-cli.exe` cạnh `Deskhub.exe` và service, và service phục vụ cả hai chương trình
trong thư mục đó. Vì vậy `deskhub-cli share` đi tiếp qua hộp thoại UAC, màn hình khoá và
màn hình đăng nhập đúng như app, còn `deskhub-cli settings set unattended=on` bật *Allow
access before anyone signs in* — cùng một setting với ô chọn của app, đẩy tới cùng một
service, và `access add`, `access remove`, `access approve`, `access clear` giữ cho host
trước khi đăng nhập luôn đồng bộ. Hai điều cần biết: chỉ administrator mới nói chuyện được
với service, nên hãy chạy terminal dưới quyền administrator cho các phần đó — một
`deskhub-cli share` không nâng quyền vẫn chạy, nhưng không có secure desktop, và khi
setting kia đang bật thì nó không giành lại được port từ host trước khi đăng nhập; và một
`deskhub-cli.exe` copy đi nơi khác, hay build từ source, không được phục vụ, như mọi bản
copy nằm ngoài thư mục đã cài. Trên macOS, *Allow access before anyone signs in* chỉ bật
được trong app: `deskhub-cli settings set unattended=on` từ chối ở đó, cũng như trên Linux,
nơi chưa có host trước khi đăng nhập, và thay đổi CLI thực hiện với danh sách được phép trên
Mac sẽ tới host trước khi đăng nhập ở lần kế tiếp app khởi động hoặc dừng share. Mọi bộ cài
và gói cài đặt đều mang theo `THIRD_PARTY_NOTICES.md` và văn bản giấy phép của các thư viện
bên trong.

Trên Linux, `deskhub-cli` share màn hình qua cùng portal và VA-API driver mà app cần, đồng
thời dựa vào cùng rule `/dev/uinput` cho remote input, nên toàn bộ mục [Linux](#-linux) áp
dụng cho nó. Trên macOS, nó share được màn hình và mở được shell nhưng không xem được màn
hình máy khác: `connect` cần một window layer mà bản command line không có, và sẽ báo lỗi
tương ứng. Trường hợp này cần dùng app.

---

## 🔒 Trước khi share màn hình

Mọi dữ liệu một session mang theo — video, phím gõ, mouse, clipboard và lưu lượng terminal
— đều chạy trên **QUIC/TLS**, và quyền truy cập hoạt động như SSH: host chỉ cho vào những
thiết bị có key trong danh sách của nó, và mỗi thiết bị có một key. Có ba cách để đưa một
thiết bị vào danh sách đó, và bạn chỉ cần một trong ba:

- **Scan mã QR.** Trong khi host đang share, bấm **Show QR code** cạnh danh sách địa chỉ
  của nó. Trên điện thoại, bấm **Scan QR code** trên trang Client và hướng camera vào màn
  hình; trên thiết bị khác, copy link bên dưới mã và dán vào trường địa chỉ. Thiết bị được
  trust, được cho phép và được connect trong một bước.
- **Approve yêu cầu.** Trên thiết bị, nhập địa chỉ của host và bấm *Connect*. Host hiện
  thông báo *Connection request* và liệt kê nó dưới **Connection requests** trên trang Host
  và trang Devices, kèm tên, fingerprint key và địa chỉ; bấm **Approve** và thiết bị connect ở lần thử tiếp theo — nó tự thử lại trong hai phút.
- **Dán key.** Trên thiết bị, **Devices** → **Copy public key**; trên host, **Devices** →
  **Devices allowed to connect to this machine** → *Allow*, dán, xong.

Connect theo địa chỉ lần đầu tiên hiển thị hộp thoại **New host** với fingerprint key của
host: đối chiếu nó với fingerprint dưới **This machine** trên trang Devices của host, rồi bấm
*Trust and connect*. Scan mã QR trong app thì bỏ qua hộp thoại đó, vì mã mang sẵn fingerprint. Từ đó trở đi,
host nằm trong **Trusted hosts**, và vẫn được trust ngay cả khi đổi địa chỉ.

[Key và quyền truy cập](#-key-và-quyền-truy-cập) hướng dẫn từng cách trên app và CLI,
cùng với script và thu hồi.

Không có passcode và không có công tắc nào cho máy lạ vào: không ai connect được nếu thiếu
*Approve* của bạn, mã QR bạn đã cho xem, hoặc key bạn đã dán. Deskhub không bao giờ scan
network.

Hãy dùng Deskhub trên **network tin cậy** hoặc qua **VPN**. Không port-forward UDP
47777. Encrypt giúp bảo vệ nội dung session, nhưng lần connect đầu tiên tới một host sẽ
trust key được hiển thị, trừ khi bạn đối chiếu fingerprint. Để truy cập từ xa, bạn có
thể cài [Tailscale](https://tailscale.com) trên cả hai máy và connect tới địa chỉ `100.x.y.z`.

[`SECURITY.vi.md`](../SECURITY.vi.md) mô tả đầy đủ threat model, phạm vi được bảo vệ và
cách báo lỗ hổng.

## 🔑 Key và quyền truy cập

Deskhub đăng nhập bằng cặp key, giống SSH. Mỗi máy có **một key**, được tạo ở lần đầu
Deskhub chạy, và key đó chính là máy dù nó đang share hay đang connect: fingerprint bạn đối
chiếu khi connect tới nó và public key mà host lưu khi cho nó vào là cùng một key. Host chỉ
cho vào những key có trong danh sách của nó, còn client chỉ connect tới host có key mà nó
đã trust. Mọi thứ nằm trên trang **Devices**, gồm bốn mục: **This machine**, **Connection
requests**, **Devices allowed to connect to this machine** và **Trusted hosts**. Mỗi bước bên dưới đều có lệnh `deskhub-cli` tương ứng;
app và CLI đọc chung các file, nên thay đổi ở bên này sẽ hiện ra ở bên kia.

### Key của bạn

**Devices** → **This machine** hiển thị tên thiết bị này và fingerprint của key, dạng `SHA256:…`, kèm nút *Copy* — đó là thứ người connect tới bạn đối
chiếu. **Copy public key** bên cạnh (CLI: `deskhub-cli key public`) copy một dòng như
`ecdsa-sha2-nistp256 AAAA… laptop`; nhãn ở cuối là tên thiết bị này, đặt ở **Settings** →
*General* → **Device name**, để chủ host biết key đó của ai. Dòng này là thứ bạn đưa cho
chủ host khi cả mã QR lẫn yêu cầu kết nối đều không tiện — chia sẻ nó là an toàn. Nửa
private không bao giờ rời khỏi máy, không có gì để tạo hay import, và key không bao giờ tự
thay. Nếu bạn xoá `host_key.pem`, máy có danh tính mới: các host phải cho nó vào lại, và
các thiết bị đã trust nó sẽ thấy nó như một host mới.

### Cho phép một thiết bị connect

Chủ host quyết định, theo một trong ba cách. Dù dùng cách nào, thiết bị sau đó xuất hiện
dưới **Devices** → **Devices allowed to connect to this machine**, gắn nhãn bằng tên của nó; *Remove* cạnh nó sẽ gỡ nó ra, và *Remove every client*
xoá sạch danh sách sau khi hỏi xác nhận.

**Connect bằng mã QR** — nhanh nhất, và là cách duy nhất không cần đối chiếu fingerprint.
Trong khi host đang share, bấm **Show QR code** cạnh danh sách địa chỉ của nó. Mã mang các
địa chỉ và port của host, fingerprint key, tên của nó và một token ngẫu nhiên dùng một lần
có hiệu lực năm phút.

- Trên điện thoại hoặc tablet, mở trang Client và bấm **Scan QR code**. Lần đầu, hệ thống
  xin permission camera; Deskhub chỉ dùng camera ở đây, giải mã khung hình trên thiết bị và
  không lưu gì. Mở một link `deskhub://pair/…` bằng camera hệ thống hoặc từ một tin nhắn
  cũng được, nhưng vì link như vậy có thể đến từ bất cứ đâu, khi host đó chưa được trust app
  sẽ hiện hộp thoại **New host** với fingerprint trong link trước, và chỉ connect khi bạn đã
  đối chiếu và bấm *Trust and connect*; host bạn đã trust thì connect ngay.
- Trên bất kỳ thiết bị nào, kể cả desktop, copy link hiển thị dưới mã và dán vào trường địa
  chỉ, rồi bấm *Connect* (CLI: `deskhub-cli connect 'deskhub://pair/…'`; `sources`, `shell`
  và `send` cũng nhận link đó).

Thiết bị kiểm tra rằng máy đang trả lời giữ key in trong mã — nếu một máy khác trả lời ở
địa chỉ đó, nó dừng lại và báo mã QR không phải của máy đó — rồi trust host, gửi token, và
được cho vào cùng connect trong một lượt. Mỗi mã dùng được một lần; bấm **Hide QR code**,
hoặc dừng share, và mã hết hiệu lực kể cả khi chưa ai dùng. Thiết bị đã được cho phép có thể
scan mã chỉ để lấy địa chỉ hiện tại của host.

**Approve một yêu cầu kết nối** — khi thiết bị không ở cùng phòng.

1. Trên thiết bị, nhập địa chỉ của host (`192.168.1.10`, hoặc `192.168.1.10:PORT` khi host
   không dùng 47777) và bấm *Connect*; xác nhận hộp thoại **New host** như mô tả bên dưới.
   Trang sau đó báo đang chờ chủ host approve, và tự thử lại trong hai phút. *Cancel* dừng
   việc này.
2. Trên host, trang Host liệt kê thiết bị dưới **Connection requests** — tên, phần đầu
   fingerprint và địa chỉ của nó, kèm **Approve** và **Deny**. Kiểm tra fingerprint khớp
   với fingerprint trên trang Devices của thiết bị và địa chỉ đúng là nơi bạn mong đợi
   thiết bị ở, rồi bấm *Approve*. Thiết bị connect ở lần thử tiếp theo. *Deny* bỏ yêu cầu;
   thiết bị chỉ được báo là không ai approve kịp thời.
3. Một yêu cầu tồn tại mười phút và host giữ tối đa mười sáu. Nếu bạn approve sau khi thiết
   bị đã bỏ cuộc, nó chỉ cần bấm *Connect* thêm một lần.

Từ command line: `deskhub-cli access requests` (thêm `--json` cho script) liệt kê những gì
đang chờ, `deskhub-cli access approve --fingerprint SHA256:…` cho một thiết bị vào và
`deskhub-cli access deny --fingerprint SHA256:…` bỏ yêu cầu. Một `deskhub-cli share` đang
chạy in ra từng yêu cầu mới khi nó tới, kèm lệnh approve để dán vào terminal khác.

**Dán key** — khi bạn muốn tự chuyển key.

1. Xin người sẽ connect bấm **Copy public key** trên trang Devices của họ (xem
   [Key của bạn](#key-của-bạn)) và gửi dòng đó cho bạn.
2. Mở **Devices** → **Devices allowed to connect to this machine**, dán dòng đó vào và bấm *Allow*. Chỉ chấp nhận public key Ed25519 và
   ECDSA P-256.

Từ command line, pipe dòng đó vào `access add`:

```sh
deskhub-cli key public                                         # trên thiết bị
deskhub-cli access add --stdin                                 # trên host: dán, rồi Ctrl-D
deskhub-cli key public | ssh me@host deskhub-cli access add --stdin
deskhub-cli access list                                        # trên host
deskhub-cli access remove --fingerprint SHA256:…               # trên host
```

`deskhub-cli access clear` làm điều tương tự *Remove every client*.

### Connect lần đầu

Mã QR làm việc này thay bạn. Connect theo địa chỉ lần đầu tiên hoạt động như SSH:

1. Trên client, nhập địa chỉ của host và bấm *Connect*.
2. Hộp thoại **New host** hiển thị fingerprint key của host, dạng `SHA256:…`. Nếu địa chỉ
   đó từng thuộc về một host khác bạn đã trust, hộp thoại nói rõ và nêu tên host đó — một
   key khác ở địa chỉ đã biết là một máy khác, nên hãy chắc bạn biết máy nào đang trả lời
   trước khi tiếp tục.
3. Trên host, mở **Devices** → **This machine** (*Copy* cạnh fingerprint đưa nó vào
   clipboard). Đối chiếu hai fingerprint qua một kênh bạn đã tin
   — gặp trực tiếp, gọi điện, hoặc chat mà bạn biết chắc là của họ.
4. Nếu khớp, bấm *Trust and connect*. Nếu không khớp, bấm *Cancel*.

Host giờ nằm trong **Trusted hosts** kèm tên, fingerprint và địa chỉ gần nhất nó trả lời.
Trust đi theo key, không theo địa chỉ: khi host có địa chỉ mới, nhập địa chỉ đó và connect —
không hộp thoại, không approve lại — và danh sách ghi nhớ địa chỉ mới cho lần sau.

**Pin host từ trước.** Với CLI, bạn có thể lưu host key trước lần connect đầu tiên, nên
không cần hộp thoại:

```sh
deskhub-cli host-key public                                    # trên host
deskhub-cli host add office --address 192.168.1.10 --host-key-stdin
```

Dán dòng của host vào lệnh thứ hai (trên client), rồi Ctrl-D. `office` là alias do bạn
chọn: `connect office`, `sources office`, `shell office` và `send office FILE` đều nhận
nó. `deskhub-cli host list` hiển thị mọi host đã lưu, kể cả các host được trust từ app —
chúng được đặt alias theo địa chỉ, ví dụ `192-168-1-10-47777`. `host update ALIAS` đổi địa
chỉ (`--address`) hoặc host key đã pin (`--host-key-stdin`); `host remove ALIAS` bỏ host
đó, và `deskhub-cli trust forget` cũng vậy khi được đưa fingerprint, alias hoặc địa chỉ gần
nhất của host.

### Script và command line

- **Host lạ bị từ chối.** `sources`, `connect`, `shell` và `send` không làm việc với host
  chưa được lưu key. Thay vào đó chúng in fingerprint của host; đối chiếu với host, rồi
  chạy lại với `--accept-new-host-key` để lưu nó. Khi được đưa link mời thay cho địa chỉ,
  chúng pin host từ link và không cần cờ nào.
- **Chờ approve.** Host chưa cho phép máy này sẽ được yêu cầu approve; `connect`, `shell`,
  `send` và `sources` in một dòng báo như vậy và chờ tối đa hai phút để chủ host bấm
  *Approve*, rồi thất bại kèm lý do nếu không ai bấm.
- **Hiển thị mã.** `deskhub-cli share --qr` in mã QR bằng ký tự khối cùng với link mời, để
  điện thoại scan ngay từ terminal.
- **Cấu hình riêng.** `--config-dir DIR` (đặt trước hoặc sau lệnh) hoặc biến môi trường
  `DESKHUB_CONFIG_DIR` trỏ CLI tới một thư mục khác chứa key, client được phép, yêu cầu kết
  nối, host đã trust và settings — tiện cho service account hoặc môi trường test.
- **Đã gỡ trong 8.0.** `key generate`, `key import`, `key delete`, `key list`, `key public
  --name`, `devices identities`, `--identity` và `host add --identity` không còn nữa: mỗi
  máy một key. Exit code `5` ("host key đã thay đổi") cũng không còn, vì một key khác giờ
  là một host lạ.
- **Exit code.** Script có thể phân biệt connection bị từ chối với các lỗi khác qua exit
  code; danh sách mã nằm ở mục
  [Command line client](BUILD.vi.md#command-line-client) trong `BUILD.vi.md`. Các lệnh liệt
  kê hỗ trợ `--json`.

### Thu hồi một thiết bị

Trên host, bấm *Remove* cạnh key của nó, hoặc chạy
`deskhub-cli access remove --fingerprint SHA256:…`. Mọi session thiết bị đó đang mở kết
thúc ngay lập tức, và nó không connect lại được cho đến khi bạn cho nó vào lại — bằng cách
approve yêu cầu tiếp theo của nó, cho nó xem mã QR, hoặc dán key của nó.

**Khi một key khác trả lời ở địa chỉ đã biết.** Điều này xảy ra khi host cài lại Deskhub
hoặc mất thư mục settings — hoặc khi một máy khác đã lấy địa chỉ đó. Deskhub không từ chối;
nó coi máy đó như một máy bạn chưa từng gặp và hiển thị hộp thoại **New host** kèm cảnh báo
nêu tên host từng trả lời ở đó. Chỉ trust nó nếu bạn biết vì sao. Host cũ vẫn nằm trong
**Trusted hosts** cho tới khi bạn *Remove* nó (hoặc `deskhub-cli host remove ALIAS`); với
CLI, `host update ALIAS --host-key-stdin` pin lại một alias sang key mới trực tiếp.

### Chuyển từ Deskhub cũ

**Từ 8.0.0.** 9.0 dùng authentication version mới hơn 8.0.0, nên Deskhub 8.0.0
ở bất kỳ bên nào cũng không connect được và bị từ chối với thông báo "That machine uses an
incompatible authentication version". Hãy cập nhật cả hai máy; ngoài ra không có gì thay
đổi — key, các thiết bị được cho phép và các host đã trust đều được giữ nguyên.

**Từ 7.0.x.** Cả hai máy đều cần 9.0 — Deskhub 7.0.x ở bất kỳ bên nào cũng không connect
được và bị từ chối với thông báo "That machine uses an incompatible authentication
version". Mỗi máy giữ nguyên key và fingerprint nó đã có, nên các host bạn đã trust vẫn được
trust. Điều thay đổi là key mà thiết bị dùng *để đăng nhập*: giờ đó chính là key của máy,
nên mỗi thiết bị phải được cho vào thêm một lần — một lần *Approve*, một lần scan mã QR,
hoặc một lần dán key của nó. Các file `client_key*.pem` và `host_cert.pem` mà 7.0.x đã ghi
bị bỏ qua; không có gì trong đó được đọc hay migrate, và bạn có thể xoá chúng.

**Từ 6.x hoặc cũ hơn.** Không có gì được migrate. Passcode và danh sách thiết bị đã pair
không được giữ lại: hãy cho từng thiết bị vào và trust lại từng host như mô tả ở trên.

## 🆘 Khi gặp sự cố

- **Không tìm thấy máy nào để connect** — hai máy phải nằm trên cùng network (hoặc cùng
  tailnet Tailscale), và UDP 47777 phải được mở ở phía host.
- **"Waiting for the owner of … to approve this device"** — host chưa có thiết bị này
  trong danh sách; chủ host cần bấm *Approve* dưới **Connection requests** trên trang Host
  của họ, cho bạn xem mã QR, hoặc dán key của bạn — xem
  [Cho phép một thiết bị connect](#cho-phép-một-thiết-bị-connect). Các thiết bị được 7.0.x
  cho phép phải được cho vào lại.
- **"The owner of that machine did not approve this device in time"** — hai phút đã trôi
  qua mà không có *Approve*. Yêu cầu vẫn nằm trên host trong mười phút: hãy hỏi, rồi bấm
  *Connect* lại.
- **"The machine that answered is not the one that made this QR code"** — có thứ gì khác
  đang trả lời ở địa chỉ trong mã. Hiển thị mã lại trên host và scan thêm một lần; nếu vẫn
  xảy ra, hãy kiểm tra máy nào đang giữ địa chỉ đó.
- **Hộp thoại New host cho một host bạn đã trust** — một key khác đang trả lời ở địa chỉ
  đó; xem [Thu hồi một thiết bị](#thu-hồi-một-thiết-bị).
- **"That machine uses an incompatible authentication version"** — một bên đang chạy
  Deskhub cũ; hãy cập nhật cả hai máy. Xem [Chuyển từ Deskhub cũ](#chuyển-từ-deskhub-cũ).
- **Linux: share thất bại ngay lập tức** — trên AMD hoặc Intel, chạy
  `vainfo | grep -E 'H264.*Enc'`; kết quả rỗng nghĩa là máy này không có H.264 encoder dùng
  được và không thể host. Trên NVIDIA, log trong `~/.deskhub` cho biết NVENC của driver có
  quá cũ hay không.
- **Linux: viewer vẫn điều khiển trong khi bạn gõ** — user của bạn chưa ở trong nhóm
  `input`; xem [Để người ngồi tại máy được ưu tiên](#host-wins).
- **Linux: con trỏ không di chuyển** — thiếu rule `/dev/uinput` ở mục 3.
- **macOS: màn hình đen hoặc input không hoạt động** — kiểm tra Screen Recording và
  Accessibility trên trang Settings.
- **Các trường hợp khác** — mở một [issue](https://github.com/manhpham90vn/Deskhub/issues)
  kèm model thiết bị, phiên bản OS và nội dung dòng status trên trang Host hoặc Client.
