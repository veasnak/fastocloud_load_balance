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

#include "config.h"

#include <fstream>
#include <utility>

#include <common/value.h>

#define SERVICE_LOG_PATH_FIELD "log_path"
#define SERVICE_LOG_LEVEL_FIELD "log_level"
#define SERVICE_HOST_FIELD "host"
#define SERVICE_HTTP_HOST_FIELD "http_host"
#define SERVICE_SUBSCRIBERS_HOST_FIELD "subscribers_host"
#define SERVICE_MONGODB_URL_FIELD "mongodb_url"
#define SERVICE_EPG_URL_FIELD "epg_url"
#define SERVICE_CATCHUP_HOST_FIELD "catchups_host"
#define SERVICE_CATCHUP_HTTP_ROOT_FIELD "catchups_http_root"

#define DUMMY_LOG_FILE_PATH "/dev/null"

namespace {
std::pair<std::string, std::string> GetKeyValue(const std::string& line, char separator) {
  const size_t pos = line.find(separator);
  if (pos != std::string::npos) {
    const std::string key = line.substr(0, pos);
    const std::string value = line.substr(pos + 1);
    return std::make_pair(key, value);
  }

  return std::make_pair(line, std::string());
}

common::ErrnoError ReadConfigFile(const std::string& path, common::HashValue** args) {
  if (!args) {
    return common::make_errno_error_inval();
  }

  if (path.empty()) {
    return common::make_errno_error("Invalid config path", EINVAL);
  }

  std::ifstream config(path);
  if (!config.is_open()) {
    return common::make_errno_error("Failed to open config file", EINVAL);
  }

  common::HashValue* options = new common::HashValue;
  std::string line;
  while (getline(config, line)) {
    const std::pair<std::string, std::string> pair = GetKeyValue(line, '=');
    if (pair.first == SERVICE_LOG_PATH_FIELD) {
      options->Insert(pair.first, common::Value::CreateStringValueFromBasicString(pair.second));
    } else if (pair.first == SERVICE_LOG_LEVEL_FIELD) {
      options->Insert(pair.first, common::Value::CreateStringValueFromBasicString(pair.second));
    } else if (pair.first == SERVICE_HOST_FIELD) {
      options->Insert(pair.first, common::Value::CreateStringValueFromBasicString(pair.second));
    } else if (pair.first == SERVICE_SUBSCRIBERS_HOST_FIELD) {
      options->Insert(pair.first, common::Value::CreateStringValueFromBasicString(pair.second));
    } else if (pair.first == SERVICE_HTTP_HOST_FIELD) {
      options->Insert(pair.first, common::Value::CreateStringValueFromBasicString(pair.second));
    } else if (pair.first == SERVICE_MONGODB_URL_FIELD) {
      options->Insert(pair.first, common::Value::CreateStringValueFromBasicString(pair.second));
    } else if (pair.first == SERVICE_EPG_URL_FIELD) {
      options->Insert(pair.first, common::Value::CreateStringValueFromBasicString(pair.second));
    } else if (pair.first == SERVICE_CATCHUP_HOST_FIELD) {
      options->Insert(pair.first, common::Value::CreateStringValueFromBasicString(pair.second));
    } else if (pair.first == SERVICE_CATCHUP_HTTP_ROOT_FIELD) {
      options->Insert(pair.first, common::Value::CreateStringValueFromBasicString(pair.second));
    }
  }

  *args = options;
  return common::ErrnoError();
}

}  // namespace

namespace fastocloud {
namespace server {

Config::Config()
    : host(GetDefaultHost()),
      log_path(DUMMY_LOG_FILE_PATH),
      log_level(common::logging::LOG_LEVEL_INFO),
      mongodb_url(MONGODB_URL),
      epg_url(EPG_URL),
      catchup_host(GetCatchupDefaultHost()),
      catchups_http_root(CATCHUPS_HTTP_ROOT) {}

common::net::HostAndPort Config::GetDefaultHost() {
  common::net::HostAndPort result;
  if (common::ConvertFromString(SERVICE_HOST, &result)) {
    return result;
  }
  return result;
}

common::net::HostAndPort Config::GetCatchupDefaultHost() {
  common::net::HostAndPort result;
  if (common::ConvertFromString(CATCHUPS_HOST, &result)) {
    return result;
  }
  return result;
}

common::ErrnoError load_config_from_file(const std::string& config_absolute_path, Config* config) {
  if (!config) {
    return common::make_errno_error_inval();
  }

  Config lconfig;
  common::HashValue* slave_config_args = nullptr;
  common::ErrnoError err = ReadConfigFile(config_absolute_path, &slave_config_args);
  if (err) {
    return err;
  }

  common::Value* log_path_field = slave_config_args->Find(SERVICE_LOG_PATH_FIELD);
  if (!log_path_field || !log_path_field->GetAsBasicString(&lconfig.log_path)) {
    lconfig.log_path = DUMMY_LOG_FILE_PATH;
  }

  std::string level_str;
  common::logging::LOG_LEVEL level = common::logging::LOG_LEVEL_INFO;
  common::Value* level_field = slave_config_args->Find(SERVICE_LOG_LEVEL_FIELD);
  if (level_field && level_field->GetAsBasicString(&level_str)) {
    if (!common::logging::text_to_log_level(level_str.c_str(), &level)) {
      level = common::logging::LOG_LEVEL_INFO;
    }
  }
  lconfig.log_level = level;

  common::Value* host_field = slave_config_args->Find(SERVICE_HOST_FIELD);
  std::string host_str;
  if (!host_field || !host_field->GetAsBasicString(&host_str) || !common::ConvertFromString(host_str, &lconfig.host)) {
    lconfig.host = Config::GetDefaultHost();
  }

  common::Value* clients_host_field = slave_config_args->Find(SERVICE_SUBSCRIBERS_HOST_FIELD);
  std::string clients_host_str;
  if (!clients_host_field || !clients_host_field->GetAsBasicString(&clients_host_str) ||
      !common::ConvertFromString(clients_host_str, &lconfig.subscribers_host)) {
    lconfig.subscribers_host = common::net::HostAndPort::CreateLocalHost(SUBSCRIBERS_PORT);
  }

  common::Value* mongodb_url_field = slave_config_args->Find(SERVICE_MONGODB_URL_FIELD);
  if (!mongodb_url_field || !mongodb_url_field->GetAsBasicString(&lconfig.mongodb_url)) {
    lconfig.mongodb_url = MONGODB_URL;
  }

  common::Value* http_host_field = slave_config_args->Find(SERVICE_HTTP_HOST_FIELD);
  std::string http_host_str;
  if (!http_host_field || !http_host_field->GetAsBasicString(&http_host_str) ||
      !common::ConvertFromString(http_host_str, &lconfig.http_host)) {
    lconfig.http_host = common::net::HostAndPort::CreateLocalHost(HTTP_PORT);
  }

  common::Value* epg_url_field = slave_config_args->Find(SERVICE_EPG_URL_FIELD);
  std::string epg_url_str;
  if (!epg_url_field || !epg_url_field->GetAsBasicString(&epg_url_str) ||
      !common::ConvertFromString(epg_url_str, &lconfig.epg_url)) {
    lconfig.epg_url = common::uri::Url(EPG_URL);
  }

  common::Value* catchup_host_field = slave_config_args->Find(SERVICE_CATCHUP_HOST_FIELD);
  std::string catchup_host_str;
  if (!catchup_host_field || !catchup_host_field->GetAsBasicString(&catchup_host_str) ||
      !common::ConvertFromString(catchup_host_str, &lconfig.catchup_host)) {
    lconfig.catchup_host = Config::GetCatchupDefaultHost();
  }

  common::file_system::ascii_directory_string_path catchups_http_root(CATCHUPS_HTTP_ROOT);
  common::Value* catchup_http_root_field = slave_config_args->Find(SERVICE_CATCHUP_HTTP_ROOT_FIELD);
  std::string catchup_http_root_str;
  if (catchup_http_root_field && catchup_http_root_field->GetAsBasicString(&catchup_http_root_str)) {
    catchups_http_root = common::file_system::ascii_directory_string_path(catchup_http_root_str);
  }
  lconfig.catchups_http_root = catchups_http_root;

  *config = lconfig;
  delete slave_config_args;
  return common::ErrnoError();
}

}  // namespace server
}  // namespace fastocloud
