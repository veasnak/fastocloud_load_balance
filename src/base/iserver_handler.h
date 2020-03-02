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

#include <common/libev/io_loop_observer.h>

namespace fastocloud {
namespace server {
namespace base {

class IServerHandler : public common::libev::IoLoopObserver {
 public:
  typedef std::atomic<size_t> online_clients_t;
  IServerHandler();

  size_t GetOnlineClients() const;

  void Accepted(common::libev::IoClient* client) override;
  void Closed(common::libev::IoClient* client) override;
  void Moved(common::libev::IoLoop* server, common::libev::IoClient* client) override;

 private:
  online_clients_t online_clients_;
};

}  // namespace base
}  // namespace server
}  // namespace fastocloud
