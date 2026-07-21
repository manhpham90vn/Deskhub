#pragma once
// =============================================================================
// BmpWriter.h — công cụ DEBUG: lưu một texture ra file .bmp để nhìn bằng mắt.
//
// ⚠ KHÔNG NẰM TRONG ĐƯỜNG NÓNG STREAMING
//   Đây là dụng cụ kiểm chứng, không phải một phần của luồng video. Streaming thật
//   đưa texture thẳng từ capture sang encoder, không bao giờ đi qua đây.
//
// NHIỆM VỤ
//   Trả lời câu hỏi "cái ta bắt được có đúng là hình mong đợi không?". Khi hình ra
//   sai, đây là cách nhanh nhất để biết lỗi nằm ở khâu bắt hình hay khâu mã hoá:
//   ảnh .bmp đúng thì capture ổn, lỗi ở phía sau.
//
// VÌ SAO NÓ CHẬM — và vì sao điều đó chấp nhận được ở đây
//   Đường đi là VRAM → bộ nhớ CPU → đĩa. Bước đầu đòi tạo một staging texture rồi
//   Map nó, thao tác này ĐỒNG BỘ HOÁ CPU với GPU: CPU phải đứng chờ GPU làm xong
//   mọi việc đang dở. Trên đường nóng thì đó là thảm hoạ; ở đây gọi vài lần lúc gỡ
//   lỗi nên không sao.
//
// VÌ SAO CHỌN BMP
//   Không nén, không cần thư viện ngoài, ghi được bằng vài chục dòng. Định dạng
//   BGRA 32-bit của BMP trùng đúng với định dạng WGC giao ra, nên không phải đổi
//   màu gì cả — chép thẳng.
//
// LIÊN QUAN: capture/CaptureTypes.h (nguồn texture), capture/WindowCapture.h
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <string>

// Copy `src` (BGRA, VRAM) về CPU rồi ghi ra file BMP 32-bit tại `path`.
// Trả về false nếu copy hoặc ghi thất bại.
bool SaveTextureToBmp(ID3D11Device* device, ID3D11DeviceContext* context,
                      ID3D11Texture2D* src, const std::string& path);
