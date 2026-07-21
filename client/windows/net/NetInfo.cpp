// =============================================================================
// NetInfo.cpp — cài đặt bằng GetAdaptersAddresses (iphlpapi).
//
// KHUÔN MẪU GỌI API CÓ BỘ ĐỆM TỰ NỞ
//   GetAdaptersAddresses không nói trước nó cần bao nhiêu bộ nhớ. Cách dùng chuẩn
//   là gọi với một bộ đệm đoán trước; nếu thiếu, hàm trả ERROR_BUFFER_OVERFLOW VÀ
//   cập nhật `size` thành kích thước thật, ta cấp lại rồi gọi lần nữa. Vòng lặp
//   giới hạn 3 lần vì danh sách adapter có thể đổi giữa hai lần gọi (cắm/rút cáp,
//   bật/tắt VPN) — về lý thuyết có thể lặp mãi, nên phải có trần.
//
// DUYỆT DANH SÁCH LIÊN KẾT LỒNG NHAU
//   Kết quả là danh sách liên kết hai tầng: mỗi adapter (a = a->Next) có một danh
//   sách địa chỉ unicast riêng (u = u->Next). Một adapter có thể mang nhiều IPv4.
//
// LIÊN QUAN: net/NetInfo.h (vì sao phải liệt kê nhiều địa chỉ)
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "net/NetInfo.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <algorithm>
#include <cstring>

#pragma comment(lib, "iphlpapi.lib")

std::vector<AdapterAddr> ListLocalIPv4() {
    std::vector<AdapterAddr> out;

    // 16 KB đủ cho gần như mọi máy ngay lần đầu; vòng lặp chỉ là đường dự phòng.
    ULONG size = 16 * 1024;
    std::vector<uint8_t> buf;
    bool ok = false;
    for (int tries = 0; tries < 3 && !ok; ++tries) {
        buf.resize(size);
        // Bỏ ba loại thông tin ta không dùng — chúng chiếm phần lớn kích thước kết
        // quả, cắt đi thì lần gọi đầu tiên gần như chắc chắn vừa bộ đệm.
        const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                            GAA_FLAG_SKIP_DNS_SERVER;
        const ULONG r = GetAdaptersAddresses(AF_INET, flags, nullptr,
                                             (IP_ADAPTER_ADDRESSES*)buf.data(), &size);
        if (r == NO_ERROR) ok = true;
        else if (r != ERROR_BUFFER_OVERFLOW) return out; // size đã được cập nhật -> thử lại
    }
    if (!ok) return out;

    for (auto* a = (IP_ADAPTER_ADDRESSES*)buf.data(); a; a = a->Next) {
        // Adapter đang tắt/rút dây: địa chỉ của nó có thể còn đó nhưng vô dụng.
        if (a->OperStatus != IfOperStatusUp) continue;
        // 127.0.0.1 chỉ tới chính máy này — đưa cho người dùng đọc là vô nghĩa.
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
            if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) continue;
            const auto* sin = (const sockaddr_in*)u->Address.lpSockaddr;
            char ip[32];
            if (!InetNtopA(AF_INET, &sin->sin_addr, ip, sizeof(ip))) continue;
            // APIPA 169.254.x.x: Windows tự đặt khi DHCP thất bại. Nó nghĩa là card
            // này KHÔNG có mạng thật, hiện ra chỉ làm người dùng thử rồi thất bại.
            if (std::strncmp(ip, "169.254.", 8) == 0) continue;
            out.push_back(AdapterAddr{a->FriendlyName ? a->FriendlyName : L"?", ip});
        }
    }
    // Adapter ảo (vEthernet của Hyper-V/WSL...) xuống cuối: máy khác thường không
    // tới được các dải này. Vẫn giữ lại vì Hyper-V external switch là trường hợp
    // hợp lệ (IP thật nằm trên vEthernet).
    std::stable_sort(out.begin(), out.end(), [](const AdapterAddr& x, const AdapterAddr& y) {
        auto virt = [](const AdapterAddr& v) { return v.name.rfind(L"vEthernet", 0) == 0 ? 1 : 0; };
        return virt(x) < virt(y);
    });
    return out;
}
