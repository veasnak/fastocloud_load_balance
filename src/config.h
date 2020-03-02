/*  Copyright (C) 2014-2020 FastoGT. All right reserved.
    This file is part of fastocloud.
    fastocloud is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    fastocloud is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with fastocloud.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <string>

#include <common/error.h>
#include <common/file_system/path.h>
#include <common/net/types.h>
#include <common/uri/url.h>

namespace fastocloud {
namespace server {

struct Config {
  Config();

  static common::net::HostAndPort GetDefaultHost();
  static common::net::HostAndPort GetCatchupDefaultHost();

  bool IsValid() const;

  common::net::HostAndPort host;
  common::net::HostAndPort subscribers_host;
  std::string log_path;
  common::logging::LOG_LEVEL log_level;
  common::net::HostAndPort http_host;
  std::string mongodb_url;
  common::uri::Url epg_url;
  common::net::HostAndPort catchup_host;
  common::file_system::ascii_directory_string_path catchups_http_root;
  std::string license_key;
};

common::ErrnoError load_config_from_file(const std::string& config_absolute_path, Config* config) WARN_UNUSED_RESULT;

}  // namespace server
}  // namespace fastocloud
