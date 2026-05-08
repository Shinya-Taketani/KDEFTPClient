#ifndef DOMAIN_TRANSFER_H
#define DOMAIN_TRANSFER_H

#include "domain/Protocol.h"

#include <cstdint>
#include <string>

namespace domain {

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

struct TransferRequest {
    TransferDirection direction { TransferDirection::Upload };
    std::string localPath;
    std::string remotePath;
    std::uint64_t expectedSize { 0 };
    Protocol protocol { Protocol::Ftp };
};

struct TransferProgress {
    TransferJobId jobId { 0 };
    TransferState state { TransferState::Pending };
    std::uint64_t transferredBytes { 0 };
    std::uint64_t totalBytes { 0 };
};

struct TransferJob {
    TransferJobId id { 0 };
    TransferRequest request;
    TransferProgress progress;
};

} // namespace domain

#endif // DOMAIN_TRANSFER_H
