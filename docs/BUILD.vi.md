[English](BUILD.md) · **Tiếng Việt** · [中文](BUILD.zh.md) · [日本語](BUILD.ja.md)

# Deskhub — Build và phát triển

Mọi thứ cần để tự biên dịch Deskhub, chạy các bộ kiểm thử và cắt một bản phát hành. Nếu
bạn chỉ muốn *dùng* app, hãy lấy bản dựng sẵn theo [`INSTALL.vi.md`](INSTALL.vi.md).

Đây là bản dịch của [`BUILD.md`](BUILD.md); khi hai bản khác nhau, bản tiếng Anh là bản
chuẩn.

```bash
git clone --recurse-submodules https://github.com/manhpham90vn/Deskhub.git
cd Deskhub
make bootstrap        # một lần: bộ công cụ + phụ thuộc cho hệ điều hành này
make test             # build và chạy bộ kiểm thử lõi, hoàn toàn offline
make build-linux      # hoặc build-windows / build-macos / build-ios / build-android
```

Không nền tảng nào được build ngầm: gõ `make` trơn thì nó in ra danh sách target và không
build gì cả. Mọi target đều được ghi chép đầy đủ ở đầu file [`Makefile`](../Makefile), còn
`make/help.txt` chính là thứ `make` trơn in ra.

---

## 1. Cần có sẵn những gì

`make bootstrap` cài được cái gì thì cài, cái gì không cài được thì nó nói cho bạn biết.
Hãy tự cài những thứ sau trước khi chạy nó:

| Hệ điều hành | Cài trước | Bootstrap sẽ lo phần còn lại |
| --- | --- | --- |
| **Ubuntu / Debian** | không cần gì ngoài apt; [Rust](https://rustup.rs) | build-essential, clang, llvm, cmake, ninja, JDK 17, các gói `-dev` của GTK3 / PipeWire / VA-API / khay hệ thống, driver VA-API, portal của GNOME, bản FFmpeg tối giản tĩnh, quiche, opus |
| **macOS** | [Homebrew](https://brew.sh), Xcode + command line tools, [Rust](https://rustup.rs) | cmake, ninja, swiftlint, pipx, LLVM của Homebrew (clang của Apple không kèm runtime libFuzzer), Temurin JDK 17, quiche và opus cho Apple và Android |
| **Windows** | winget (App Installer), Visual Studio kèm bộ công cụ C++ và thành phần *C++ Clang tools*, [Rust](https://rustup.rs) | phần còn lại qua winget, do `scripts/bootstrap.ps1` điều khiển |

Trên mọi hệ điều hành nó còn ghim các công cụ định dạng: clang-format, clang-tidy, ktlint
và SwiftFormat, mỗi cái một phiên bản cố định kèm kiểm tra checksum — đừng bao giờ tự cài
tay, vì CI so đúng với những phiên bản này.

Các target di động cần thêm: `build-android` cần Android SDK kèm NDK (bootstrap sẽ cài các
gói SDK khi `ANDROID_HOME` trỏ tới một bản cmdline-tools), còn `build-ios` cần Xcode kèm
một runtime Simulator.

Chỉ có phần header `nvenc` là git submodule; `--recurse-submodules` lúc clone, hoặc
`git submodule update --init`, là đủ. `make bootstrap` cũng đồng bộ chúng.

## 2. Cây mã nguồn được bố trí thế nào

```
core/       C++20 độc lập nền tảng — giao thức, đóng gói tin, FEC, trạng thái phiên,
            ánh xạ thao tác, điều khiển bitrate, bộ giả lập VT. Không có header hệ
            điều hành. Được kiểm thử đơn vị.
platform/   lớp bọc mỏng quanh hệ điều hành, cùng một API — socket, đồng hồ, log,
            số ngẫu nhiên, liệt kê nguồn hình. Phụ thuộc vào core.
client/     năm app: android, ios, linux, macos, windows.
            client/apple/ là phần Swift dùng chung cho app macOS và iOS, không phải một app.
            client/cli/ là client dòng lệnh, một binary cho cả ba máy để bàn.
third_party/  quiche (QUIC), opus (âm thanh), header nvenc, bản FFmpeg tối giản
make/       mỗi nền tảng một file .mk, được Makefile gốc include
scripts/    bootstrap, đóng gói, coverage, định dạng và các tiện ích cho CI
.github/    workflows, và actions/ — những bước dùng chung mà tất cả chúng tái sử dụng
```

Viết một lần rồi dùng chung: trước khi thêm bất cứ thứ gì vào `client/*`, hãy xem nó có
thuộc về `core/` (độc lập nền tảng) hay `platform/` (cần hệ điều hành, nhưng cùng một API
ở mọi nơi) không. [`ARCHITECTURE.vi.md`](ARCHITECTURE.vi.md) giải thích cách phân tầng, mô
hình luồng và giao thức trên đường truyền; `CLAUDE.md` là nơi ghi các quy tắc repo này bắt
buộc tuân theo.

## 3. Vòng lặp hằng ngày

```bash
make test      # bộ kiểm thử lõi, offline, không cần GPU và mạng — vài giây
make lint      # kiểm tra định dạng C++, Kotlin và Swift mà không sửa file
```

Chạy cả hai trước khi coi một thay đổi là xong. `make format` thì áp dụng định dạng thay vì
chỉ kiểm tra — đừng bao giờ tự căn chỉnh tay, công cụ được ghim phiên bản là có lý do.

Logic mới trong `core/` phải có bài kiểm thử tương ứng trong thư mục con của `core/tests/`.

## 4. Build và chạy một app

| Target | Tạo ra | Cần |
| --- | --- | --- |
| `make build-windows` | một file `Deskhub.exe` | Windows + MSVC |
| `make build-macos` | app macOS | macOS + Xcode |
| `make build-linux` | một file `deskhub` | Ubuntu + các gói `-dev` |
| `make build-ios` | app iOS cho Simulator | macOS + Xcode + một runtime Simulator |
| `make build-android` | APK bản debug | Android SDK + NDK, `adb` |

Mỗi target đều có anh em `release-<os>` (bản tối ưu) và `run-<os>` (build rồi chạy).
Các app để bàn không đọc cờ dòng lệnh nào cả — mọi thứ chọn trên bốn trang của chúng.
`run-android` cài và mở app trên thiết bị hoặc máy ảo đang kết nối qua adb, còn `run-ios`
làm điều tương tự trên Simulator.

### Client dòng lệnh

`client/cli/` dựng một binary `deskhub-cli` làm đúng những việc đó nhưng không cần toolkit
đồ hoạ, điều khiển bằng cờ thay vì bằng trang. Đây là cách chạy Deskhub qua SSH, trong
script, hay dưới systemd.

```bash
make build-cli                       # bản debug cho OS hiện tại
make release-cli                     # bản tối ưu
make run-cli ARGS="scan"             # build rồi chạy với các tham số đó
```

Nó nằm sau `-DDESKHUB_CLI=ON` (mặc định tắt), nên app vẫn build như cũ và các preset
sanitizer, coverage, fuzz không bị đụng tới. Bật nó lên thì các thư viện media của từng OS
trở thành bắt buộc chứ không còn tuỳ chọn, vì một client không chụp và không giải mã được
thì không phải là client.

| Lệnh | Làm gì |
| --- | --- |
| `share` | chia sẻ máy này — màn hình nào cũng được, shell, hoặc cả hai |
| `connect ĐỊA_CHỈ` | mở một cửa sổ xem màn hình host và điều khiển nó |
| `shell ĐỊA_CHỈ` | mở shell trên host, ngay trong terminal đang dùng |
| `displays`, `scan`, `sources`, `probe` | chia sẻ được những gì, và ngoài kia có ai |
| `devices`, `trust`, `settings` | đúng những file mà app để bàn đọc và ghi |

`deskhub-cli help LỆNH` in ra các cờ. Mọi lệnh đều nhận `--json`, và mã thoát nói rõ hỏng ở
đâu: `2` sai cờ, `3` không ai trả lời, `4` bị từ chối, `5` khoá của host đã đổi, `9` bản
build này không làm được.

Tình trạng theo OS hiện nay: Linux làm được tất cả. Windows chia sẻ và kết nối qua đúng
phần cửa sổ mà app để bàn đang dùng. macOS chia sẻ và mở shell được, nhưng `connect` cần
một lớp cửa sổ chưa viết, và nó báo đúng như vậy.

Nếu chỉ động vào `core/` và `platform/`, dùng cây CMake dùng chung sẽ nhanh hơn:

```bash
make debug        # cấu hình + build preset debug
make release      # …preset release
```

**quiche và opus được build riêng cho từng ABI.** Lớp truyền QUIC là một thư viện tĩnh
viết bằng Rust, được dựng vào `third_party/quiche`, và không có nó thì không chia sẻ hay
kết nối được gì. Codec âm thanh Opus là một thư viện tĩnh viết bằng C, dựng vào
`third_party/opus`, và không có nó thì phiên chia sẻ không có tiếng. `debug`, `release` và
mọi target `build-*` đều tự build ABI chúng cần trước, và build rồi thì lần sau bỏ qua —
`make quiche`, `quiche-android`, `quiche-ios`, `quiche-macos` cùng các target `opus`,
`opus-android`, `opus-ios`, `opus-macos` chạy riêng các bước đó. Nếu CMake dừng lại vì
thiếu quiche thì đó là cố ý: nó từ chối tạo ra một file chạy được nhưng không bao giờ kết
nối được.

## 5. Kiểm thử

| Lệnh | Chạy trong điều kiện | Bao phủ |
| --- | --- | --- |
| `make test` | offline, không socket | toàn bộ `core/`: định dạng gói tin, đóng khung, FEC, phiên, bộ giả lập VT, cấu hình, chuỗi |
| `make test-platform` | socket loopback | bắt tay QUIC thật, SPAKE2 đầu-cuối, terminal host + viewer qua đường truyền, PTY với một shell thật, khoá tạm, phê duyệt |
| `make test-integration` | loopback, thu hình/mã hoá giả | phiên host↔client đầy đủ: thương lượng, hình ảnh qua đường truyền, thao tác điều khiển, chặn theo mật mã và phê duyệt, chịu dữ liệu rác |
| `make test-all` | cả ba bộ, lõi trước | |
| `make test-ctest` | cùng các bài đó qua CTest | đúng cách CI gọi chúng |
| `make test-asan` | cả ba bộ dưới ASan + UBSan | chỉ clang/gcc, không có MSVC |
| `make test-tsan` | cả ba bộ dưới ThreadSanitizer | chỉ clang/gcc, không có MSVC |
| `make test-perf` | bản release, offline + loopback | đo chứ không chỉ chạy các đường nóng: `core_perf` lo đóng gói/ghép gói/FEC, hạ kích thước 1080p, CRC và lô tệp, bộ phân tích VT và màn hình, mã hoá/giải mã gói tin, bộ đệm chống giật âm thanh; `platform_perf` lo QUIC thật trên loopback |

Không bài kiểm thử nào cần máy đối diện, GPU hay mạng.

**Coverage.** `make coverage` tạo báo cáo cho `core/` bằng clang + llvm-cov;
`scripts/check-coverage.sh` áp đúng ngưỡng CI đòi hỏi — **≥ 90 % dòng, ≥ 80 % nhánh**.

**Fuzzing.** `make fuzz` chạy các mục tiêu libFuzzer lên bộ phân tích gói tin, H.264, khâu
ghép gói, byte terminal, chuỗi giao diện, cùng máy trạng thái phiên của host và viewer
(clang, Linux/macOS; `FUZZ_SECONDS=N` cho mỗi mục tiêu). Mỗi mục tiêu chạy lại
`core/fuzz/regressions/<target>` trước để các cú crash đã sửa không quay lại, rồi mới fuzz
từ bộ hạt giống và từ điển đã commit. `make fuzz-coverage` cho biết corpus thực sự chạm
tới những dòng nào trong core. Mỗi cú crash tìm được đều trở thành một đầu vào hồi quy.

**Hiệu năng.** `make test-perf` dựng cả hai binary đo hiệu năng bằng preset release rồi
chạy chúng: `core_perf` đo 37 phép đo trên các đường nóng thuần C++, sau đó `platform_perf`
đo thêm 6 phép nữa trên QUIC thật qua loopback. Tổng cộng hết vài giây. Có ba thứ làm bất
kỳ cái nào trong hai cái fail, và không thứ nào là một con số mili-giây bịa ra:

- **Số lần cấp phát trên mỗi đơn vị**, đếm chính xác bằng cách thay `operator new` toàn
  cục. Một đường bắt đầu cấp phát cho từng gói hay từng khung hình sẽ fail trên mọi máy,
  trong mọi lần chạy.
- **Chi phí giãn ra sao theo đầu vào**: mỗi dòng `-scaling` chạy đúng phần việc đó với đầu
  vào gấp 4 lần và fail khi thời gian tăng nhanh hơn đầu vào rất nhiều — đúng hình dạng
  của một O(n²) lỡ tay.
- **Độ lệch so với mốc đã ghi**: `make perf-baseline` ghi `out/perf/baseline.txt` trên máy
  đang rảnh, các lần chạy sau báo mức thay đổi của từng dòng và fail khi vượt 25 %. Tệp đó
  mô tả riêng máy ấy nên không nằm trong git.

`DESKHUB_PERF_TOLERANCE`, `DESKHUB_PERF_REPEATS`, `DESKHUB_PERF_BASELINE` và
`DESKHUB_PERF_WRITE` điều chỉnh phần đo thời gian. Cả `make test` lẫn CI đều không chạy bộ
này: bản debug, ASan và coverage không nói lên điều gì về tốc độ thật.

## 6. Định dạng và phân tích tĩnh

| Lệnh | Kiểm tra |
| --- | --- |
| `make format` | áp dụng định dạng cho C++, Kotlin và Swift |
| `make lint` | vẫn những kiểm tra đó nhưng không sửa file — đúng thứ CI bắt buộc |
| `make lint-tidy` | clang-tidy trên `core/src` + `platform/src` |

Có cả bản cho từng ngôn ngữ: `format-cpp`, `lint-cpp`, `format-kotlin`, `lint-kotlin`,
`format-swift`, `lint-swift`.

Quy tắc trong nhà, nói ngắn — bản đầy đủ nằm ở `CLAUDE.md`:

- C++20, không dùng phần mở rộng của trình biên dịch. `deskhub` cho core, `deskhubp` cho
  platform.
- Hàm và kiểu viết `PascalCase`, biến cục bộ `camelCase`, thành viên private có gạch dưới
  ở cuối.
- **Không viết chú thích, ở bất cứ đâu.** Thay vào đó là tên gọi rõ nghĩa, hàm nhỏ, return
  sớm và hằng số có tên. Kiến thức không được phép mất đi thì đưa vào thông báo lỗi của
  chính nhánh sẽ hỏng khi thiếu nó, hoặc vào `ARCHITECTURE.md`.
- Mọi định danh và thông báo log đều bằng tiếng Anh; mọi tài liệu đều xuất bản bằng bốn
  thứ tiếng — Anh, Việt, Trung, Nhật — bản tiếng Anh là bản chuẩn.

## 7. Đóng gói

| Lệnh | Tạo ra |
| --- | --- |
| `make dist-macos` | file dmg ký bằng Developer ID, đã notarize và staple |
| `make verify-macos` | kiểm tra Gatekeeper trên bản vừa dựng |
| `make dist-linux` | `.deb` + `.rpm`, cả hai đều cài luật udev cho uinput |

App Windows và Linux mỗi cái chỉ là một file; không có trình cài đặt nào để dựng.

## 8. Phát hành

1. Nâng số ở [`VERSION`](../VERSION) — `scripts/check-version.sh` sẽ chặn bản deploy nếu
   tag và file này không khớp nhau.
2. Cập nhật những tài liệu bị ảnh hưởng, cả hai ngôn ngữ, trong cùng một commit.
3. Gắn tag `vX.Y.Z` rồi push. `.github/workflows/deploy.yml` sẽ build mọi nền tảng, tạo
   GitHub Release, đẩy iOS lên TestFlight, notarize bản macOS và đưa Android lên kênh
   internal của Play.

**Ghi chú phát hành được sinh ra từ tiêu đề các commit** nằm giữa tag trước và tag này, do
`scripts/changelog.sh` lo. Chạy nó ở máy để xem một tag sẽ cho ra cái gì:

```bash
scripts/changelog.sh v5.0.0     # hoặc không tham số, để lấy tag ở HEAD
```

Nghĩa là tiêu đề commit là thứ người dùng sẽ đọc, và tiền tố kiểu conventional commit
đứng trước nó quyết định nó rơi vào mục nào. Bảng ánh xạ đầy đủ, các quy tắc đè lên nó và
ví dụ cụ thể nằm ở [`.claude/skills/commit/SKILL.md`](../.claude/skills/commit/SKILL.md) —
hãy đọc trước khi viết một tiêu đề. Mục nào trống thì không in ra trong bản phát hành, còn
`INCLUDE_INTERNAL=1 scripts/changelog.sh` hiện thêm cả những commit bị bỏ qua.

## 9. CI chặn những gì

`make test` + `make lint` xanh ở máy chưa phải là toàn bộ câu chuyện. Trên mỗi pull
request:

- clang-tidy trên `core/src` + `platform/src`, SwiftLint `--strict`, Android Lint
- actionlint + shellcheck trên các workflow và `scripts/*.sh`
- cả ba bộ kiểm thử dưới ASan/UBSan và TSan, đồng thời build chéo cho Linux arm64, một máy
  ảo Android và iOS Simulator
- toàn bộ bộ integration chạy thêm ba lần nữa trên Windows, để săn một lỗi hỏng stack không
  đều trong `DrainStreams`, khoảng ba lần chạy mới lộ một lần nên một lần chạy là không đủ
- coverage của core ≥ 90 % dòng / 80 % nhánh
- các mục tiêu libFuzzer, mỗi cái 30 giây (mỗi cái 15 phút trong lần chạy đêm)
- CodeQL trên C++/Kotlin/Swift, quét gitleaks toàn bộ lịch sử, và soát xét phụ thuộc

## 10. Công cụ cho người phát triển

| Lệnh | Làm gì |
| --- | --- |
| `make icons` | dựng lại toàn bộ icon của các client từ `assets/icon_1024.png` |
| `make quic-smoke` | một cặp client + server QUIC độc lập chạy trên thư viện tĩnh quiche |
| `make opus-smoke` | một vòng mã hoá + giải mã độc lập chạy trên thư viện tĩnh opus — báo lại bitrate thật, gói lớn nhất và DTX có hoạt động không |
| `make screenshots` | macOS: chụp lại ảnh cho store trên simulator iPhone/iPad, máy ảo Android và app macOS, rồi làm mới `docs/imgs` (`ARGS="ios android macos readme"` để làm một phần) |
| `make setup-linux-permissions` | luật udev cho `/dev/uinput` + nhóm `input`, để host từ một bản build mã nguồn |
| `make reset-macos-permissions` | xoá các quyền TCC khi bản build ở máy và bản tải về tranh nhau cùng một bundle id (`ARGS="--purge"` xoá luôn các bản đã dựng) |
| `make ffmpeg-min` | Ubuntu: dựng bản FFmpeg tối giản tĩnh mà app liên kết tới (`build-linux` tự chạy) |
| `make opus` | codec âm thanh Opus cho target của máy hiện tại (`debug`, `release` và `build-linux` tự chạy) |
| `make clean` | xoá `out/` |

## 11. Khi bản build trở chứng

- **CMake dừng vì thiếu thư viện quiche** — chạy target `make quiche*` tương ứng; mỗi ABI
  cần bản của riêng nó. opus và các target `make opus*` cũng vậy.
- **`make fuzz` trên macOS không tìm thấy libFuzzer** — nó cần LLVM của Homebrew;
  `make bootstrap` cài sẵn, phần còn lại vẫn build bằng bộ công cụ Xcode.
- **`make lint` không đồng ý với editor của bạn** — công cụ được ghim mới là đúng. Chạy lại
  `make bootstrap` để lấy đúng phiên bản, rồi `make format`.
- **Các target Android không tìm thấy SDK** — đặt `ANDROID_HOME`, rồi chạy lại
  `make bootstrap`; `ANDROID_NDK_VERSION=<v>` chọn một NDK khác.
- **Quyền trên macOS giở chứng sau khi đổi qua lại giữa bản build ở máy và bản tải về** —
  `make reset-macos-permissions`.

Báo lỗi và hỏi han: [issues](https://github.com/manhpham90vn/Deskhub/issues).
