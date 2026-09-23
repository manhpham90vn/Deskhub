[English](BUILD.md) · **Tiếng Việt** · [中文](BUILD.zh.md) · [日本語](BUILD.ja.md)

# Deskhub — Build và phát triển

Tài liệu này mô tả mọi thứ cần thiết để tự compile Deskhub, chạy các test suite và phát
hành một bản release. Nếu chỉ cần *sử dụng* app, hãy lấy bản build sẵn theo hướng dẫn ở
[`INSTALL.vi.md`](INSTALL.vi.md).

Đây là bản dịch của [`BUILD.md`](BUILD.md). Nếu hai bản có khác biệt, bản tiếng Anh là bản
chuẩn.

```bash
git clone --recurse-submodules https://github.com/manhpham90vn/Deskhub.git
cd Deskhub
make bootstrap        # một lần: toolchain và dependency cho OS hiện tại
make test             # build và chạy core suite ở chế độ offline
make build-linux      # hoặc build-windows / build-macos / build-ios / build-android
```

Không nền tảng nào được build ngầm: chạy `make` không kèm tham số thì nó chỉ in danh
sách target chứ không build gì. Mọi target được mô tả đầy đủ ở đầu
[`Makefile`](../Makefile), còn `make/help.txt` chính là nội dung mà `make` in ra.

---

## 1. Yêu cầu chuẩn bị trước

`make bootstrap` cài những thành phần có thể cài tự động và thông báo những thành phần
còn lại. Cần cài sẵn các mục sau trước khi chạy lệnh này:

| OS đang dùng | Cài trước | Bootstrap sẽ cài tiếp |
| --- | --- | --- |
| **Ubuntu / Debian** | không cần gì ngoài apt; [Rust](https://rustup.rs) | build-essential, clang, llvm, cmake, ninja, JDK 17, các package `-dev` của GTK3 / PipeWire / VA-API / tray, VA-API driver, GNOME portal, bản FFmpeg tối giản link tĩnh, quiche, opus |
| **macOS** | [Homebrew](https://brew.sh), Xcode và command line tools, [Rust](https://rustup.rs) | cmake, ninja, swiftlint, pipx, LLVM của Homebrew (Apple clang không kèm runtime libFuzzer), Temurin JDK 17, quiche và opus cho Apple và Android |
| **Windows** | winget (App Installer), Visual Studio kèm C++ toolchain và component *C++ Clang tools*, [Rust](https://rustup.rs) | phần còn lại qua winget, do `scripts/bootstrap.ps1` thực hiện |

Trên mọi OS, bootstrap cũng pin các công cụ style và phân tích: clang-format, clang-tidy,
ktlint, SwiftFormat, cppcheck và detekt, mỗi công cụ ở một phiên bản cố định kèm kiểm tra
checksum. Không nên cài thủ công các công cụ này, vì CI đối chiếu đúng những phiên bản đó.
Periphery, công cụ tìm code Swift thừa, được tải theo cùng cách vào lần đầu chạy
`make lint-dead-swift`.

Các target mobile có yêu cầu bổ sung: `build-android` cần Android SDK kèm NDK (bootstrap
sẽ cài các package SDK khi `ANDROID_HOME` trỏ tới thư mục cài cmdline-tools), còn
`build-ios` cần Xcode kèm Simulator runtime.

Chỉ header `nvenc` là git submodule. Dùng `--recurse-submodules` khi clone, hoặc
`git submodule update --init` sau đó. `make bootstrap` cũng sync submodule này.

## 2. Cấu trúc cây thư mục

```
core/       C++20 không phụ thuộc nền tảng — protocol, packetization, FEC, session state,
            input mapping, bitrate control, VT emulator. Không OS header. Có unit test.
platform/   lớp abstraction mỏng cho OS, chung một API — socket, clock, logging,
            random, liệt kê source. Phụ thuộc core.
client/     năm app: android, ios, linux, macos, windows.
            client/apple/ là phần Swift dùng chung giữa app macOS và iOS, không phải một app.
            client/cli/ là command line client, một binary cho cả ba nền tảng desktop.
third_party/  quiche (QUIC), opus (audio), header nvenc, bản FFmpeg tối giản
make/       mỗi nền tảng một file .mk, được Makefile gốc include
scripts/    bootstrap, đóng gói, coverage, style và các tiện ích cho CI
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
make lint      # kiểm tra format C++, Kotlin và Swift, không ghi lại file
```

Cần chạy cả hai trước khi coi một thay đổi là hoàn tất. `make format` áp dụng format thay
vì chỉ kiểm tra. Không nên format thủ công; các công cụ được pin phiên bản có chủ đích.

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
chạy). Các app desktop không nhận cờ command line nào; mọi lựa chọn nằm trên bốn trang
giao diện. `run-android` cài và mở app trên thiết bị hoặc emulator đang kết nối qua adb;
`run-ios` thực hiện tương tự trên Simulator.

### Command line client

`client/cli/` build ra một binary `deskhub-cli` thực hiện cùng chức năng nhưng không cần
GUI toolkit, điều khiển bằng cờ thay vì bằng giao diện. Đây là cách chạy Deskhub qua SSH,
từ một script, hoặc dưới systemd.

```bash
make build-cli                       # bản debug cho OS hiện tại
make release-cli                     # bản tối ưu
make run-cli ARGS="scan"             # build, sau đó chạy với các tham số đã cho
```

Target này nằm sau `-DDESKHUB_CLI=ON` (mặc định tắt), nên app cùng các preset sanitizer,
coverage và fuzz không bị ảnh hưởng. Khi bật, các thư viện media theo từng OS chuyển từ
tùy chọn sang bắt buộc, vì một client không capture và không decode được thì không còn là
client.

| Lệnh | Chức năng |
| --- | --- |
| `share` | share máy này — display bất kỳ, shell, hoặc cả hai |
| `connect ADDRESS` | mở cửa sổ hiển thị màn hình host và điều khiển host đó |
| `shell ADDRESS` | mở một shell trên host, ngay trong terminal hiện tại |
| `displays`, `scan`, `sources`, `probe` | những gì share được, và các máy đang hiện diện |
| `devices`, `trust`, `settings` | cùng những file mà app desktop đọc và ghi |

`deskhub-cli help COMMAND` in ra các cờ. Mọi lệnh đều nhận `--json`, và exit code cho biết
nguyên nhân lỗi: `2` sai cờ, `3` không có phản hồi, `4` bị từ chối, `5` host key đã thay
đổi, `9` bản build này không hỗ trợ.

Tình trạng theo từng OS hiện tại: Linux hỗ trợ đầy đủ. Windows share và connect được,
dùng lại phần mã cửa sổ của app desktop. macOS share và mở shell được, nhưng `connect`
cần một window layer chưa được viết và sẽ báo lỗi tương ứng.

Khi chỉ làm việc với `core/` và `platform/`, cây CMake dùng chung sẽ nhanh hơn:

```bash
make debug        # configure và build preset debug
make release      # …preset release
```

**quiche và opus được build theo từng ABI.** QUIC transport là một thư viện tĩnh viết bằng
Rust, build trong `third_party/quiche`; thiếu nó thì không share và không connect được.
Opus audio codec là một thư viện tĩnh viết bằng C, build trong `third_party/opus`; thiếu
nó thì bản share không có âm thanh. `debug`, `release` và mọi target `build-*` đều build
trước ABI tương ứng, và không làm lại nếu đã build. Các target `make quiche`,
`quiche-android`, `quiche-ios`, `quiche-macos` cùng `opus`, `opus-android`, `opus-ios`,
`opus-macos` chạy riêng các bước này. Việc CMake dừng lại khi thiếu quiche là có chủ đích: nó từ chối tạo ra
một binary không thể connect.

## 5. Test

| Lệnh | Phạm vi chạy | Nội dung kiểm tra |
| --- | --- | --- |
| `make test` | offline, không socket | toàn bộ `core/`: wire format, framing, FEC, session, VT emulator, settings, chuỗi văn bản |
| `make test-platform` | socket loopback | QUIC handshake thật, SPAKE2 end-to-end, terminal host và viewer qua đường truyền, PTY với shell thật, lockout, approval |
| `make test-integration` | loopback, capture/encode giả lập | session host↔client đầy đủ: negotiation, video qua đường truyền, input, kiểm soát bằng passcode và approval, khả năng chịu dữ liệu không hợp lệ |
| `make test-all` | cả ba suite, core chạy trước | |
| `make test-ctest` | cùng các test đó nhưng qua CTest | đúng cách CI gọi chúng |
| `make test-asan` | cả ba suite dưới ASan và UBSan | chỉ clang/gcc, không hỗ trợ MSVC |
| `make test-tsan` | cả ba suite dưới ThreadSanitizer | chỉ clang/gcc, không hỗ trợ MSVC |
| `make test-perf` | bản release, offline và loopback | đo thực tế các hot path: `core_perf` bao phủ packetize/reassemble/FEC, downscale 1080p, CRC và batch file, VT parser và screen, encode/decode wire, audio jitter buffer; `platform_perf` bao phủ QUIC thật qua loopback |

Không test nào trong các suite cần peer từ xa, GPU hay network.

**Coverage.** `make coverage` tạo báo cáo cho `core/` bằng clang và llvm-cov.
`scripts/check-coverage.sh` áp dụng đúng ngưỡng mà CI yêu cầu: **≥ 90 % line, ≥ 80 %
branch**.

**Fuzzing.** `make fuzz` chạy các libFuzzer target nhắm vào parser của wire, H.264,
reassembly, byte stream terminal và chuỗi UI, cùng các session state machine phía host và
phía viewer (clang, Linux/macOS; `FUZZ_SECONDS=N` cho mỗi target). Mỗi target trước hết
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
  trên một máy đang rảnh; các lần chạy sau báo mức chênh lệch theo từng dòng và fail khi
  vượt 25 %. File này mô tả riêng một máy nên không được đưa vào git.

`DESKHUB_PERF_TOLERANCE`, `DESKHUB_PERF_REPEATS`, `DESKHUB_PERF_BASELINE` và
`DESKHUB_PERF_WRITE` điều chỉnh phần đo thời gian. Cả `make test` lẫn CI đều không chạy
phần này: bản debug, ASan và coverage không phản ánh tốc độ của bản production.

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
accessor đó và thêm dòng `tên: test nào cần nó và nó chứng minh điều gì` vào
`scripts/dead-code-allow.txt`; dòng thiếu lý do, hoặc dòng có hàm mà production đã bắt đầu
gọi, cũng làm kiểm tra thất bại. `make lint-dead-swift` build cả hai app Apple để lập index
rồi chạy Periphery trên kết quả. Ngoài ra, các bản build bằng clang cảnh báo về member
function, template và tham số exception không dùng, và `-Werror` trong CI biến chúng thành
lỗi.

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
| `make dist-linux` | `.deb` và `.rpm`, cả hai đều cài udev rule cho uinput |

App Windows và Linux mỗi bản chỉ là một file, không có installer nào để build.

## 8. Release

1. Tăng [`VERSION`](../VERSION). `scripts/check-version.sh` làm fail bước deploy nếu tag
   và file không khớp.
2. Cập nhật các tài liệu chịu ảnh hưởng của thay đổi này, ở tất cả các ngôn ngữ, trong
   cùng một commit.
3. Tạo tag `vX.Y.Z` và push. `.github/workflows/deploy.yml` build mọi nền tảng, tạo GitHub
   Release, đưa iOS lên TestFlight, macOS qua notarization, và Android lên track internal
   của Play.

**Release notes được sinh từ các commit subject** nằm giữa tag trước và tag hiện tại, bởi
`scripts/changelog.sh`. Có thể chạy tại máy để xem một tag sẽ sinh ra nội dung gì:

```bash
scripts/changelog.sh v5.0.0     # hoặc không tham số, cho tag tại HEAD
```

Nghĩa là commit subject là nội dung người dùng đọc, và conventional-commit type đứng trước
nó quyết định mục mà nó thuộc về. Bảng ánh xạ đầy đủ, các quy tắc ghi đè và ví dụ cụ thể
nằm trong [`.claude/skills/commit/SKILL.md`](../.claude/skills/commit/SKILL.md); cần đọc
trước khi viết subject. Các mục rỗng không xuất hiện trong bản release, và
`INCLUDE_INTERNAL=1 scripts/changelog.sh` hiển thị những commit đã bị bỏ qua.

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
  gửi bằng tay, và chọn `deskhub` / `deskhub-cli` làm portable command alias khi komac hỏi.
  Trước khi Microsoft merge pull request đó, job chỉ cảnh báo rồi bỏ qua.

  ```bash
  komac new ManhPham.Deskhub --version X.Y.Z --urls https://github.com/manhpham90vn/Deskhub/releases/download/vX.Y.Z/deskhub-vX.Y.Z-windows.exe
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
- cả ba suite dưới ASan/UBSan và TSan, cùng cross-build cho Linux arm64, Android
  emulator và iOS Simulator
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
| `make reset-macos-permissions` | xoá các grant TCC khi một bản build tại máy và một bản tải về cùng dùng một bundle id (`ARGS="--purge"` xoá luôn các bản đã build) |
| `make ffmpeg-min` | Ubuntu: bản FFmpeg tối giản link tĩnh mà app sử dụng (`build-linux` tự chạy) |
| `make opus` | Opus audio codec cho target host (`debug`, `release` và `build-linux` tự chạy) |
| `make clean` | xoá `out/` |

## 11. Xử lý sự cố khi build

- **CMake dừng vì thiếu thư viện quiche** — chạy target `make quiche*` tương ứng; mỗi ABI
  cần bản riêng. Với opus và các target `make opus*` cũng vậy.
- **`make fuzz` trên macOS không tìm thấy libFuzzer** — nó cần LLVM của Homebrew.
  `make bootstrap` cài thành phần này, phần còn lại vẫn build bằng toolchain của Xcode.
- **`make lint` cho kết quả khác với editor** — các công cụ đã pin phiên bản là chuẩn.
  Chạy lại `make bootstrap` để lấy đúng phiên bản, sau đó chạy `make format`.
- **Các target Android không tìm thấy SDK** — đặt `ANDROID_HOME`, sau đó chạy lại
  `make bootstrap`. `ANDROID_NDK_VERSION=<v>` chọn một NDK khác.
- **Permission trên macOS hoạt động không đúng sau khi chuyển qua lại giữa bản build tại
  máy và bản tải về** — chạy `make reset-macos-permissions`.

Báo lỗi và đặt câu hỏi: [issues](https://github.com/manhpham90vn/Deskhub/issues).
