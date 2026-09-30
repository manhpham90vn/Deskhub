#include "deskhubp/ffi/SendFfi.h"

#include "deskhub/ui/Strings.h"
#include "deskhubp/ffi/FfiText.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/client/FileTransferClient.h"
#include "deskhubp/client/FileUpload.h"
#include "deskhubp/system/UiSettingsStore.h"

#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {

std::vector<std::filesystem::path> PathList(const char* const* paths, int count) {
    std::vector<std::filesystem::path> list;
    if (!paths || count <= 0) return list;
    list.reserve(size_t(count));
    for (int i = 0; i < count; ++i)
        if (paths[i]) list.push_back(deskhubp::FfiPath(paths[i]));
    return list;
}

int32_t StateOf(deskhubp::FileTransferClientState state) {
    return int32_t(state);
}

}

struct DHSend {
    deskhubp::FileTransferClient client{};
};

int dh_send_check(const char* const* paths, int count, char* error, int errorCapacity) {
    if (error && errorCapacity > 0) error[0] = '\0';
    const deskhubp::FileBatch batch = deskhubp::InspectFiles(PathList(paths, count));
    if (batch.Ok()) return 1;
    if (error && errorCapacity > 0)
        deskhubp::CopyToBuf(error, size_t(errorCapacity), batch.error);
    return 0;
}

DHSend* dh_send_start(const char* address, const char* name, const char* const* paths,
    int count) {
    NetAddr host;
    if (!address || !ParseNetAddr(address, host)) return nullptr;

    deskhubp::FileTransferClientConfig config;
    config.host = host;
    config.hostLabel = address;
    config.clientName = name && *name ? name : deskhubp::SessionDeviceName();
    config.files = PathList(paths, count);
    if (config.files.empty()) return nullptr;

    auto handle = std::make_unique<DHSend>();
    if (!handle->client.Start(config, deskhubp::FileTransferClientCallbacks{})) return nullptr;
    return handle.release();
}

void dh_send_snapshot(DHSend* handle, DHSendProgress* out) {
    if (!out) return;
    *out = DHSendProgress{};
    if (!handle) return;

    const deskhub::TransferProgress progress = handle->client.Progress();
    out->state = StateOf(handle->client.State());
    out->finished = handle->client.Finished();
    out->fileIndex = progress.fileIndex;
    out->fileCount = progress.fileCount;
    out->bytes = progress.batchBytes;
    out->total = progress.batchSize;
    deskhubp::CopyToBuf(out->name, sizeof(out->name), progress.name);
    deskhubp::CopyToBuf(out->message, sizeof(out->message), handle->client.Message());
}

void dh_send_cancel(DHSend* handle) {
    if (handle) handle->client.Cancel();
}

void dh_send_stop(DHSend* handle) {
    if (!handle) return;
    handle->client.Stop();
    delete handle;
}
