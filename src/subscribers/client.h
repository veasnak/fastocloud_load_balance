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

#include <fastotv/commands_info/client_info.h>
#include <fastotv/server/client.h>

#include "base/subscriber_info.h"

namespace fastocloud {
namespace server {
namespace subscribers {

class SubscriberClient : public fastotv::server::Client, public base::SubscriberInfo {
 public:
  typedef common::Optional<fastotv::commands_info::ClientInfo> client_info_t;
  typedef fastotv::server::Client base_class;

  SubscriberClient(common::libev::IoLoop* server, const common::net::socket_info& info);

  const char* ClassName() const override;

  void SetClInfo(const client_info_t& info);
  client_info_t GetClInfo() const;

 private:
  client_info_t client_info_;
};

}  // namespace subscribers
}  // namespace server
}  // namespace fastocloud
