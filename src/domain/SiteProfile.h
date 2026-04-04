#ifndef DOMAIN_SITEPROFILE_H
#define DOMAIN_SITEPROFILE_H

#include "domain/Protocol.h"

#include <cstdint>
#include <string>

namespace domain {

struct SiteProfile {
    std::string connectionName;
    std::string host;
    std::uint16_t port { defaultPortForProtocol(Protocol::Ftp) };
    std::string userName;
    Protocol protocol { Protocol::Ftp };
    bool passiveMode { true };
    bool allowAnonymousLogin { false };
    bool useSshTunnel { false };

    [[nodiscard]] static SiteProfile createDefault()
    {
        return SiteProfile {};
    }

    [[nodiscard]] SiteProfile withProtocol(Protocol newProtocol) const
    {
        SiteProfile updated = *this;
        updated.protocol = newProtocol;

        if (updated.port == 0 || updated.port == defaultPortForProtocol(protocol)) {
            updated.port = defaultPortForProtocol(newProtocol);
        }

        return updated;
    }

    [[nodiscard]] bool operator==(const SiteProfile &) const = default;
};

} // namespace domain

#endif // DOMAIN_SITEPROFILE_H
