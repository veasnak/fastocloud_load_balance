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

#include "process_slave_wrapper.h"

#include <thread>
#include <vector>

#include <common/daemon/commands/activate_info.h>
#include <common/daemon/commands/stop_info.h>
#include <common/license/check_expire_license.h>
#include <common/net/net.h>

#include "daemon/client.h"
#include "daemon/commands.h"
#include "daemon/server.h"

#include "http/handler.h"
#include "http/server.h"

#include "mongo/subscribers_manager.h"

#include "subscribers/handler.h"
#include "subscribers/server.h"

namespace fastocloud {
namespace server {

ProcessSlaveWrapper::ProcessSlaveWrapper(const Config& config)
    : config_(config),
      loop_(nullptr),
      subscribers_server_(nullptr),
      subscribers_handler_(nullptr),
      http_server_(nullptr),
      http_handler_(nullptr),
      ping_client_timer_(INVALID_TIMER_ID) {
  loop_ = new DaemonServer(config.host, this);
  loop_->SetName("client_server");

  mongo::SubscribersManager* sub_manager =
      new mongo::SubscribersManager(config.catchup_host, config.catchups_http_root);
  sub_manager->ConnectToDatabase(config.mongodb_url);
  sub_manager_ = sub_manager;

  subscribers_handler_ = new subscribers::SubscribersHandler(this, sub_manager_, config.epg_url);
  subscribers_server_ = new subscribers::SubscribersServer(config.subscribers_host, subscribers_handler_);
  subscribers_server_->SetName("subscribers_server");

  http_handler_ = new http::HttpHandler(sub_manager_);
  http_server_ = new http::HttpServer(config.http_host, http_handler_);
  http_server_->SetName("http_server");
}

int ProcessSlaveWrapper::SendStopDaemonRequest(const Config& config) {
  if (!config.IsValid()) {
    return EXIT_FAILURE;
  }

  const common::net::HostAndPort host = Config::GetDefaultHost();
  common::net::socket_info client_info;
  common::ErrnoError err = common::net::connect(host, common::net::ST_SOCK_STREAM, nullptr, &client_info);
  if (err) {
    return EXIT_FAILURE;
  }

  std::unique_ptr<ProtocoledDaemonClient> connection(new ProtocoledDaemonClient(nullptr, client_info));
  err = connection->StopMe(*config.license_key);
  ignore_result(connection->Close());
  if (err) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

ProcessSlaveWrapper::~ProcessSlaveWrapper() {
  (static_cast<mongo::SubscribersManager*>(sub_manager_))->Disconnect();
  destroy(&http_server_);
  destroy(&http_handler_);
  destroy(&subscribers_server_);
  destroy(&subscribers_handler_);
  destroy(&sub_manager_);
  destroy(&loop_);
}

int ProcessSlaveWrapper::Exec() {
  subscribers::SubscribersServer* subs_server = static_cast<subscribers::SubscribersServer*>(subscribers_server_);
  std::thread subs_thread = std::thread([subs_server] {
    common::ErrnoError err = subs_server->Bind(true);
    if (err) {
      DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      return;
    }

    err = subs_server->Listen(5);
    if (err) {
      DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      return;
    }

    int res = subs_server->Exec();
    UNUSED(res);
  });

  http::HttpServer* http_server = static_cast<http::HttpServer*>(http_server_);
  std::thread http_thread = std::thread([http_server] {
    common::ErrnoError err = http_server->Bind(true);
    if (err) {
      DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      return;
    }

    err = http_server->Listen(5);
    if (err) {
      DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      return;
    }

    int res = http_server->Exec();
    UNUSED(res);
  });

  int res = EXIT_FAILURE;
  DaemonServer* server = static_cast<DaemonServer*>(loop_);
  common::ErrnoError err = server->Bind(true);
  if (err) {
    DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
    goto finished;
  }

  err = server->Listen(5);
  if (err) {
    DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
    goto finished;
  }

  res = server->Exec();

finished:
  subs_thread.join();
  http_thread.join();
  return res;
}

void ProcessSlaveWrapper::PreLooped(common::libev::IoLoop* server) {
  ping_client_timer_ = server->CreateTimer(ping_timeout_clients_seconds, true);
}

void ProcessSlaveWrapper::Accepted(common::libev::IoClient* client) {
  UNUSED(client);
}

void ProcessSlaveWrapper::Moved(common::libev::IoLoop* server, common::libev::IoClient* client) {
  UNUSED(server);
  UNUSED(client);
}

void ProcessSlaveWrapper::Closed(common::libev::IoClient* client) {
  UNUSED(client);
}

void ProcessSlaveWrapper::TimerEmited(common::libev::IoLoop* server, common::libev::timer_id_t id) {
  if (ping_client_timer_ == id) {
    std::vector<common::libev::IoClient*> online_clients = server->GetClients();
    for (size_t i = 0; i < online_clients.size(); ++i) {
      common::libev::IoClient* client = online_clients[i];
      ProtocoledDaemonClient* dclient = dynamic_cast<ProtocoledDaemonClient*>(client);
      if (dclient && dclient->IsVerified()) {
        common::ErrnoError err = dclient->Ping();
        if (err) {
          DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
          ignore_result(dclient->Close());
          delete dclient;
        } else {
          INFO_LOG() << "Sent ping to client[" << client->GetFormatedName() << "], from server["
                     << server->GetFormatedName() << "], " << online_clients.size() << " client(s) connected.";
        }
      }
    }
  }
}

void ProcessSlaveWrapper::Accepted(common::libev::IoChild* child) {
  UNUSED(child);
}

void ProcessSlaveWrapper::Moved(common::libev::IoLoop* server, common::libev::IoChild* child) {
  UNUSED(server);
  UNUSED(child);
}

void ProcessSlaveWrapper::ChildStatusChanged(common::libev::IoChild* child, int status, int signal) {
  UNUSED(child);
  UNUSED(status);
  UNUSED(signal);
}

void ProcessSlaveWrapper::BroadcastClients(const fastotv::protocol::request_t& req) {
  std::vector<common::libev::IoClient*> clients = loop_->GetClients();
  for (size_t i = 0; i < clients.size(); ++i) {
    ProtocoledDaemonClient* dclient = dynamic_cast<ProtocoledDaemonClient*>(clients[i]);
    if (dclient && dclient->IsVerified()) {
      common::ErrnoError err = dclient->WriteRequest(req);
      if (err) {
        WARNING_LOG() << "BroadcastClients error: " << err->GetDescription();
      }
    }
  }
}

common::ErrnoError ProcessSlaveWrapper::DaemonDataReceived(ProtocoledDaemonClient* dclient) {
  CHECK(loop_->IsLoopThread());
  std::string input_command;
  common::ErrnoError err = dclient->ReadCommand(&input_command);
  if (err) {
    return err;  // i don't want handle spam, comand must be foramated according protocol
  }

  fastotv::protocol::request_t* req = nullptr;
  fastotv::protocol::response_t* resp = nullptr;
  common::Error err_parse = common::protocols::json_rpc::ParseJsonRPC(input_command, &req, &resp);
  if (err_parse) {
    const std::string err_str = err_parse->GetDescription();
    return common::make_errno_error(err_str, EAGAIN);
  }

  if (req) {
    DEBUG_LOG() << "Received daemon request: " << input_command;
    err = HandleRequestServiceCommand(dclient, req);
    if (err) {
      DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
    }
    delete req;
  } else if (resp) {
    DEBUG_LOG() << "Received daemon responce: " << input_command;
    err = HandleResponceServiceCommand(dclient, resp);
    if (err) {
      DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
    }
    delete resp;
  } else {
    DNOTREACHED();
    return common::make_errno_error("Invalid command type.", EINVAL);
  }

  return common::ErrnoError();
}

void ProcessSlaveWrapper::DataReceived(common::libev::IoClient* client) {
  if (ProtocoledDaemonClient* dclient = dynamic_cast<ProtocoledDaemonClient*>(client)) {
    common::ErrnoError err = DaemonDataReceived(dclient);
    if (err) {
      DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      ignore_result(dclient->Close());
      delete dclient;
    }
  } else {
    NOTREACHED();
  }
}

void ProcessSlaveWrapper::DataReadyToWrite(common::libev::IoClient* client) {
  UNUSED(client);
}

void ProcessSlaveWrapper::PostLooped(common::libev::IoLoop* server) {
  if (ping_client_timer_ != INVALID_TIMER_ID) {
    server->RemoveTimer(ping_client_timer_);
    ping_client_timer_ = INVALID_TIMER_ID;
  }
}

common::ErrnoError ProcessSlaveWrapper::HandleRequestClientStopService(ProtocoledDaemonClient* dclient,
                                                                       const fastotv::protocol::request_t* req) {
  CHECK(loop_->IsLoopThread());
  if (req->params) {
    const char* params_ptr = req->params->c_str();
    json_object* jstop = json_tokener_parse(params_ptr);
    if (!jstop) {
      return common::make_errno_error_inval();
    }

    common::daemon::commands::StopInfo stop_info;
    common::Error err_des = stop_info.DeSerialize(jstop);
    json_object_put(jstop);
    if (err_des) {
      const std::string err_str = err_des->GetDescription();
      return common::make_errno_error(err_str, EAGAIN);
    }

    bool is_verified_request = stop_info.GetLicense() == config_.license_key || dclient->IsVerified();
    if (!is_verified_request) {
      return common::make_errno_error_inval();
    }

    common::ErrnoError err = dclient->StopSuccess(req->id);
    subscribers_server_->Stop();
    http_server_->Stop();
    loop_->Stop();
    return err;
  }

  return common::make_errno_error_inval();
}

common::ErrnoError ProcessSlaveWrapper::HandleRequestClientActivate(ProtocoledDaemonClient* dclient,
                                                                    const fastotv::protocol::request_t* req) {
  CHECK(loop_->IsLoopThread());
  if (req->params) {
    const char* params_ptr = req->params->c_str();
    json_object* jactivate = json_tokener_parse(params_ptr);
    if (!jactivate) {
      return common::make_errno_error_inval();
    }

    common::daemon::commands::ActivateInfo activate_info;
    common::Error err_des = activate_info.DeSerialize(jactivate);
    json_object_put(jactivate);
    if (err_des) {
      ignore_result(dclient->ActivateFail(req->id, err_des));
      return common::make_errno_error(err_des->GetDescription(), EAGAIN);
    }

    const auto expire_key = activate_info.GetLicense();
    common::time64_t tm;
    common::Error err_exp =
        common::license::GetExpireTimeFromKey(PROJECT_NAME_LOWERCASE, *config_.license_key, *expire_key, &tm);
    if (err_exp) {
      ignore_result(dclient->ActivateFail(req->id, err_exp));
      return common::make_errno_error(err_exp->GetDescription(), EINVAL);
    }

    common::ErrnoError err_ser = dclient->ActivateSuccess(req->id);
    if (err_ser) {
      return err_ser;
    }

    dclient->SetVerified(true, tm);
    return common::ErrnoError();
  }

  return common::make_errno_error_inval();
}

common::ErrnoError ProcessSlaveWrapper::HandleResponcePingService(ProtocoledDaemonClient* dclient,
                                                                  const fastotv::protocol::response_t* resp) {
  UNUSED(dclient);
  CHECK(loop_->IsLoopThread());
  if (resp->IsMessage()) {
    const char* params_ptr = resp->message->result.c_str();
    json_object* jclient_ping = json_tokener_parse(params_ptr);
    if (!jclient_ping) {
      return common::make_errno_error_inval();
    }

    common::daemon::commands::ClientPingInfo client_ping_info;
    common::Error err_des = client_ping_info.DeSerialize(jclient_ping);
    json_object_put(jclient_ping);
    if (err_des) {
      const std::string err_str = err_des->GetDescription();
      return common::make_errno_error(err_str, EAGAIN);
    }
    return common::ErrnoError();
  }
  return common::ErrnoError();
}

common::ErrnoError ProcessSlaveWrapper::HandleResponceCatchupCreatedService(ProtocoledDaemonClient* dclient,
                                                                            const fastotv::protocol::response_t* resp) {
  UNUSED(dclient);
  CHECK(loop_->IsLoopThread());
  if (resp->IsMessage()) {
    const char* params_ptr = resp->message->result.c_str();
    json_object* jcatchup = json_tokener_parse(params_ptr);
    if (!jcatchup) {
      return common::make_errno_error_inval();
    }

    return common::ErrnoError();
  }
  return common::ErrnoError();
}

common::ErrnoError ProcessSlaveWrapper::HandleRequestClientPingService(ProtocoledDaemonClient* dclient,
                                                                       const fastotv::protocol::request_t* req) {
  CHECK(loop_->IsLoopThread());
  if (!dclient->IsVerified()) {
    return common::make_errno_error_inval();
  }

  if (req->params) {
    const char* params_ptr = req->params->c_str();
    json_object* jstop = json_tokener_parse(params_ptr);
    if (!jstop) {
      return common::make_errno_error_inval();
    }

    common::daemon::commands::ClientPingInfo client_ping_info;
    common::Error err_des = client_ping_info.DeSerialize(jstop);
    json_object_put(jstop);
    if (err_des) {
      const std::string err_str = err_des->GetDescription();
      return common::make_errno_error(err_str, EAGAIN);
    }

    return dclient->Pong(req->id);
  }

  return common::make_errno_error_inval();
}

common::ErrnoError ProcessSlaveWrapper::HandleRequestServiceCommand(ProtocoledDaemonClient* dclient,
                                                                    const fastotv::protocol::request_t* req) {
  if (req->method == DAEMON_STOP_SERVICE) {
    return HandleRequestClientStopService(dclient, req);
  } else if (req->method == DAEMON_PING_SERVICE) {
    return HandleRequestClientPingService(dclient, req);
  }

  WARNING_LOG() << "Received unknown method: " << req->method;
  return common::ErrnoError();
}

common::ErrnoError ProcessSlaveWrapper::HandleResponceServiceCommand(ProtocoledDaemonClient* dclient,
                                                                     const fastotv::protocol::response_t* resp) {
  CHECK(loop_->IsLoopThread());
  if (!dclient->IsVerified()) {
    return common::make_errno_error_inval();
  }

  fastotv::protocol::request_t req;
  if (dclient->PopRequestByID(resp->id, &req)) {
    if (req.method == DAEMON_SERVER_PING) {
      ignore_result(HandleResponcePingService(dclient, resp));
    } else if (req.method == DAEMON_SERVER_CATCHUP_CREATED) {
      ignore_result(HandleResponceCatchupCreatedService(dclient, resp));
    } else {
      WARNING_LOG() << "HandleResponceServiceCommand not handled command: " << req.method;
    }
  }

  return common::ErrnoError();
}

void ProcessSlaveWrapper::CatchupCreated(subscribers::SubscribersHandler* handler,
                                         const fastotv::commands_info::CatchupInfo& chan) {
  UNUSED(handler);
  fastotv::protocol::request_t req;
  common::Error err_ser = CatchupCreatedBroadcast(chan, &req);
  if (err_ser) {
    return;
  }

  loop_->ExecInLoopThread([this, req]() { BroadcastClients(req); });
}

}  // namespace server
}  // namespace fastocloud
