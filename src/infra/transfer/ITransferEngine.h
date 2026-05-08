#ifndef INFRA_TRANSFER_ITRANSFERENGINE_H
#define INFRA_TRANSFER_ITRANSFERENGINE_H

#include "infra/transfer/TransferTypes.h"

#include <string>

namespace infra::transfer {

class ITransferEngine
{
public:
    virtual ~ITransferEngine() = default;

    virtual ConnectionResult connect(const ConnectionRequest &request) = 0;
    virtual OperationResult disconnect() = 0;
    virtual ListDirectoryResult listDirectory(const std::string &remotePath) = 0;
    virtual StartTransferResult upload(const TransferRequest &request) = 0;
    virtual StartTransferResult download(const TransferRequest &request) = 0;
    virtual TransferProgressResult progress(TransferJobId jobId) = 0;
    virtual OperationResult cancel(TransferJobId jobId) = 0;
};

} // namespace infra::transfer

#endif // INFRA_TRANSFER_ITRANSFERENGINE_H
