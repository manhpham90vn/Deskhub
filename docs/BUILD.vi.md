[English](BUILD.md) · **Tiếng Việt** · [中文](BUILD.zh.md) · [日本語](BUILD.ja.md)

# Deskhub — Build và phát triển

Hướng dẫn này dành cho việc build Deskhub, chạy test và chuẩn bị release. Nếu chỉ muốn
cài app để sử dụng, hãy bắt đầu từ [`INSTALL.vi.md`](INSTALL.vi.md).

Đây là bản dịch của [`BUILD.md`](BUILD.md). Nếu hai bản có khác biệt, bản tiếng Anh là bản
chuẩn.

```bash
git clone --recurse-submodules https://github.com/manhpham90vn/Deskhub.git
cd Deskhub
make bootstrap        # một lần: toolchain và dependency cho OS hiện tại
make test             # build và chạy core suite ở chế độ offline
make build-linux      # hoặc build-windows / build-macos / build-ios / build-android
```

Hãy chọn rõ target cho nền tảng cần build. Chạy `make` không kèm tham số chỉ in danh sách
target từ `make/help.txt`, không build app. Mỗi target đều được mô tả trong
[`Makefile`](../Makefile).

---

## 1. Yêu cầu chuẩn bị trước

`make bootstrap` cài những thành phần có thể cài tự động và thông báo những thành phần
còn lại. Cần cài sẵn các mục sau trước khi chạy lệnh này:

| OS đang dùng | Cài trước | Bootstrap sẽ cài tiếp |
| --- | --- | --- |
| **Ubuntu / Debian** | không cần gì ngoài apt; [Rust](https://rustup.rs) | build-essential, clang, llvm, CMake (`pipx install "cmake>=3.25,<4"` khi bản của apt cũ hơn 3.25), ninja, JDK 17, pipx, python3-venv, rpm, nasm, các package `-dev` của GTK3 / PipeWire / VA-API / tray, VA-API driver, GNOME portal, bản FFmpeg tối giản link tĩnh, cargo-ndk, quiche, opus |
| **macOS** | [Homebrew](https://brew.sh), Xcode và command line tools, [Rust](https://rustup.rs) | cmake, ninja, pipx, LLVM của Homebrew (Apple clang không kèm runtime libFuzzer), Temurin JDK 17, cargo-ndk, quiche và opus cho Apple |
| **Windows** | winget (App Installer), [Git for Windows](https://git-scm.com/download/win) (các build script chạy trong Git Bash của nó), Python 3 | Visual Studio Build Tools kèm C++ workload và *C++ Clang tools*, Rust, NASM, GNU make, Temurin JDK 17 và Android Studio qua winget, do `scripts/bootstrap.ps1` thực hiện |

Cây CMake dùng chung cần CMake 3.25 trở lên. Trên Windows, lần chạy đầu tiên chưa có
`make`: hãy chạy trực tiếp `powershell -ExecutionPolicy Bypass -File scripts\bootstrap.ps1`,
rồi mở một terminal mới.

Trên mọi OS, bootstrap cũng pin các công cụ style và phân tích mà CI dùng — clang-format,
clang-tidy, ktlint, SwiftFormat, cppcheck và detekt, thêm SwiftLint trên macOS — mỗi công
cụ ở một phiên bản cố định. Những công cụ được tải về (ktlint, detekt, SwiftFormat, SwiftLint
và mã nguồn cppcheck) được kiểm tra checksum; SwiftLint (0.65.0) được cài vào
`tools/swiftlint`, không qua Homebrew, và `make lint` tự cài nó vào đó trên macOS khi còn
thiếu; clang-format và clang-tidy được cài từ PyPI
đúng phiên bản đã pin vào `tools/venv`, còn trên Windows cppcheck lấy từ winget đúng phiên
bản đã pin. Không nên cài thủ công các công cụ này, vì CI đối chiếu đúng những phiên bản đó.
Periphery, công cụ tìm code Swift thừa, được tải kèm kiểm tra checksum vào lần đầu chạy
`make lint-dead-swift`.

Các target mobile có yêu cầu bổ sung: `build-android` cần Android SDK kèm NDK (bootstrap
sẽ cài các package SDK — platform-tools, platform, NDK và CMake — khi `ANDROID_HOME` trỏ
tới thư mục cài cmdline-tools, rồi build quiche và opus cho Android), còn `build-ios` cần
Xcode kèm Simulator runtime. Phiên bản NDK lấy từ `-PandroidNdkVersion=<v>` trên dòng lệnh
Gradle, nếu không có thì từ biến môi trường `ANDROID_NDK_VERSION`, nếu không nữa thì là
`26.1.10909125`; bootstrap cài đúng phiên bản đó.

Chỉ header `nvenc` là git submodule. Dùng `--recurse-submodules` khi clone, hoặc
`git submodule update --init` sau đó. `make bootstrap` cũng sync submodule này.

## 2. Cấu trúc cây thư mục

```
core/       C++20 không phụ thuộc nền tảng — protocol, packetization, FEC, session state,
            input mapping, bitrate control, VT emulator. Không OS header. Có unit test.
platform/   lớp abstraction mỏng cho OS, chung một API — socket, clock, logging,
            random, liệt kê source, cùng phần media, audio và input mà nhiều app dùng
            chung. Phụ thuộc core.
client/     năm app: android, ios, linux, macos, windows.
            client/apple/ là phần Swift dùng chung giữa app macOS và iOS, không phải một app.
            client/cli/ là command line client, một binary cho cả ba nền tảng desktop.
tests/integration/  host và viewer qua loopback, với capture và encode giả lập
cmake/      các module CMake dùng chung: warning, quiche, opus
third_party/  quiche (QUIC), opus (audio), header nvenc, bản FFmpeg tối giản
patches/    patch áp vào quiche trước khi build
licenses/   văn bản giấy phép bên thứ ba đi kèm các gói
packaging/  bộ cài Windows, template Homebrew, release notes, site apt và Pages
assets/     icon gốc để sinh mọi icon của client
make/       mỗi nền tảng một file .mk, được Makefile gốc include
scripts/    bootstrap, đóng gói, coverage, style và các tiện ích cho CI
tools/      các công cụ style đã pin do bootstrap cài (bị git bỏ qua)
.github/    workflow, và actions/ — các composite step dùng chung
```

Logic được viết một lần và dùng chung. Trước khi thêm mã vào `client/*`, cần xác định
đoạn mã đó thuộc về `core/` (không phụ thuộc nền tảng) hay `platform/` (cần OS nhưng
cùng một API ở mọi nơi). [`ARCHITECTURE.vi.md`](ARCHITECTURE.vi.md) mô tả cách phân
layer, mô hình thread và wire protocol. `CLAUDE.md` liệt kê các quy tắc mà repo này bắt
buộc tuân thủ.

## 3. Quy trình hằng ngày

```bash
make test      # core suite, offline, không cần GPU và network — vài giây
make lint      # kiểm tra format C++, Kotlin và Swift, rồi kiểm tra code thừa
```

Hãy chạy cả hai trước khi hoàn tất một thay đổi. Khi cần áp dụng format, dùng `make format`;
`make lint` chỉ kiểm tra, không bao giờ ghi lại file. Repo dùng các phiên bản formatter cố định giống CI.

Logic mới trong `core/` cần có test trong thư mục con tương ứng ở `core/tests/`.

## 4. Build và chạy một app

| Target | Sản phẩm | Yêu cầu |
| --- | --- | --- |
| `make build-windows` | một file `Deskhub.exe` | Windows và MSVC |
| `make build-macos` | app macOS | macOS và Xcode |
| `make build-linux` | một binary `deskhub` | Ubuntu và các package `-dev` |
| `make build-ios` | app iOS cho Simulator | macOS, Xcode và một Simulator runtime |
| `make build-android` | một APK debug | Android SDK, NDK, `adb` |

Mỗi target có hai target đi kèm: `release-<os>` (bản tối ưu) và `run-<os>` (build rồi
chạy); `release-ios` vẫn build cho Simulator, còn `release-android` tạo một APK release chưa
ký. Các app desktop không nhận cờ command line nào; mọi lựa chọn nằm trên bốn trang giao
diện. `run-android` cài và mở app trên thiết bị hoặc emulator đang kết nối qua adb;
`run-ios` thực hiện tương tự trên Simulator đang boot, hoặc trên simulator iPhone đầu tiên
có sẵn; `IOS_DEVICE=<udid>` chọn một simulator cụ thể.

Các target macOS ký ad hoc khi keychain không có identity Apple Development nào.
`MACOS_SIGN=adhoc` hoặc `MACOS_SIGN=developerid` ép một chế độ, `MACOS_TEAM` đặt team dùng
khi ký Developer ID, và `MACOS_XCARGS` truyền thêm build setting cho `xcodebuild`.

Bản Debug không đụng tới bản release đã cài. Trên mọi hệ điều hành desktop, bản Debug của
app và CLI lưu key, client được phép, host đã trust, settings và log trong
`~/.deskhub-dev` (Windows: `%USERPROFILE%\.deskhub-dev`) thay vì `~/.deskhub`; trên macOS,
`build-macos`/`run-macos` tạo ra **Deskhub Dev** với bundle id `com.deskhub.macos.debug`.
Vì vậy chạy bản build cục bộ không bao giờ tắt app đã cài, không đụng tới dữ liệu của nó,
và tự xin quyền riêng tư macOS của riêng mình. Các target `release-<os>` dùng thư mục và
bundle id bình thường.

### Command line client

`client/cli/` build ra `deskhub-cli`, dùng lệnh thay cho các trang của app. Bạn có thể
chạy qua SSH, từ script hoặc dưới systemd. Lệnh `connect` mở cửa sổ viewer trên Windows
và Linux; bản macOS chưa hỗ trợ lệnh đó.

```bash
make build-cli                       # bản debug cho OS hiện tại
make release-cli                     # bản tối ưu
make run-cli ARGS="host list"        # build, sau đó chạy với các tham số đã cho
make cli-smoke                       # build, sau đó chạy nó với chính nó qua loopback
```

Target này nằm sau `-DDESKHUB_CLI=ON` (mặc định tắt), nên app cùng các preset sanitizer,
coverage và fuzz không bị ảnh hưởng. Khi bật, các thư viện media theo từng OS chuyển từ
tùy chọn sang bắt buộc, vì một client không capture và không decode được thì không còn là
client. Trên Linux điều đó bao gồm bản FFmpeg tối giản link tĩnh, nên các target CLI build
`ffmpeg-min` trước, giống như `build-linux`.

`make cli-smoke` chạy CLI vừa build ở chế độ headless với chính nó qua loopback: trao đổi
khóa, một người lạ chờ approve, một lời mời QR, một remote shell và file gửi tới host. Trên
Windows nó bỏ qua các bước cần POSIX signal. CI chạy nó trên cả ba nền tảng desktop.

| Lệnh | Chức năng |
| --- | --- |
| `share` | share máy này — display bất kỳ, shell, hoặc cả hai |
| `connect ADDRESS` | mở cửa sổ xem và điều khiển màn hình host (Windows và Linux) |
| `shell ADDRESS` | mở một shell trên host, ngay trong terminal hiện tại |
| `send ADDRESS FILE...` | gửi file tới host đang nhận file |
| `displays`, `sources ADDRESS` | màn hình cục bộ và những gì một host đã xác thực đang share |
| `key public`, `access`, `host`, `host-key public` | public key của máy này, client được phép và yêu cầu kết nối, host đã lưu và khóa host này |
| `devices`, `trust`, `settings` | các lệnh cũ dùng cùng file cấu hình |
| `help [COMMAND]`, `version` | cách dùng, và phiên bản mà bản build này báo |

Mỗi máy có một khóa. Xem nửa public của nó bằng `key public`; trên host, chuyển dòng đó
vào `access add --stdin` để cho phép bằng tay. Client kết nối khi chưa được cho phép sẽ để
lại một **yêu cầu kết nối** trên host: `access requests` liệt kê chúng và
`access approve --fingerprint SHA256:...` hoặc `access deny --fingerprint SHA256:...` xử lý
một yêu cầu; `share --qr` in ra mã QR mà link `deskhub://pair/...` của nó cho một thiết bị
tự vào, và link đó dùng được thay cho địa chỉ với `connect`, `sources`, `shell` và `send`.
Chuyển kết quả `host-key public` của host sang client rồi đưa vào
`host add office --address 192.168.1.10:47777 --host-key-stdin` để ghim host trước.
Sau đó dùng `connect office`, `sources office`, `shell office` hoặc `send office FILE`.
`host update office` đổi địa chỉ hoặc khóa ghim; `host remove office` xóa profile.
`access remove --fingerprint SHA256:...` thu hồi khóa client.
`sources`, `connect`, `shell` và `send` nhận một địa chỉ, một link mời hoặc alias đã lưu,
`--accept-new-host-key` để lưu khóa của host gặp lần đầu, và `--approval-wait SEC` để đổi
thời gian chờ chủ host approve (mặc định 120). Không có `--accept-new-host-key` thì host lạ
bị từ chối và fingerprint của nó được in ra. Trust đi theo khóa của host, nên host đổi địa
chỉ vẫn được trust. Không có scan network và không có cờ passcode.
`--config-dir PATH` chọn chung thư mục cấu hình cho mọi lệnh, đặt trước hoặc sau lệnh.

`deskhub-cli help COMMAND` in ra các cờ. Các lệnh liệt kê hỗ trợ `--json`, và exit code cho biết
nguyên nhân lỗi: `1` lỗi khác, `2` sai cờ, `3` không có phản hồi, `4` bị từ chối hoặc
không được approve kịp thời, `6` không có gì để share hoặc để xem, `8` host không thể bắt
đầu lắng nghe, `9` bản build này không hỗ trợ, `130` bị ngắt bằng Ctrl-C.

Linux hỗ trợ mọi lệnh trong bảng. Trên Windows, `connect` dùng lại mã cửa sổ của app
desktop. Trên macOS, bạn có thể share và mở remote shell; lệnh `connect` sẽ báo rằng bản
build này chưa xem được màn hình.

Khi chỉ làm việc với `core/` và `platform/`, cây CMake dùng chung sẽ nhanh hơn:

```bash
make debug        # configure và build preset debug
make release      # …preset release
```

**quiche và opus được build theo từng ABI.** QUIC transport là một thư viện tĩnh viết bằng
Rust, build trong `third_party/quiche`; thiếu nó thì không share và không connect được.
Opus audio codec là một thư viện tĩnh viết bằng C, build trong `third_party/opus`; thiếu
nó thì bản share không có âm thanh. `debug`, `release`, mọi target `build-*`, `release-*`
và CLI, cùng `test`, mọi target `test-*` và `lint-tidy` đều build trước ABI tương ứng, và
không làm lại nếu đã build (`coverage`, `fuzz` và `fuzz-coverage` không build thư viện nào:
chúng chỉ đo `core/`) — nên `make bootstrap && make test` chạy được trên một bản clone
mới ở mọi OS. Các target `make quiche`, `quiche-android`, `quiche-ios`, `quiche-macos` cùng
`opus`, `opus-android`, `opus-ios`, `opus-macos` chạy riêng các bước này; nếu một trong hai
thư viện build lỗi thì `make` dừng ngay tại đó. Việc CMake dừng lại khi thiếu quiche là có
chủ đích: nó từ chối tạo ra một binary không thể connect.

**Hardening.** `cmake/DeskhubHardening.cmake` cho các bản build GCC và Clang
`-fstack-protector-strong` và, ở bản build tối ưu, `-D_FORTIFY_SOURCE=3` — trừ Android, nơi
giữ mức mặc định 2 của NDK; full RELRO (`-Wl,-z,relro,-z,now`) chỉ được thêm trên Linux.
Bản build MSVC dùng `/sdl`.

## 5. Test

| Lệnh | Phạm vi chạy | Nội dung kiểm tra |
| --- | --- | --- |
| `make test` | offline, không socket | toàn bộ `core/`: wire format, framing, FEC, session, VT emulator, settings, chuỗi văn bản |
| `make test-platform` | socket loopback | QUIC handshake thật, xác thực bằng chữ ký khóa end-to-end, ghim host key, terminal host và viewer qua đường truyền, PTY với shell thật, lockout khi chữ ký sai |
| `make test-integration` | loopback, capture/encode giả lập | session host↔client đầy đủ: negotiation, video qua đường truyền, input, chấp nhận theo khóa được phép, khả năng chịu dữ liệu không hợp lệ |
| `make test-all` | cả ba suite, core chạy trước | |
| `make test-ctest` | cùng các test đó nhưng qua CTest | đúng cách CI gọi chúng |
| `make test-asan` | cả ba suite dưới ASan và UBSan | chỉ clang/gcc, không hỗ trợ MSVC |
| `make test-tsan` | cả ba suite dưới ThreadSanitizer | chỉ clang/gcc, không hỗ trợ MSVC |
| `make test-perf` | bản release, offline và loopback | đo thực tế các hot path: `core_perf` bao phủ packetize/reassemble/FEC, downscale 1080p, CRC và batch file, VT parser và screen, encode/decode wire, audio jitter buffer và PCM ring, đường đi của input, framing của record stream; `platform_perf` bao phủ QUIC thật qua loopback |
| `make perf-build` | bản build release, không chạy gì | build `core_perf` và `platform_perf` mà không chạy |
| `make cli-smoke` | loopback, headless | command line client chạy với chính nó (xem ở trên) |

Không test nào trong các suite cần peer từ xa, GPU hay network.

**Coverage.** `make coverage` tạo báo cáo cho `core/` bằng clang và llvm-cov.
`scripts/check-coverage.sh` áp dụng đúng ngưỡng mà CI yêu cầu: **≥ 90 % line, ≥ 80 %
branch**.

**Fuzzing.** `make fuzz` chạy các libFuzzer target nhắm vào parser của wire, H.264,
reassembly, byte stream terminal và chuỗi UI, các định dạng văn bản của khóa, danh sách
truy cập và lời mời, bộ encode QR, cùng các session state machine phía host và phía viewer (clang, Linux/macOS; `FUZZ_SECONDS=N` cho mỗi target). Mỗi target trước hết
phát lại `core/fuzz/regressions/<target>` để các crash đã sửa không tái xuất hiện, sau đó
fuzz từ seed và dictionary đã commit. `make fuzz-coverage` cho biết corpus thực sự chạm
tới những dòng nào trong core. Mọi crash phát hiện được đều trở thành một input
regression.

**Hiệu năng.** `make test-perf` build hai binary perf bằng preset release rồi chạy chúng.
`core_perf` đo 37 workload trên các hot path thuần C++; `platform_perf` đo thêm 6 workload
trên QUIC thật qua loopback. Tổng thời gian vài giây. Có ba tiêu chí khiến chúng fail, và
không tiêu chí nào là một ngưỡng mili-giây được chọn tùy tiện:

- **Số lần allocate trên mỗi đơn vị**, đếm chính xác bằng cách thay thế `operator new`
  toàn cục. Một path bắt đầu allocate theo từng packet hoặc từng frame sẽ fail trên mọi
  máy, trong mọi lần chạy.
- **Cách chi phí tăng theo input**: mỗi dòng `-scaling` chạy cùng khối lượng công việc với
  input gấp 4 lần và fail khi thời gian tăng nhanh hơn input rất nhiều — dấu hiệu của một
  thuật toán O(n²) vô tình được đưa vào.
- **Độ lệch so với baseline đã ghi**: `make perf-baseline` ghi `out/perf/baseline.txt`
  và `out/perf/platform-baseline.txt` trên một máy đang rảnh; các lần chạy sau báo mức
  chênh lệch theo từng dòng và fail khi vượt 25 %. Các file này mô tả riêng một máy nên
  không được đưa vào git.

`DESKHUB_PERF_TOLERANCE`, `DESKHUB_PERF_REPEATS`, `DESKHUB_PERF_BASELINE` và
`DESKHUB_PERF_WRITE` điều chỉnh phần đo thời gian. `make test` không chạy phần nào trong
số này: bản debug, ASan và coverage không phản ánh tốc độ của bản production. CI chạy cả
hai binary trên các job release Linux và macOS, nơi chỉ tiêu chí allocate và scaling có thể
làm fail, vì baseline thời gian chỉ mô tả một máy. Trên pull request, CI còn build commit
gốc và thay đổi trên cùng một runner rồi đăng độ lệch giữa hai bên thành một comment — chỉ
ở mức cảnh báo, vì runner dùng chung quá nhiễu để fail theo thời gian.

## 6. Style và phân tích tĩnh

| Lệnh | Nội dung kiểm tra |
| --- | --- |
| `make format` | áp dụng format cho C++, Kotlin và Swift |
| `make lint` | cùng các kiểm tra đó nhưng không ghi lại file, rồi chạy `lint-dead` — đúng phần CI yêu cầu |
| `make lint-dead` | code thừa: hàm C++, hàm FFI, mã chuỗi và code Kotlin không ai dùng |
| `make lint-dead-swift` | code Swift thừa trong cả hai app Apple, qua Periphery (macOS + Xcode) |
| `make lint-tidy` | clang-tidy trên `core/src` và `platform/src` |

Có cả biến thể cho từng ngôn ngữ: `format-cpp`, `lint-cpp`, `format-kotlin`,
`lint-kotlin`, `format-swift`, `lint-swift`.

Code thừa là lỗi, giống cách lint `dead_code` của Rust coi nó là lỗi. `make lint-dead`
chạy cppcheck trên mọi file C++ mà bản production build, và báo lỗi với bất kỳ hàm nào
không có gì trong đó gọi tới — test, fuzzer và benchmark không được tính, nên một hàm chỉ
test gọi cũng là code thừa. Lời gọi từ Swift, Kotlin và Objective-C++ được tính là có dùng.
Cùng script đó báo lỗi với hàm FFI không app nào gọi, mã chuỗi `DHStr*` không app nào hiển
thị, và hằng Kotlin không ai đọc; detekt báo lỗi với code Kotlin private, import và tham số
không dùng. Khi một test thực sự không có cách nào khác để quan sát một hành vi, hãy giữ
accessor đó và thêm dòng `name: test nào cần nó và nó chứng minh điều gì` vào
`scripts/dead-code-allow.txt`; dòng thiếu lý do, hoặc dòng có hàm mà production đã bắt đầu
gọi, cũng làm kiểm tra thất bại. `make lint-dead-swift` build cả hai app Apple để lập index
rồi chạy Periphery trên kết quả. Ngoài ra, các bản build bằng clang cảnh báo về member
function, template và tham số exception không dùng, và `-Werror` biến chúng thành lỗi:
mọi preset CMake trừ `asan-msvc` đều đặt `DESKHUB_WERROR=ON`, nên `make test` tại máy
cũng fail vì một cảnh báo giống như CI.

Quy ước của dự án, bản rút gọn; bản đầy đủ nằm trong `CLAUDE.md`:

- C++20, không dùng extension của compiler. Namespace `deskhub` cho core, `deskhubp` cho
  platform.
- Hàm và type theo `PascalCase`, biến cục bộ theo `camelCase`, member private có dấu gạch
  dưới ở cuối.
- **Không viết comment ở bất kỳ đâu.** Thay vào đó dùng tên gọi rõ nghĩa, hàm nhỏ, return
  sớm và hằng số có tên. Những kiến thức cần được giữ lại phải nằm trong message lỗi của
  chính path sẽ thất bại nếu thiếu nó, hoặc trong `ARCHITECTURE.md`.
- Mọi identifier và log message viết bằng tiếng Anh. Mọi tài liệu văn xuôi được xuất bản
  bằng bốn ngôn ngữ — Anh, Việt, Trung, Nhật — trong đó bản tiếng Anh là bản chuẩn.

## 7. Đóng gói

| Lệnh | Sản phẩm |
| --- | --- |
| `make dist-macos` | một file dmg đã sign bằng Developer ID, đã notarize và staple |
| `make verify-macos` | kiểm tra Gatekeeper trên bản vừa build |
| `make dist-linux` | gói `.deb` và `.rpm` riêng cho app và CLI, mỗi gói cài udev rule cho uinput |

`dist-macos` cần một identity *Developer ID Application* trong keychain và một App Store
Connect API key để notarize: `ASC_KEY_P8` (đường dẫn tới file `.p8`), `ASC_KEY_ID` và
`ASC_ISSUER_ID`. Nó dừng trước khi build nếu thiếu bất kỳ biến nào trong ba biến đó.

Workflow release cũng tạo bộ cài Inno Setup cho app và CLI trên Windows từ
`packaging/windows/`. Các bản Windows portable và app Linux vẫn là file đơn lẻ.

Mọi gói đều kèm `THIRD_PARTY_NOTICES.md` và các văn bản giấy phép trong `licenses/` áp dụng
cho nó: `.deb` và `.rpm` của app và CLI (dưới `/usr/share/doc/<package>/`, các văn bản —
kể cả LGPL — nằm trong thư mục `licenses/` của nó), cả hai bộ cài Windows, `Resources` của
app macOS, bundle của app iOS và assets của APK Android. `scripts/stage-licenses.sh` chuẩn bị
bộ cho Linux và Apple, `packaging/windows/*.iss` liệt kê bộ cho Windows, còn bản build
Android tự copy bộ của nó. Các binary portable không mang theo gì, nên bản release thêm
`deskhub-<tag>-licenses.zip` chứa `LICENSE`, file notices và toàn bộ thư mục `licenses/`.
`licenses/rust-crates.txt` được sinh ra:
`scripts/rust-crate-notices.py <quiche source dir> > licenses/rust-crates.txt` ghi lại nó
khi pin của quiche thay đổi. APK còn mang theo `licenses/android-libraries.txt` — các thư
viện Java và Kotlin mà Gradle đóng gói vào nó, kèm notice của từng thư viện — và file này
cũng được sinh ra: `scripts/android-library-notices.py > licenses/android-libraries.txt` đọc
release runtime classpath của `client/android` (cần đặt `JAVA_HOME`, sau một lần build
Android để Gradle đã cache các artifact) và ghi lại nó mỗi khi một phụ thuộc Android thay đổi.

## 8. Release

1. Tăng [`VERSION`](../VERSION). `scripts/check-version.sh` làm fail bước deploy nếu tag
   và file không khớp.
2. Cập nhật các tài liệu chịu ảnh hưởng của thay đổi này, ở cả bốn ngôn ngữ, trong
   cùng một commit.
3. Viết release notes (xem bên dưới) và commit.
4. Tạo tag `vX.Y.Z` và push. `.github/workflows/deploy.yml` build mọi nền tảng, tạo GitHub
   Release, đưa iOS lên TestFlight, macOS qua notarization, và Android lên track internal
   của Play.

**Release notes được viết tay**, mỗi tag một file: `packaging/release-notes/vX.Y.Z.md`, tên
file trùng khớp tuyệt đối với tag. File đó chính là nội dung của GitHub Release, theo sau là
một đoạn footer cố định trỏ tới hướng dẫn cài đặt và mô hình bảo mật. Hãy mở đầu bằng những
việc người dùng phải làm sau khi cập nhật — breaking change, cách kết nối lại — rồi mới tới
những gì mới và những gì đã sửa. `scripts/check-release-notes.sh` làm fail bước deploy trước
khi build bất cứ thứ gì nếu tag không có file này, nên cần commit nó trước khi tạo tag.

### Package manager

Khi GitHub Release đã có, `deploy.yml` chuyển tag cho `publish-packages.yml`, và các job trong
đó sẽ publish nó. Muốn publish lại một tag mà không tạo release mới — chẳng hạn sau khi sửa
một trong các script này — chạy tay: `gh workflow run publish-packages.yml -f tag=vX.Y.Z`.

| Job | Publish | Secret trong environment `stg` |
| --- | --- | --- |
| `winget` | pull request vào `microsoft/winget-pkgs` cho `ManhPham.Deskhub` và `ManhPham.DeskhubCLI`, qua [komac](https://github.com/russellbanks/Komac) (pin trong `scripts/pinned-versions.txt`) | `WINGET_TOKEN` — classic PAT có `public_repo` |
| `homebrew` | `Casks/deskhub.rb` và `Formula/deskhub-cli.rb` trong `manhpham90vn/homebrew-tap`, sinh từ `packaging/homebrew/` | `HOMEBREW_TAP_TOKEN` — fine-grained PAT, Contents: write trên tap |
| `build-apt-repo` → `deploy-apt-repo` | apt repository đã ký, chứa deb của ba release gần nhất, trên GitHub Pages tại `/apt` (`scripts/build-apt-repo.sh`) | `APT_GPG_PRIVATE_KEY`, `APT_GPG_PASSPHRASE` |

Mỗi job cần thiết lập một lần trước tag đầu tiên chạy nó:

- **winget** — job chỉ cập nhật package đã tồn tại, nên bản đầu tiên của mỗi package phải
  gửi bằng tay. Dùng bộ cài Inno Setup cho `ManhPham.Deskhub`; chọn `deskhub-cli` làm
  portable command alias cho `ManhPham.DeskhubCLI` khi komac hỏi.
  Trước khi Microsoft merge pull request đó, job chỉ cảnh báo rồi bỏ qua.

  ```bash
  komac new ManhPham.Deskhub --version X.Y.Z --urls https://github.com/manhpham90vn/Deskhub/releases/download/vX.Y.Z/deskhub-vX.Y.Z-windows-setup.exe
  ```

  Làm lại với `ManhPham.DeskhubCLI` và URL `deskhub-cli-vX.Y.Z-windows.exe`.
- **Homebrew** — tạo repository public `manhpham90vn/homebrew-tap`; job sẽ tự điền nội dung.
- **apt** — tạo signing key một lần, lưu file đó làm `APT_GPG_PRIVATE_KEY` và giữ một bản
  backup offline. Mọi người dùng đều trust key này: thay key sẽ làm hỏng `apt update` của họ.

  ```bash
  gpg --quick-gen-key "Deskhub APT <manhpv151090@gmail.com>" rsa4096 sign 5y
  gpg --armor --export-secret-keys "Deskhub APT" > deskhub-apt.key
  ```

  Trong *Settings → Pages* đặt source là **GitHub Actions**, và trong *Settings →
  Environments → github-pages* cho phép tag khớp `v*` — nếu không, Pages từ chối deploy từ
  một tag.

## 9. Những gì CI kiểm tra

`make test` và `make lint` chạy thành công tại máy chưa phải là toàn bộ. Trên mỗi pull
request, CI thực hiện:

- clang-tidy trên `core/src` và `platform/src`, SwiftLint `--strict`, Android Lint
- actionlint và shellcheck trên các workflow cùng `scripts/*.sh`
- code thừa: `scripts/dead-code.sh` (cppcheck, các kiểm tra FFI / mã chuỗi / hằng Kotlin
  và detekt) cùng Periphery trên cả hai app Apple
- cả ba suite trên Linux x64 và arm64 (cả hai đều build native), macOS và Windows; dưới
  ASan/UBSan và TSan trên Linux, ASan/UBSan trên macOS; suite platform và integration dưới
  ASan của MSVC trên Windows; cùng cross-build cho Android — chạy trên emulator x86_64, chỉ
  build cho arm64-v8a — và cho iOS Simulator
- script của `make cli-smoke` chạy với CLI trên cả ba nền tảng desktop
- chạy lại toàn bộ integration suite thêm ba lần trên Windows để tìm một lỗi memory
  corruption không thường xuyên, xuất hiện khoảng một lần trong ba lần chạy nên dễ bị bỏ
  sót nếu chỉ chạy một lượt. Frame xảy ra crash là hệ quả của lỗi corruption chứ không
  phải nguyên nhân, vì vậy mỗi job Windows ghi một minidump đầy đủ đặt cạnh symbol, và
  bản nightly chạy lại các load test thêm hai lượt: một lượt dưới full page heap, trong đó
  một thao tác ghi vượt quá vùng đã allocate sẽ fault ngay tại instruction gây ra nó; và
  một lượt với quiche được build kèm Rust debug assertion và overflow check, đây là cơ chế
  duy nhất quan sát được bên trong quiche, vì ASan không instrument Rust còn page heap chỉ
  bảo vệ phần heap
- coverage của core ở mức ≥ 90 % line và ≥ 80 % branch
- các libFuzzer target, mỗi target 30 giây (bản nightly là 15 phút mỗi target)
- CodeQL trên C++/Kotlin/Swift, một lượt gitleaks quét toàn bộ lịch sử, và một vòng
  dependency review

## 10. Công cụ cho nhà phát triển

| Lệnh | Chức năng |
| --- | --- |
| `make icons` | sinh lại toàn bộ icon của mọi client từ `assets/icon_1024.png` |
| `make quic-smoke` | một cặp QUIC client và server độc lập chạy trên thư viện tĩnh quiche |
| `make opus-smoke` | một vòng encode/decode độc lập trên thư viện tĩnh opus — báo bitrate thực tế, packet lớn nhất và việc DTX có được kích hoạt hay không |
| `make screenshots` | macOS: chụp lại bộ ảnh cho store trên simulator iPhone/iPad, emulator Android và app macOS, sau đó cập nhật `docs/imgs` (`ARGS="ios android macos readme"` cho một phần) |
| `make setup-linux-permissions` | udev rule cho `/dev/uinput` và group `input`, để host được từ một bản build từ source |
| `make reset-macos-permissions` | xoá các grant TCC của cả bundle id bản release (`com.deskhub.macos`) lẫn bản Debug tại máy (`com.deskhub.macos.debug`), và liệt kê mọi bản sao của app cùng cách mỗi bản được ký — dùng khi Screen Recording hoặc Accessibility ngừng hoạt động sau các lần build lại (`ARGS="--purge"` xoá luôn các bản đã build) |
| `make ffmpeg-min` | Ubuntu: bản FFmpeg tối giản link tĩnh mà app và CLI sử dụng (`build-linux`, `release-linux` và các target CLI tự chạy) |
| `make opus` | Opus audio codec cho target host (`debug`, `release`, các target Linux, CLI và test tự chạy) |
| `make clean` | xoá `out/` |

## 11. Xử lý sự cố khi build

- **CMake dừng vì thiếu thư viện quiche** — chạy target `make quiche*` tương ứng; mỗi ABI
  cần bản riêng. Với opus và các target `make opus*` cũng vậy.
- **`make fuzz` trên macOS không tìm thấy libFuzzer** — nó cần LLVM của Homebrew.
  `make bootstrap` cài thành phần này, phần còn lại vẫn build bằng toolchain của Xcode.
- **`make lint` cho kết quả khác với editor** — chạy lại `make bootstrap` để lấy đúng
  phiên bản công cụ mà CI sử dụng, rồi chạy `make format`.
- **Các target Android không tìm thấy SDK** — đặt `ANDROID_HOME`, sau đó chạy lại
  `make bootstrap`. `ANDROID_NDK_VERSION=<v>` chọn một NDK khác.
- **Permission trên macOS hoạt động không đúng sau khi build lại, hoặc sau khi chuyển qua
  lại giữa bản build tại máy và bản tải về** — chạy `make reset-macos-permissions`, rồi cấp
  lại quyền cho đúng một bản sao mà bạn giữ lại.

Báo lỗi và đặt câu hỏi: [issues](https://github.com/manhpham90vn/Deskhub/issues).
