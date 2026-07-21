#pragma once
// =============================================================================
// DiagLog.h — chuyển toàn bộ log ra file, bật bằng checkbox ở màn hình chính.
//
// NHIỆM VỤ
//   Người dùng tick "Save diagnostic log to a file" rồi bấm Share/Connect; mọi thứ
//   chương trình in ra (kể cả dòng [DIAG] của docs/09-diagnostics.md) chảy vào một
//   file cạnh exe, tên theo VAI TRÒ và thời điểm chạy.
//
// VÌ SAO KHÔNG BẢO NGƯỜI DÙNG TỰ REDIRECT
//   docs/09-diagnostics.md từng hướng dẫn `client.exe > diag-host.log 2>&1`. Cách
//   đó hỏng ở đúng ca cần log nhất — vai HOST có bật điều khiển:
//     1. UAC. Bấm Share có "allow control" thì RelaunchElevatedShare() dựng một
//        TIẾN TRÌNH MỚI qua ShellExecuteExW/runas. Tiến trình đó KHÔNG kế thừa
//        redirect của shell gốc, nên phiên host thật không ghi được chữ nào —
//        file redirect nằm lại 0 byte (đã gặp: diag-host.log 21/07/2026).
//     2. PowerShell. Toán tử `>` của PS 5.1 ghi ra UTF-16LE, làm grep/ripgrep coi
//        file như nhị phân. Tự mở file bằng freopen thì ra UTF-8 như cmd.
//   Cả hai lỗi đều thuộc loại người dùng không tự đoán ra được, nên đưa hẳn vào
//   chương trình thay vì viết thêm chú ý trong tài liệu.
//
// TÊN FILE TÁCH THEO VAI TRÒ
//   diag-agent-<ngày>-<giờ>.log   — vai host (chia sẻ)
//   diag-client-<ngày>-<giờ>.log  — vai client (xem)
//   Một phiên chẩn đoán cần CẢ HAI file (xem docs/09-diagnostics.md §1), mà hai vai
//   thường chạy trên hai máy rồi gộp lại một thư mục — trùng tên là mất bằng chứng.
//   Dấu thời gian giữ lại lần chạy trước, vì lỗi giật hay phải thử vài lần mới dính.
//
// LIÊN QUAN: ui/MainMenuWindow.cpp (checkbox + gọi Start), main.cpp (đường instance
//            admin), ElevatedShare.h (cờ --diag-log), docs/09-diagnostics.md
// =============================================================================
#include <string>

enum class DiagRole { Agent, Client };

// Đổi hướng stdout+stderr sang file trong suốt một phiên, trả lại như cũ khi hết
// phạm vi. RAII vì màn hình chính chạy nhiều phiên liên tiếp: mỗi phiên một file,
// và console phải sống lại giữa hai phiên để menu còn in được.
class DiagLogRedirect {
public:
    DiagLogRedirect() = default;
    ~DiagLogRedirect();

    DiagLogRedirect(const DiagLogRedirect&) = delete;
    DiagLogRedirect& operator=(const DiagLogRedirect&) = delete;

    // Mở file cạnh exe và bắt đầu hứng log. False = không tạo được file (thư mục
    // chỉ-đọc, ví dụ exe nằm trong Program Files) — phiên vẫn phải chạy tiếp, chỉ
    // là không có log.
    bool Start(DiagRole role);

    bool active() const { return active_; }
    const std::wstring& path() const { return path_; }

private:
    void Stop();

    std::wstring path_;
    int  savedOut_ = -1;   // fd gốc của stdout/stderr, giữ để trả lại
    int  savedErr_ = -1;
    bool active_ = false;
};
