#ifndef INFRA_TRANSFER_TRANSFERTYPES_H
#define INFRA_TRANSFER_TRANSFERTYPES_H

#include "domain/Protocol.h"
#include "domain/SiteProfile.h"

#include <cstdint>
#include <string>
#include <vector>

namespace infra::transfer {

using TransferJobId = std::uint64_t;

enum class TransferDirection : std::uint8_t {
    Upload,
    Download,
};

enum class TransferState : std::uint8_t {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
};

struct TransferProgress {
    TransferJobId jobId { 0 };
    TransferDirection direction { TransferDirection::Upload };
    TransferState state { TransferState::Pending };
    std::uint64_t transferredBytes { 0 };
    std::uint64_t totalBytes { 0 };
};

struct TransferError {
    std::string code;
    std::string message;

    [[nodiscard]] bool hasError() const noexcept
    {
        return !code.empty() || !message.empty();
    }
};

struct OperationResult {
    bool succeeded { false };
    TransferError error;
};

struct RemoteEntry {
    std::string name;
    std::string path;
    bool isDirectory { false };
    std::uint64_t size { 0 };
};

struct ListDirectoryResult {
    OperationResult operation;
    std::vector<RemoteEntry> entries;
};

struct TransferRequest {
    TransferDirection direction { TransferDirection::Upload };
    std::string localPath;
    std::string remotePath;
    std::uint64_t expectedSize { 0 };
    domain::Protocol protocol { domain::Protocol::Ftp };
};

struct StartTransferResult {
    OperationResult operation;
    TransferJobId jobId { 0 };
};

struct ConnectionResult {
    OperationResult operation;
    domain::SiteProfile siteProfile;
};

} // namespace infra::transfer

#endif // INFRA_TRANSFER_TRANSFERTYPES_H
