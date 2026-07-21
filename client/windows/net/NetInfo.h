#pragma once
// =============================================================================
// NetInfo.h — liệt kê địa chỉ IPv4 của máy theo từng card mạng.
//
// NHIỆM VỤ
//   Trả lời câu hỏi "máy này có địa chỉ gì để máy kia gọi tới?". Dùng ở màn hình
//   chính kiểu AnyDesk: hiện địa chỉ để người dùng đọc cho máy bên kia, và để agent
//   in ra khi bắt đầu lắng nghe.
//
// VÌ SAO PHẢI LIỆT KÊ THEO ADAPTER, KHÔNG PHẢI MỘT ĐỊA CHỈ DUY NHẤT
//   Máy Windows hiện đại hầu như luôn có nhiều IPv4 cùng lúc: Ethernet, Wi-Fi, rồi
//   một loạt vEthernet ảo do Hyper-V, WSL hoặc Docker tạo ra. Không có cách nào
//   chắc chắn đoán được cái nào là "đúng" — nó phụ thuộc vào máy kia nằm ở đâu.
//   Nên ta hiện HẾT kèm tên adapter và để người dùng chọn; họ biết mình đang cắm
//   dây hay dùng Wi-Fi, còn chương trình thì không.
//
// LỌC SẴN NHỮNG THỨ CHẮC CHẮN VÔ DỤNG
//   Chỉ trả adapter đang Up, bỏ loopback và bỏ địa chỉ APIPA 169.254.x.x — xem
//   NetInfo.cpp về lý do từng mục.
//
// LIÊN QUAN: ui/MainMenuWindow.cpp (hiển thị cho người dùng), AgentLoop.cpp
// =============================================================================
#include <string>
#include <vector>

struct AdapterAddr {
    std::wstring name; // tên thân thiện của adapter ("Ethernet", "Wi-Fi"...)
    std::string  ip;   // "192.168.1.10"
};

// Chỉ trả về adapter đang Up, bỏ loopback và địa chỉ APIPA 169.254.x.x.
std::vector<AdapterAddr> ListLocalIPv4();
