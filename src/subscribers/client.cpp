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

#include "subscribers/client.h"

namespace fastocloud {
namespace server {
namespace subscribers {

SubscriberClient::SubscriberClient(common::libev::IoLoop* server, const common::net::socket_info& info)
    : base_class(server, info), client_info_() {}

const char* SubscriberClient::ClassName() const {
  return "SubscriberClient";
}

void SubscriberClient::SetClInfo(const client_info_t& info) {
  client_info_ = info;
}

SubscriberClient::client_info_t SubscriberClient::GetClInfo() const {
  return client_info_;
}

}  // namespace subscribers
}  // namespace server
}  // namespace fastocloud
