#ifndef INFRA_TRANSFER_TRANSFERTYPES_H
#define INFRA_TRANSFER_TRANSFERTYPES_H

#include "domain/RemoteEntry.h"
#include "domain/SiteProfile.h"
#include "domain/Transfer.h"

#include <string>
#include <vector>

namespace infra::transfer {

using RemoteEntry = domain::RemoteEntry;
using TransferDirection = domain::TransferDirection;
using TransferJobId = domain::TransferJobId;
using TransferProgress = domain::TransferProgress;
using TransferRequest = domain::TransferRequest;
using TransferState = domain::TransferState;

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

struct ListDirectoryResult {
    OperationResult operation;
    std::vector<RemoteEntry> entries;
};

struct ConnectionRequest {
    domain::SiteProfile siteProfile;
    std::string password;
};

struct StartTransferResult {
    OperationResult operation;
    TransferJobId jobId { 0 };
};

struct TransferProgressResult {
    OperationResult operation;
    TransferProgress progress;
    bool found { false };
};

struct ConnectionResult {
    OperationResult operation;
    domain::SiteProfile siteProfile;
};

} // namespace infra::transfer

#endif // INFRA_TRANSFER_TRANSFERTYPES_H
