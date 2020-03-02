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

#include "base/iserver_handler.h"

namespace fastocloud {
namespace server {
namespace base {
class ISubscribersManager;
}
namespace http {

class HttpClient;

class HttpHandler : public base::IServerHandler {
 public:
  enum { BUF_SIZE = 4096 };
  typedef base::IServerHandler base_class;
  explicit HttpHandler(base::ISubscribersManager* manager);

  void PreLooped(common::libev::IoLoop* server) override;

  void Accepted(common::libev::IoClient* client) override;
  void Moved(common::libev::IoLoop* server, common::libev::IoClient* client) override;
  void Closed(common::libev::IoClient* client) override;
  void TimerEmited(common::libev::IoLoop* server, common::libev::timer_id_t id) override;
  void Accepted(common::libev::IoChild* child) override;
  void Moved(common::libev::IoLoop* server, common::libev::IoChild* child) override;
  void ChildStatusChanged(common::libev::IoChild* child, int status, int signal) override;

  void DataReceived(common::libev::IoClient* client) override;
  void DataReadyToWrite(common::libev::IoClient* client) override;

  void PostLooped(common::libev::IoLoop* server) override;

 private:
  void ProcessReceived(HttpClient* hclient, const char* request, size_t req_len);

  base::ISubscribersManager* const manager_;
};

}  // namespace http
}  // namespace server
}  // namespace fastocloud
