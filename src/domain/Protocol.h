#ifndef DOMAIN_PROTOCOL_H
#define DOMAIN_PROTOCOL_H

#include <cstdint>

namespace domain {

enum class Protocol : std::uint8_t {
    Ftp,
    Ftps,
    Sftp,
};

[[nodiscard]] constexpr std::uint16_t defaultPortForProtocol(Protocol protocol) noexcept
{
    switch (protocol) {
    case Protocol::Ftp:
        return 21;
    case Protocol::Ftps:
        return 990;
    case Protocol::Sftp:
        return 22;
    }

    return 0;
}

} // namespace domain

#endif // DOMAIN_PROTOCOL_H
