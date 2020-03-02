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

#include "subscribers/handler.h"

#include <string>
#include <vector>

#include <common/libev/io_loop.h>

#include <fastotv/commands/commands.h>
#include <fastotv/commands_info/catchup_generate_info.h>
#include <fastotv/commands_info/catchup_undo_info.h>
#include <fastotv/commands_info/favorite_info.h>
#include <fastotv/commands_info/recent_stream_time_info.h>

#include "base/isubscribers_manager.h"

#include "subscribers/client.h"
#include "subscribers/handler_observer.h"

namespace fastocloud {
namespace server {
namespace subscribers {

SubscribersHandler::SubscribersHandler(ISubscribersHandlerObserver* observer,
                                       base::ISubscribersManager* manager,
                                       const common::uri::Url& epg_url)
    : base_class(),
      epg_url_(epg_url),
      ping_client_id_timer_(INVALID_TIMER_ID),
      manager_(manager),
      observer_(observer) {}

void SubscribersHandler::PreLooped(common::libev::IoLoop* server) {
  UNUSED(server);
  ping_client_id_timer_ = server->CreateTimer(ping_timeout_clients, true);
}

void SubscribersHandler::Accepted(common::libev::IoClient* client) {
  base_class::Accepted(client);
}

void SubscribersHandler::Moved(common::libev::IoLoop* server, common::libev::IoClient* client) {
  base_class::Moved(server, client);
}

void SubscribersHandler::Closed(common::libev::IoClient* client) {
  SubscriberClient* iclient = static_cast<SubscriberClient*>(client);
  const auto server_user_auth = iclient->GetLogin();
  common::Error unreg_err = manager_->UnRegisterInnerConnectionByHost(iclient);
  if (unreg_err) {
    base_class::Closed(client);
    return;
  }

  if (!server_user_auth) {
    base_class::Closed(client);
    return;
  }

  INFO_LOG() << "Bye registered user: " << server_user_auth->GetLogin();
  base_class::Closed(client);
}

void SubscribersHandler::TimerEmited(common::libev::IoLoop* server, common::libev::timer_id_t id) {
  if (ping_client_id_timer_ == id) {
    std::vector<common::libev::IoClient*> online_clients = server->GetClients();
    for (size_t i = 0; i < online_clients.size(); ++i) {
      common::libev::IoClient* client = online_clients[i];
      SubscriberClient* iclient = static_cast<SubscriberClient*>(client);
      if (iclient) {
        common::ErrnoError err = iclient->Ping();
        if (err) {
          DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
          ignore_result(client->Close());
          delete client;
        } else {
          const auto user_auth = iclient->GetLogin();
          if (user_auth) {
            const auto client_info = iclient->GetClInfo();
            if (!client_info) {
              ignore_result(iclient->GetClientInfo());
            }
          }
          INFO_LOG() << "Sent ping to client[" << client->GetFormatedName() << "], from server["
                     << server->GetFormatedName() << "], " << online_clients.size() << " client(s) connected.";
        }
      }
    }
  }
}

void SubscribersHandler::Accepted(common::libev::IoChild* child) {
  UNUSED(child);
}

void SubscribersHandler::Moved(common::libev::IoLoop* server, common::libev::IoChild* child) {
  UNUSED(server);
  UNUSED(child);
}

void SubscribersHandler::ChildStatusChanged(common::libev::IoChild* child, int status, int signal) {
  UNUSED(child);
  UNUSED(status);
  UNUSED(signal);
}

void SubscribersHandler::DataReceived(common::libev::IoClient* client) {
  std::string buff;
  SubscriberClient* iclient = static_cast<SubscriberClient*>(client);
  common::ErrnoError err = iclient->ReadCommand(&buff);
  if (err) {
    DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
    ignore_result(client->Close());
    delete client;
    return;
  }

  HandleInnerDataReceived(iclient, buff);
}

void SubscribersHandler::DataReadyToWrite(common::libev::IoClient* client) {
  UNUSED(client);
}

void SubscribersHandler::PostLooped(common::libev::IoLoop* server) {
  if (ping_client_id_timer_ != INVALID_TIMER_ID) {
    server->RemoveTimer(ping_client_id_timer_);
    ping_client_id_timer_ = INVALID_TIMER_ID;
  }
}

common::ErrnoError SubscribersHandler::HandleInnerDataReceived(SubscriberClient* client,
                                                               const std::string& input_command) {
  fastotv::protocol::request_t* req = nullptr;
  fastotv::protocol::response_t* resp = nullptr;
  common::Error err_parse = common::protocols::json_rpc::ParseJsonRPC(input_command, &req, &resp);
  if (err_parse) {
    const std::string err_str = err_parse->GetDescription();
    return common::make_errno_error(err_str, EAGAIN);
  }

  if (req) {
    INFO_LOG() << "Received request: " << input_command;
    common::ErrnoError err = HandleRequestCommand(client, req);
    if (err) {
      DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
    }
    delete req;
  } else if (resp) {
    INFO_LOG() << "Received responce: " << input_command;
    common::ErrnoError err = HandleResponceCommand(client, resp);
    if (err) {
      DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
    }
    delete resp;
  } else {
    DNOTREACHED();
    return common::make_errno_error("Invalid command type", EINVAL);
  }

  return common::ErrnoError();
}

common::ErrnoError SubscribersHandler::HandleRequestCommand(SubscriberClient* client,
                                                            fastotv::protocol::request_t* req) {
  SubscriberClient* iclient = static_cast<SubscriberClient*>(client);
  if (req->method == CLIENT_ACTIVATE_DEVICE) {
    return HandleRequestClientActivate(iclient, req);
  } else if (req->method == CLIENT_LOGIN) {
    return HandleRequestClientLogin(iclient, req);
  } else if (req->method == CLIENT_PING) {
    return HandleRequestClientPing(iclient, req);
  } else if (req->method == CLIENT_GET_SERVER_INFO) {
    return HandleRequestClientGetServerInfo(iclient, req);
  } else if (req->method == CLIENT_GET_CHANNELS) {
    return HandleRequestClientGetChannels(iclient, req);
  } else if (req->method == CLIENT_GET_RUNTIME_CHANNEL_INFO) {
    return HandleRequestClientGetRuntimeChannelInfo(iclient, req);
  } else if (req->method == CLIENT_SET_FAVORITE) {
    return HandleRequestClientSetFavorite(iclient, req);
  } else if (req->method == CLIENT_SET_RECENT) {
    return HandleRequestClientSetRecent(iclient, req);
  } else if (req->method == CLIENT_INTERRUPT_STREAM_TIME) {
    return HandleRequestInterruptStreamTime(iclient, req);
  } else if (req->method == CLIENT_REQUEST_CATCHUP) {
    return HandleRequestGenerateCatchup(iclient, req);
  } else if (req->method == CLIENT_REQUEST_UNDO_CATCHUP) {
    return HandleRequestUndoCatchup(iclient, req);
  }

  WARNING_LOG() << "Received unknown command: " << req->method;
  return common::ErrnoError();
}

common::ErrnoError SubscribersHandler::HandleResponceCommand(SubscriberClient* client,
                                                             fastotv::protocol::response_t* resp) {
  fastotv::protocol::request_t req;
  SubscriberClient* sclient = static_cast<SubscriberClient*>(client);
  SubscriberClient::callback_t cb;
  if (sclient->PopRequestByID(resp->id, &req, &cb)) {
    if (cb) {
      cb(resp);
    }
    if (req.method == SERVER_PING) {
      return HandleResponceServerPing(sclient, resp);
    } else if (req.method == SERVER_GET_CLIENT_INFO) {
      return HandleResponceServerGetClientInfo(sclient, resp);
    } else {
      WARNING_LOG() << "HandleResponceServiceCommand not handled command: " << req.method;
    }
  }

  return common::ErrnoError();
}

common::ErrnoError SubscribersHandler::HandleRequestClientActivate(SubscriberClient* client,
                                                                   fastotv::protocol::request_t* req) {
  if (req->params) {
    const char* params_ptr = req->params->c_str();
    json_object* jauth = json_tokener_parse(params_ptr);
    if (!jauth) {
      return common::make_errno_error_inval();
    }

    fastotv::commands_info::LoginInfo uauth;
    common::Error err = uauth.DeSerialize(jauth);
    json_object_put(jauth);
    if (err) {
      client->ActivateDeviceFail(req->id, err);
      return common::make_errno_error(err->GetDescription(), EINVAL);
    }

    fastotv::commands_info::DevicesInfo devices;
    err = manager_->ClientActivate(uauth, &devices);
    if (err) {
      client->ActivateDeviceFail(req->id, err);
      return common::make_errno_error(err->GetDescription(), EINVAL);
    }

    client->ActivateDeviceSuccess(req->id, devices);
    INFO_LOG() << "Active registered user: " << uauth.GetLogin();
    return common::ErrnoError();
  }

  return common::make_errno_error_inval();
}

common::ErrnoError SubscribersHandler::HandleRequestClientLogin(SubscriberClient* client,
                                                                fastotv::protocol::request_t* req) {
  if (req->params) {
    const char* params_ptr = req->params->c_str();
    json_object* jauth = json_tokener_parse(params_ptr);
    if (!jauth) {
      return common::make_errno_error_inval();
    }

    fastotv::commands_info::AuthInfo uauth;
    common::Error err = uauth.DeSerialize(jauth);
    json_object_put(jauth);
    if (err) {
      client->LoginFail(req->id, err);
      return common::make_errno_error(err->GetDescription(), EINVAL);
    }

    base::ServerDBAuthInfo ser;
    err = manager_->ClientLogin(uauth, &ser);
    if (err) {
      client->LoginFail(req->id, err);
      return common::make_errno_error(err->GetDescription(), EINVAL);
    }

    common::ErrnoError errn = client->LoginSuccess(req->id, ser);
    if (errn) {
      return errn;
    }

    err = manager_->RegisterInnerConnectionByHost(client, ser);
    DCHECK(!err) << "Register inner connection error: " << err->GetDescription();
    INFO_LOG() << "Welcome registered user: " << ser.GetLogin();
    return common::ErrnoError();
  }

  return common::make_errno_error_inval();
}

common::ErrnoError SubscribersHandler::HandleRequestClientPing(SubscriberClient* client,
                                                               fastotv::protocol::request_t* req) {
  base::ServerDBAuthInfo user;
  common::Error err = manager_->CheckIsLoginClient(client, &user);
  if (err) {
    ignore_result(client->CheckLoginFail(req->id, err));
    ignore_result(client->Close());
    delete client;
    return common::make_errno_error(err->GetDescription(), EINVAL);
  }

  if (req->params) {
    const char* params_ptr = req->params->c_str();
    json_object* jstop = json_tokener_parse(params_ptr);
    if (!jstop) {
      return common::make_errno_error_inval();
    }

    common::daemon::commands::ClientPingInfo ping_info;
    common::Error err_des = ping_info.DeSerialize(jstop);
    json_object_put(jstop);
    if (err_des) {
      const std::string err_str = err_des->GetDescription();
      return common::make_errno_error(err_str, EAGAIN);
    }

    return client->Pong(req->id);
  }

  return common::make_errno_error_inval();
}

common::ErrnoError SubscribersHandler::HandleRequestClientGetServerInfo(SubscriberClient* client,
                                                                        fastotv::protocol::request_t* req) {
  base::ServerDBAuthInfo user;
  common::Error err = manager_->CheckIsLoginClient(client, &user);
  if (err) {
    ignore_result(client->CheckLoginFail(req->id, err));
    ignore_result(client->Close());
    delete client;
    return common::make_errno_error(err->GetDescription(), EINVAL);
  }

  fastotv::commands_info::ServerInfo serv(epg_url_);
  return client->GetServerInfoSuccess(req->id, serv);
}

common::ErrnoError SubscribersHandler::HandleRequestClientGetChannels(SubscriberClient* client,
                                                                      fastotv::protocol::request_t* req) {
  base::ServerDBAuthInfo auth;
  common::Error err = manager_->CheckIsLoginClient(client, &auth);
  if (err) {
    ignore_result(client->CheckLoginFail(req->id, err));
    ignore_result(client->Close());
    delete client;
    return common::make_errno_error(err->GetDescription(), EINVAL);
  }

  fastotv::commands_info::ChannelsInfo chans;
  fastotv::commands_info::VodsInfo vods;
  fastotv::commands_info::ChannelsInfo pchans;
  fastotv::commands_info::VodsInfo pvods;
  fastotv::commands_info::CatchupsInfo catchups;
  err = manager_->ClientGetChannels(auth, &chans, &vods, &pchans, &pvods, &catchups);
  if (err) {
    client->GetChannelsFail(req->id, err);
    return common::make_errno_error(err->GetDescription(), EINVAL);
  }

  return client->GetChannelsSuccess(req->id, chans, vods, pchans, pvods, catchups);
}

common::ErrnoError SubscribersHandler::HandleRequestClientGetRuntimeChannelInfo(SubscriberClient* client,
                                                                                fastotv::protocol::request_t* req) {
  base::ServerDBAuthInfo auth;
  common::Error err = manager_->CheckIsLoginClient(client, &auth);
  if (err) {
    ignore_result(client->CheckLoginFail(req->id, err));
    ignore_result(client->Close());
    delete client;
    return common::make_errno_error(err->GetDescription(), EINVAL);
  }

  if (req->params) {
    const char* params_ptr = req->params->c_str();
    json_object* jrun = json_tokener_parse(params_ptr);
    if (!jrun) {
      return common::make_errno_error_inval();
    }

    fastotv::commands_info::RuntimeChannelLiteInfo run;
    common::Error err_des = run.DeSerialize(jrun);
    json_object_put(jrun);
    if (err_des) {
      const std::string err_str = err_des->GetDescription();
      return common::make_errno_error(err_str, EAGAIN);
    }

    const fastotv::stream_id_t sid = run.GetStreamID();
    size_t watchers = manager_->GetAndUpdateOnlineUserByStreamID(sid);  // calc watchers
    client->SetCurrentStreamID(sid);                                    // add to watcher

    return client->GetRuntimeChannelInfoSuccess(req->id, sid, watchers);
  }

  return common::make_errno_error_inval();
}

common::ErrnoError SubscribersHandler::HandleRequestClientSetFavorite(SubscriberClient* client,
                                                                      fastotv::protocol::request_t* req) {
  base::ServerDBAuthInfo auth;
  common::Error err = manager_->CheckIsLoginClient(client, &auth);
  if (err) {
    ignore_result(client->CheckLoginFail(req->id, err));
    ignore_result(client->Close());
    delete client;
    return common::make_errno_error(err->GetDescription(), EINVAL);
  }

  if (req->params) {
    const char* params_ptr = req->params->c_str();
    json_object* jfav = json_tokener_parse(params_ptr);
    if (!jfav) {
      return common::make_errno_error_inval();
    }

    fastotv::commands_info::FavoriteInfo fav;
    common::Error err_des = fav.DeSerialize(jfav);
    json_object_put(jfav);
    if (err_des) {
      const std::string err_str = err_des->GetDescription();
      return common::make_errno_error(err_str, EAGAIN);
    }

    // write to DB
    auto login = client->GetLogin();
    manager_->SetFavorite(*login, fav);
    return client->GetFavoriteInfoSuccess(req->id);
  }

  return common::make_errno_error_inval();
}

common::ErrnoError SubscribersHandler::HandleRequestClientSetRecent(SubscriberClient* client,
                                                                    fastotv::protocol::request_t* req) {
  base::ServerDBAuthInfo auth;
  common::Error err = manager_->CheckIsLoginClient(client, &auth);
  if (err) {
    ignore_result(client->CheckLoginFail(req->id, err));
    ignore_result(client->Close());
    delete client;
    return common::make_errno_error(err->GetDescription(), EINVAL);
  }

  if (req->params) {
    const char* params_ptr = req->params->c_str();
    json_object* jfav = json_tokener_parse(params_ptr);
    if (!jfav) {
      return common::make_errno_error_inval();
    }

    fastotv::commands_info::RecentStreamTimeInfo fav;
    common::Error err_des = fav.DeSerialize(jfav);
    json_object_put(jfav);
    if (err_des) {
      const std::string err_str = err_des->GetDescription();
      return common::make_errno_error(err_str, EAGAIN);
    }

    // write to DB
    auto login = client->GetLogin();
    manager_->SetRecent(*login, fav);
    return client->GetRecentInfoSuccess(req->id);
  }

  return common::make_errno_error_inval();
}

common::ErrnoError SubscribersHandler::HandleRequestInterruptStreamTime(SubscriberClient* client,
                                                                        fastotv::protocol::request_t* req) {
  base::ServerDBAuthInfo auth;
  common::Error err = manager_->CheckIsLoginClient(client, &auth);
  if (err) {
    ignore_result(client->CheckLoginFail(req->id, err));
    ignore_result(client->Close());
    delete client;
    return common::make_errno_error(err->GetDescription(), EINVAL);
  }

  if (req->params) {
    const char* params_ptr = req->params->c_str();
    json_object* jinter = json_tokener_parse(params_ptr);
    if (!jinter) {
      return common::make_errno_error_inval();
    }

    fastotv::commands_info::InterruptStreamTimeInfo inter;
    common::Error err_des = inter.DeSerialize(jinter);
    json_object_put(jinter);
    if (err_des) {
      const std::string err_str = err_des->GetDescription();
      return common::make_errno_error(err_str, EAGAIN);
    }

    // write to DB
    auto login = client->GetLogin();
    manager_->SetInterruptTime(*login, inter);
    return client->GetInterruptStreamTimeInfoSuccess(req->id);
  }

  return common::make_errno_error_inval();
}

common::ErrnoError SubscribersHandler::HandleRequestGenerateCatchup(SubscriberClient* client,
                                                                    fastotv::protocol::request_t* req) {
  base::ServerDBAuthInfo auth;
  common::Error err = manager_->CheckIsLoginClient(client, &auth);
  if (err) {
    ignore_result(client->CheckLoginFail(req->id, err));
    ignore_result(client->Close());
    delete client;
    return common::make_errno_error(err->GetDescription(), EINVAL);
  }

  if (req->params) {
    const char* params_ptr = req->params->c_str();
    json_object* jcatch = json_tokener_parse(params_ptr);
    if (!jcatch) {
      return common::make_errno_error_inval();
    }

    fastotv::commands_info::CatchupGenerateInfo cat_gen;
    common::Error err_des = cat_gen.DeSerialize(jcatch);
    json_object_put(jcatch);
    if (err_des) {
      const std::string err_str = err_des->GetDescription();
      return common::make_errno_error(err_str, EAGAIN);
    }

    bool is_created = false;
    fastotv::commands_info::CatchupInfo chan;
    common::Error err = manager_->CreateCatchup(auth, cat_gen.GetStreamID(), cat_gen.GetTitle(), cat_gen.GetStart(),
                                                cat_gen.GetStop(), &chan, &is_created);
    if (err) {
      const std::string err_str = err->GetDescription();
      client->CatchupGenerateFail(req->id, err);
      return common::make_errno_error(err_str, EAGAIN);
    }

    err = manager_->AddUserCatchup(auth, chan.GetStreamID());
    if (err) {
      const std::string err_str = err->GetDescription();
      client->CatchupGenerateFail(req->id, err);
      return common::make_errno_error(err_str, EAGAIN);
    }

    if (is_created) {
      if (observer_) {
        observer_->CatchupCreated(this, chan);
      }
    }

    fastotv::commands_info::CatchupQueueInfo qcatch(chan);
    return client->CatchupGenerateSuccess(req->id, qcatch);
  }

  return common::make_errno_error_inval();
}

common::ErrnoError SubscribersHandler::HandleRequestUndoCatchup(SubscriberClient* client,
                                                                fastotv::protocol::request_t* req) {
  base::ServerDBAuthInfo auth;
  common::Error err = manager_->CheckIsLoginClient(client, &auth);
  if (err) {
    ignore_result(client->CheckLoginFail(req->id, err));
    ignore_result(client->Close());
    delete client;
    return common::make_errno_error(err->GetDescription(), EINVAL);
  }

  if (req->params) {
    const char* params_ptr = req->params->c_str();
    json_object* jcatch = json_tokener_parse(params_ptr);
    if (!jcatch) {
      return common::make_errno_error_inval();
    }

    fastotv::commands_info::CatchupUndoInfo cat_undo;
    common::Error err_des = cat_undo.DeSerialize(jcatch);
    json_object_put(jcatch);
    if (err_des) {
      const std::string err_str = err_des->GetDescription();
      return common::make_errno_error(err_str, EAGAIN);
    }

    err = manager_->RemoveUserCatchup(auth, cat_undo.GetStreamID());
    if (err) {
      const std::string err_str = err->GetDescription();
      client->CatchupUndoFail(req->id, err);
      return common::make_errno_error(err_str, EAGAIN);
    }

    return client->CatchupUndoSuccess(req->id);
  }

  return common::make_errno_error_inval();
}

common::ErrnoError SubscribersHandler::HandleResponceServerPing(SubscriberClient* client,
                                                                fastotv::protocol::response_t* resp) {
  UNUSED(client);
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

common::ErrnoError SubscribersHandler::HandleResponceServerGetClientInfo(SubscriberClient* client,
                                                                         fastotv::protocol::response_t* resp) {
  if (resp->IsMessage()) {
    const char* params_ptr = resp->message->result.c_str();
    json_object* jclient_info = json_tokener_parse(params_ptr);
    if (!jclient_info) {
      return common::make_errno_error_inval();
    }

    fastotv::commands_info::ClientInfo cinf;
    common::Error err_des = cinf.DeSerialize(jclient_info);
    json_object_put(jclient_info);
    if (err_des) {
      const std::string err_str = err_des->GetDescription();
      return common::make_errno_error(err_str, EAGAIN);
    }
    client->SetClInfo(cinf);
    return common::ErrnoError();
  }
  return common::ErrnoError();
}

}  // namespace subscribers
}  // namespace server
}  // namespace fastocloud
