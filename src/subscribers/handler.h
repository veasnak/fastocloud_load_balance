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

#include <common/uri/url.h>

#include <fastotv/protocol/types.h>

#include "base/iserver_handler.h"

namespace fastocloud {
namespace server {
namespace base {
class ISubscribersManager;
}
namespace subscribers {

class SubscriberClient;
class ISubscribersHandlerObserver;

class SubscribersHandler : public base::IServerHandler {
 public:
  typedef base::IServerHandler base_class;
  enum {
    ping_timeout_clients = 60  // sec
  };

  explicit SubscribersHandler(ISubscribersHandlerObserver* observer,
                              base::ISubscribersManager* manager,
                              const common::uri::Url& epg_url);

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
  common::ErrnoError HandleInnerDataReceived(SubscriberClient* client, const std::string& input_command);
  common::ErrnoError HandleRequestCommand(SubscriberClient* client, fastotv::protocol::request_t* req);
  common::ErrnoError HandleResponceCommand(SubscriberClient* client, fastotv::protocol::response_t* resp);

  common::ErrnoError HandleRequestClientActivate(SubscriberClient* client, fastotv::protocol::request_t* req);
  common::ErrnoError HandleRequestClientLogin(SubscriberClient* client, fastotv::protocol::request_t* req);
  common::ErrnoError HandleRequestClientPing(SubscriberClient* client, fastotv::protocol::request_t* req);
  common::ErrnoError HandleRequestClientGetServerInfo(SubscriberClient* client, fastotv::protocol::request_t* req);
  common::ErrnoError HandleRequestClientGetChannels(SubscriberClient* client, fastotv::protocol::request_t* req);
  common::ErrnoError HandleRequestClientGetRuntimeChannelInfo(SubscriberClient* client,
                                                              fastotv::protocol::request_t* req);
  common::ErrnoError HandleRequestClientSetFavorite(SubscriberClient* client, fastotv::protocol::request_t* req);
  common::ErrnoError HandleRequestClientSetRecent(SubscriberClient* client, fastotv::protocol::request_t* req);
  common::ErrnoError HandleRequestInterruptStreamTime(SubscriberClient* client, fastotv::protocol::request_t* req);
  common::ErrnoError HandleRequestGenerateCatchup(SubscriberClient* client, fastotv::protocol::request_t* req);
  common::ErrnoError HandleRequestUndoCatchup(SubscriberClient* client, fastotv::protocol::request_t* req);

  common::ErrnoError HandleResponceServerPing(SubscriberClient* client, fastotv::protocol::response_t* resp);
  common::ErrnoError HandleResponceServerGetClientInfo(SubscriberClient* client, fastotv::protocol::response_t* resp);

 private:
  const common::uri::Url epg_url_;

  common::libev::timer_id_t ping_client_id_timer_;
  base::ISubscribersManager* const manager_;
  ISubscribersHandlerObserver* const observer_;
};

}  // namespace subscribers
}  // namespace server
}  // namespace fastocloud
