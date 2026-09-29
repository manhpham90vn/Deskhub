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
rm -rf ~/.deskhub            # settings, key, client được phép và host đã trust
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
chung settings, client key, client được phép và các host đã trust.

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
— đều chạy trên **QUIC/TLS**, và quyền truy cập hoạt động như SSH. Để cho phép một thiết bị
connect:

1. Trên thiết bị sẽ connect, mở **Devices** → *When this machine is the client* →
   **My keys** và bấm *Copy public key* (CLI: `deskhub-cli key public --name default`).
2. Trên host, mở **Devices** → *When this machine is the host* → **Clients allowed to
   connect to this machine**, bấm *Allow* và dán key đó vào (CLI:
   `deskhub-cli access add --stdin`).
3. Connect theo địa chỉ. Lần đầu tiên, hộp thoại **New host** hiển thị fingerprint key của
   host: đối chiếu nó với **This machine's host key** trên trang Devices của host, rồi bấm
   *Trust and connect*. Từ đó trở đi, host nằm trong **Trusted hosts**.

[Key và quyền truy cập](#-key-và-quyền-truy-cập) hướng dẫn từng bước trên app và CLI,
cùng với key bổ sung, script, thu hồi và thay key.

Không có gì được chấp thuận qua network và Deskhub không bao giờ scan network. Nếu key của
một host đã trust thay đổi, connection bị từ chối ngay; chỉ gỡ host đó khỏi *Trusted hosts*
khi bạn biết vì sao key của nó thay đổi, rồi trust lại.

Hãy dùng Deskhub trên **network tin cậy** hoặc qua **VPN**. **Không port-forward UDP
47777.** Encrypt giúp bảo vệ nội dung session, nhưng lần connect đầu tiên tới một host sẽ
trust key được hiển thị, trừ khi bạn đối chiếu fingerprint. Để truy cập từ xa, bạn có
thể cài [Tailscale](https://tailscale.com) trên cả hai máy và connect tới địa chỉ `100.x.y.z`.

[`SECURITY.vi.md`](../SECURITY.vi.md) mô tả đầy đủ threat model, phạm vi được bảo vệ và
cách báo lỗ hổng.

## 🔑 Key và quyền truy cập

Deskhub đăng nhập bằng cặp key, giống SSH. Mỗi thiết bị connect có một **client key**, và
mỗi host có một **host key**. Host chỉ cho vào những client key có trong danh sách của nó,
còn client chỉ connect tới host có key mà nó đã trust. Mọi thứ nằm trên trang **Devices**,
chia thành *When this machine is the host* và *When this machine is the client*. Mỗi bước
bên dưới đều có lệnh `deskhub-cli` tương ứng; app và CLI đọc chung các file, nên thay đổi
ở bên này sẽ hiện ra ở bên kia.

### Key của bạn

Key của bạn nằm ở **Devices** → *When this machine is the client* → **My keys**. Nửa
private không bao giờ rời khỏi máy.

- **Key `default`.** App tạo key tên `default` ở lần đầu bạn mở trang Devices, còn CLI tạo
  nó ở lần đầu connect tới bất kỳ đâu hoặc khi in nó bằng
  `deskhub-cli key public --name default`. Phần lớn người dùng không cần key nào khác.
- **Thêm key.** Bấm *New key* và đặt tên (CLI: `deskhub-cli key generate --name NAME`).
  Dùng key riêng khi bạn muốn thu hồi quyền truy cập của một host mà không ảnh hưởng các
  host khác.
- **Key có sẵn.** Bấm *Import key…*, chọn file private key, đặt tên và, nếu file được
  encrypt, nhập passphrase (CLI: `deskhub-cli key import --name NAME --file PATH`, thêm
  `--passphrase-stdin` để đọc passphrase từ standard input). Deskhub đọc file OpenSSH
  (`ssh-keygen`) và PKCS#8 chứa key Ed25519 hoặc ECDSA P-256; RSA không được hỗ trợ.
  Passphrase chỉ dùng để mở file trong lúc import — nó không được lưu và không bao giờ
  được gửi tới host.
- **Public key của bạn.** Bấm *Copy public key* cạnh một key (CLI:
  `deskhub-cli key public --name NAME`). Bạn nhận được một dòng như
  `ssh-ed25519 AAAA… laptop`; nhãn ở cuối là tên thiết bị này, đặt ở **Settings** →
  *General* → **Device name**, để chủ host biết key đó của ai. Dòng này là thứ bạn gửi cho
  chủ host — chia sẻ nó là an toàn.
- **Xoá key.** Bấm *Delete* cạnh key đó. Không xoá được key `default`, và cũng không xoá
  được key mà một host đã trust vẫn đang dùng — hãy chuyển host đó sang key khác trước.
  Private key đã xoá không khôi phục được. Trên CLI: `deskhub-cli key list` và
  `deskhub-cli key delete --name NAME`, cùng các quy tắc trên.

### Cho phép một thiết bị connect

Trên host:

1. Xin người sẽ connect dòng public key của họ (xem [Key của bạn](#key-của-bạn)).
2. Mở **Devices** → *When this machine is the host* → **Clients allowed to connect to
   this machine**, dán dòng đó vào và bấm *Allow*. Chỉ chấp nhận public key Ed25519 và
   ECDSA P-256.
3. Key giờ hiện trong danh sách kèm nhãn. *Remove* cạnh một key sẽ gỡ nó; *Remove every
   client* xoá sạch danh sách sau khi hỏi xác nhận.

Từ command line, pipe dòng đó vào `access add`:

```sh
deskhub-cli key public --name default                          # trên client
deskhub-cli access add --stdin                                 # trên host: dán, rồi Ctrl-D
deskhub-cli key public --name default | ssh me@host deskhub-cli access add --stdin
deskhub-cli access list                                        # trên host
deskhub-cli access remove --fingerprint SHA256:…               # trên host
```

`deskhub-cli access clear` làm điều tương tự *Remove every client*.

### Connect lần đầu

1. Trên client, nhập địa chỉ của host (`192.168.1.10`, hoặc `192.168.1.10:PORT` khi host
   không dùng 47777) và bấm *Connect*.
2. Hộp thoại **New host** hiển thị fingerprint host key, dạng `SHA256:…`.
3. Trên host, mở **Devices** → *When this machine is the host* → **This machine's host
   key** (*Copy* đưa nó vào clipboard). Đối chiếu hai fingerprint qua một kênh bạn đã tin
   — gặp trực tiếp, gọi điện, hoặc chat mà bạn biết chắc là của họ.
4. Nếu khớp, bấm *Trust and connect*. Nếu không khớp, bấm *Cancel*.

Host giờ nằm trong **Trusted hosts**, kèm client key nó dùng, và các lần connect sau sẽ
bỏ qua hộp thoại.

**Pin host từ trước.** Với CLI, bạn có thể lưu host key trước lần connect đầu tiên, nên
không cần hộp thoại:

```sh
deskhub-cli host-key public                                    # trên host
deskhub-cli host add office --address 192.168.1.10 --identity default --host-key-stdin
```

Dán dòng của host vào lệnh thứ hai (trên client), rồi Ctrl-D. `office` là alias do bạn
chọn: `connect office`, `sources office`, `shell office` và `send office FILE` đều nhận
nó. `deskhub-cli host list` hiển thị mọi host đã lưu, kể cả các host được trust từ app —
chúng được đặt alias theo địa chỉ, ví dụ `192-168-1-10-47777`. `host update ALIAS` đổi địa
chỉ (`--address`), client key (`--identity`) hoặc host key đã pin (`--host-key-stdin`);
`host remove ALIAS` bỏ host đó. App connect tới host mới bằng `default`; để dùng key khác
cho một host, đặt nó ở đây bằng `--identity` — app cũng sẽ dùng key đó.

### Script và command line

- **Host lạ bị từ chối.** `sources`, `connect`, `shell` và `send` không làm việc với host
  chưa được lưu key. Thay vào đó chúng in fingerprint của host; đối chiếu với host, rồi
  chạy lại với `--accept-new-host-key` để lưu nó. Cờ này chỉ lưu key được thấy lần đầu —
  host có key *đã thay đổi* luôn bị từ chối.
- **Chọn key.** `--identity NAME` chọn client key cho một lệnh. Không có cờ này thì dùng
  key của host đã lưu, và `default` cho host chưa có key nào.
- **Cấu hình riêng.** `--config-dir DIR` (đặt trước hoặc sau lệnh) hoặc biến môi trường
  `DESKHUB_CONFIG_DIR` trỏ CLI tới một thư mục khác chứa key, client được phép, host đã
  trust và settings — tiện cho service account hoặc môi trường test.
- **Exit code.** Script có thể phân biệt connection bị từ chối hay host key đã thay đổi
  với các lỗi khác qua exit code; danh sách mã nằm ở mục
  [Command line client](BUILD.vi.md#command-line-client) trong `BUILD.vi.md`. Các lệnh liệt
  kê hỗ trợ `--json`.

### Thu hồi và thay key

**Thu hồi một thiết bị.** Trên host, bấm *Remove* cạnh key của nó, hoặc chạy
`deskhub-cli access remove --fingerprint SHA256:…`. Mọi session thiết bị đó đang mở kết
thúc ngay lập tức, và nó không connect lại được cho đến khi key của nó được cho phép lại.

**Thay client key.**

1. Tạo key mới: *New key*, hoặc `deskhub-cli key generate --name NAME`.
2. Nhờ mỗi host cần chấp nhận key này cho phép public key của nó.
3. Cho host đã trust dùng key mới: `deskhub-cli host update ALIAS --identity NAME`, hoặc
   gỡ host rồi thêm lại bằng `host add … --identity NAME`. Connect một lần để kiểm tra.
4. Nhờ chủ host gỡ key cũ, rồi *Delete* key cũ trên máy này.

**Khi key của host thay đổi.** Deskhub từ chối connect và báo
"This host's key has changed". Điều này xảy ra khi host cài lại Deskhub hoặc mất thư mục
settings — hoặc khi một máy khác đang trả lời ở địa chỉ đó. Chỉ khi bạn biết vì sao key
thay đổi, hãy gỡ host khỏi **Trusted hosts** (*Remove*, hoặc
`deskhub-cli host remove ALIAS`), connect lại và đối chiếu fingerprint mới như lần connect
đầu tiên. Với CLI, `host update ALIAS --host-key-stdin` pin trực tiếp key mới.

### Chuyển từ Deskhub cũ

Không có gì được migrate. Passcode và danh sách thiết bị đã pair từ các phiên bản Deskhub
cũ không được giữ lại: hãy tạo hoặc copy key, cho phép chúng trên từng host và trust lại
từng host như mô tả ở trên. Cả hai máy đều cần phiên bản này — Deskhub cũ ở bất kỳ bên nào
cũng không connect được và bị từ chối với thông báo "That machine uses an incompatible
authentication version".

## 🆘 Khi gặp sự cố

- **Không tìm thấy máy nào để connect** — hai máy phải nằm trên cùng network (hoặc cùng
  tailnet Tailscale), và UDP 47777 phải được mở ở phía host.
- **"This device's key is not authorized on that machine yet"** — host không có client key này
  trong danh sách; hãy cho phép public key của nó trên trang Devices của host — xem
  [Cho phép một thiết bị connect](#cho-phép-một-thiết-bị-connect). Các client được một
  phiên bản Deskhub cũ cho phép phải được cho phép lại.
- **"This host's key has changed"** — xem [Thu hồi và thay key](#thu-hồi-và-thay-key).
- **"That machine uses an incompatible authentication version"** — một bên đang chạy
  Deskhub cũ; hãy cập nhật cả hai máy. Xem [Chuyển từ Deskhub cũ](#chuyển-từ-deskhub-cũ).
- **Linux: share thất bại ngay lập tức** — chạy `vainfo | grep -E 'H264.*Enc'`. Kết quả
  rỗng nghĩa là máy này không có H.264 encoder dùng được và không thể host.
- **Linux: con trỏ không di chuyển** — thiếu rule `/dev/uinput` ở mục 3.
- **macOS: màn hình đen hoặc input không hoạt động** — kiểm tra Screen Recording và
  Accessibility trên trang Settings.
- **Các trường hợp khác** — mở một [issue](https://github.com/manhpham90vn/Deskhub/issues)
  kèm model thiết bị, phiên bản OS và nội dung dòng status trên trang Host hoặc Client.
