#pragma once
// [U-511] server.cfg: every server setting in one plain file, shared by the server app (em_server), `elemental_moba --server`
// and install/run-server.sh. Lines are key=value; a missing line keeps the default.
#include <string>

#include "net/server.hpp"

namespace em::net {

inline constexpr const char* kServerCfgFile = "server.cfg";
// false when there is no file yet (the defaults stay). `language` is only for the server app's own texts.
bool load_server_cfg(ServerConfig& c, std::string* language = nullptr, const std::string& path = kServerCfgFile);
bool save_server_cfg(const ServerConfig& c, const std::string& language, const std::string& path = kServerCfgFile);

}  // namespace em::net
