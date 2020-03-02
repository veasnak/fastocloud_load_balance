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

#include "http/handler.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include <common/convert2string.h>

#include "base/isubscribers_manager.h"

#include "http/client.h"

namespace fastocloud {
namespace server {
namespace http {

HttpHandler::HttpHandler(base::ISubscribersManager* manager) : base_class(), manager_(manager) {}

void HttpHandler::PreLooped(common::libev::IoLoop* server) {
  UNUSED(server);
}

void HttpHandler::Accepted(common::libev::IoClient* client) {
  base_class::Accepted(client);
}

void HttpHandler::Moved(common::libev::IoLoop* server, common::libev::IoClient* client) {
  base_class::Moved(server, client);
}

void HttpHandler::Closed(common::libev::IoClient* client) {
  HttpClient* iclient = static_cast<HttpClient*>(client);
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

  INFO_LOG() << "Bye http registered user: " << server_user_auth->GetLogin();
  base_class::Closed(client);
}

void HttpHandler::TimerEmited(common::libev::IoLoop* server, common::libev::timer_id_t id) {
  UNUSED(server);
  UNUSED(id);
}

void HttpHandler::Accepted(common::libev::IoChild* child) {
  UNUSED(child);
}

void HttpHandler::Moved(common::libev::IoLoop* server, common::libev::IoChild* child) {
  UNUSED(server);
  UNUSED(child);
}

void HttpHandler::ChildStatusChanged(common::libev::IoChild* child, int status, int signal) {
  UNUSED(child);
  UNUSED(status);
  UNUSED(signal);
}

void HttpHandler::DataReceived(common::libev::IoClient* client) {
  char buff[BUF_SIZE] = {0};
  size_t nread = 0;
  common::ErrnoError errn = client->SingleRead(buff, BUF_SIZE - 1, &nread);
  if ((errn && errn->GetErrorCode() != EAGAIN) || nread == 0) {
    ignore_result(client->Close());
    delete client;
    return;
  }

  HttpClient* hclient = static_cast<HttpClient*>(client);
  ProcessReceived(hclient, buff, nread);
}

void HttpHandler::DataReadyToWrite(common::libev::IoClient* client) {
  UNUSED(client);
}

void HttpHandler::PostLooped(common::libev::IoLoop* server) {
  UNUSED(server);
}

void HttpHandler::ProcessReceived(HttpClient* hclient, const char* request, size_t req_len) {
  static const common::libev::http::HttpServerInfo hinf(PROJECT_NAME_TITLE, PROJECT_DOMAIN);
  common::http::HttpRequest hrequest;
  std::string request_str(request, req_len);
  std::pair<common::http::http_status, common::Error> result = common::http::parse_http_request(request_str, &hrequest);
  DEBUG_LOG() << "Http request:\n" << request;

  if (result.second) {
    const std::string error_text = result.second->GetDescription();
    DEBUG_MSG_ERROR(result.second, common::logging::LOG_LEVEL_ERR);
    common::ErrnoError err =
        hclient->SendError(common::http::HP_1_1, result.first, nullptr, error_text.c_str(), false, hinf);
    if (err) {
      DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
    }
    ignore_result(hclient->Close());
    delete hclient;
    return;
  }

  // keep alive
  common::http::header_t connection_field;
  bool is_find_connection = hrequest.FindHeaderByKey("Connection", false, &connection_field);
  bool IsKeepAlive = is_find_connection ? common::EqualsASCII(connection_field.value, "Keep-Alive", false) : false;
  const common::http::http_protocol protocol = hrequest.GetProtocol();
  const char* extra_header = "Access-Control-Allow-Origin: *";
  if (hrequest.GetMethod() == common::http::http_method::HM_GET ||
      hrequest.GetMethod() == common::http::http_method::HM_HEAD) {
    common::uri::Upath path = hrequest.GetPath();
    if (!path.IsValid() || path.IsRoot()) {  // for hls
      common::ErrnoError err =
          hclient->SendError(protocol, common::http::HS_NOT_FOUND, extra_header, "Invalid request.", IsKeepAlive, hinf);
      if (err) {
        DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      }
      goto finish;
    }

    std::vector<std::string> tokens;
    const size_t levels = common::Tokenize(path.GetPath(), "/", &tokens);
    if (levels < 6) {
      common::ErrnoError err =
          hclient->SendError(protocol, common::http::HS_NOT_FOUND, extra_header, "Invalid request.", IsKeepAlive, hinf);
      if (err) {
        DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      }
      goto finish;
    }

    base::ServerDBAuthInfo maybe_auth;
    common::Error cerr = manager_->CheckIsLoginClient(hclient, &maybe_auth);
    if (cerr) {
      // user_id/password_hash/device_id/stream_id/channel_id/file
      const fastotv::user_id_t user_uid = tokens[0];
      const std::string password = tokens[1];
      const fastotv::device_id_t dev = tokens[2];
      // try to check login
      cerr = manager_->ClientLogin(user_uid, password, dev, &maybe_auth);
      if (cerr) {
        const std::string err_desc = cerr->GetDescription();
        common::ErrnoError errn =
            hclient->SendError(protocol, common::http::HS_NOT_FOUND, extra_header, err_desc.c_str(), IsKeepAlive, hinf);
        if (errn) {
          DEBUG_MSG_ERROR(errn, common::logging::LOG_LEVEL_ERR);
        }
        goto finish;
      }
      cerr = manager_->RegisterInnerConnectionByHost(hclient, maybe_auth);
      DCHECK(!cerr) << "Register inner connection error: " << cerr->GetDescription();
      INFO_LOG() << "Welcome registered user: " << maybe_auth.GetLogin();
    }

    const fastotv::stream_id_t sid = tokens[3];
    fastotv::channel_id_t cid;
    const std::string file_name = tokens[5];
    if (!common::ConvertFromString(tokens[4], &cid)) {
      common::ErrnoError err = hclient->SendError(protocol, common::http::HS_NOT_FOUND, extra_header,
                                                  "Invalid channel id.", IsKeepAlive, hinf);
      if (err) {
        DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      }
      goto finish;
    }

    base::ISubscribersManager::http_directory_t directory;
    common::uri::Url url;
    cerr = manager_->ClientFindHttpDirectoryOrUrlForChannel(maybe_auth, sid, cid, &directory, &url);
    if (cerr) {
      const std::string err_desc = cerr->GetDescription();
      common::ErrnoError errn =
          hclient->SendError(protocol, common::http::HS_NOT_FOUND, extra_header, err_desc.c_str(), IsKeepAlive, hinf);
      if (errn) {
        DEBUG_MSG_ERROR(errn, common::logging::LOG_LEVEL_ERR);
      }
      goto finish;
    }

    if (!directory.IsValid()) {
      DCHECK(url.IsValid());
      const std::string url_str = url.GetUrl();
      const std::string redirect_header = common::MemSPrintf("Location: %s\r\n", url_str);
      common::ErrnoError err =
          hclient->SendHeaders(protocol, common::http::HS_PERMANENT_REDIRECT, redirect_header.c_str(), nullptr, nullptr,
                               nullptr, IsKeepAlive, hinf);
      if (err) {
        DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      } else {
        DEBUG_LOG() << "Sent redirect to: " << url_str;
        hclient->SetCurrentStreamID(sid);
      }
      goto finish;
    }

    auto file_path = directory.MakeFileStringPath(file_name);
    if (!file_path) {
      common::ErrnoError err =
          hclient->SendError(protocol, common::http::HS_NOT_FOUND, extra_header, "File not found.", IsKeepAlive, hinf);
      if (err) {
        DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      }
      goto finish;
    }

    const std::string file_path_str = file_path->GetPath();
    int open_flags = O_RDONLY;
    struct stat sb;
    if (stat(file_path_str.c_str(), &sb) < 0) {
      common::ErrnoError err =
          hclient->SendError(protocol, common::http::HS_NOT_FOUND, extra_header, "File not found.", IsKeepAlive, hinf);
      WARNING_LOG() << "File path: " << file_path_str << ", not found";
      if (err) {
        DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      }
      goto finish;
    }

    if (S_ISDIR(sb.st_mode)) {
      common::ErrnoError err =
          hclient->SendError(protocol, common::http::HS_BAD_REQUEST, extra_header, "Bad filename.", IsKeepAlive, hinf);
      if (err) {
        DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      }
      goto finish;
    }

    int file = open(file_path_str.c_str(), open_flags);
    if (file == INVALID_DESCRIPTOR) { /* open the file for reading */
      common::ErrnoError err = hclient->SendError(protocol, common::http::HS_FORBIDDEN, extra_header,
                                                  "File is protected.", IsKeepAlive, hinf);
      if (err) {
        DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      }
      goto finish;
    }

    const std::string mime = path.GetMime();
    common::ErrnoError err = hclient->SendHeaders(protocol, common::http::HS_OK, extra_header, mime.c_str(),
                                                  &sb.st_size, &sb.st_mtime, IsKeepAlive, hinf);
    if (err) {
      DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      ::close(file);
      goto finish;
    }

    if (hrequest.GetMethod() == common::http::http_method::HM_GET) {
      common::ErrnoError err = hclient->SendFileByFd(protocol, file, sb.st_size);
      if (err) {
        DEBUG_MSG_ERROR(err, common::logging::LOG_LEVEL_ERR);
      } else {
        DEBUG_LOG() << "Sent file path: " << file_path_str << ", size: " << sb.st_size;
        hclient->SetCurrentStreamID(sid);
      }
    }

    ::close(file);
  }

finish:
  if (!IsKeepAlive) {
    ignore_result(hclient->Close());
    delete hclient;
  }
}

}  // namespace http
}  // namespace server
}  // namespace fastocloud
