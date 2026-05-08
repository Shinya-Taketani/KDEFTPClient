#ifndef DOMAIN_REMOTEENTRY_H
#define DOMAIN_REMOTEENTRY_H

#include <cstdint>
#include <string>

namespace domain {

struct RemoteEntry {
    std::string name;
    std::string path;
    bool isDirectory { false };
    std::uint64_t size { 0 };
};

} // namespace domain

#endif // DOMAIN_REMOTEENTRY_H
