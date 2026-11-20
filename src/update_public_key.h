#pragma once
// Public keys allowed to sign releases (see update_signature.h). Written by
// scripts/new-release-key.ps1; the matching private key stays on the maintainer's PC.
// More than one key lets a new key be added before an old one is retired.
// Empty means no key yet: official builds then refuse to compile, and updates cannot be installed.
namespace updates {
inline constexpr const char* OfficialReleaseKeys[] = {
    "RUNTMSAAAABHVmMa+jOhQf2lyG+uIwvN9qe/NV1cjLaPUS4q24cZQYGYwwSZneuwZwXglzei+4JLyaXAzpUf48BywGB4SAA5",
};

// Generated locally with scripts/new-release-key.ps1 -Channel Seelen. The matching private key
// stays outside the repository at %USERPROFILE%\.batteryhub-seelen\release-key.wbk.
inline constexpr const char* SeelenReleaseKeys[] = {
    "RUNTMSAAAAApmJ+zgr6OKn27Q5cRGAmDzPopwjBgXUYd5GSr1fwuoicRx//5NxIvOTSiW+Vz6Syxc/08NjPuQkhGfVmGCsx4",
};
}
