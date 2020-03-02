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

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <common/net/types.h>

#include "base/isubscribers_manager.h"

typedef struct _mongoc_client_t mongoc_client_t;
typedef struct _mongoc_collection_t mongoc_collection_t;
typedef struct _bson_t bson_t;

namespace fastocloud {
namespace server {
namespace mongo {

class SubscribersManager : public base::ISubscribersManager {
 public:
  typedef std::unordered_map<fastotv::user_id_t, std::vector<base::SubscriberInfo*>> inner_connections_t;
  SubscribersManager(const common::net::HostAndPort& catchup_host,
                     const common::file_system::ascii_directory_string_path& catchups_http_root);

  common::ErrnoError ConnectToDatabase(const std::string& mongodb_url) WARN_UNUSED_RESULT;
  common::ErrnoError Disconnect() WARN_UNUSED_RESULT;

  common::Error RegisterInnerConnectionByHost(base::SubscriberInfo* client,
                                              const base::ServerDBAuthInfo& info) override WARN_UNUSED_RESULT;
  common::Error UnRegisterInnerConnectionByHost(base::SubscriberInfo* client) override WARN_UNUSED_RESULT;

  common::Error CheckIsLoginClient(base::SubscriberInfo* client,
                                   base::ServerDBAuthInfo* ser) const override WARN_UNUSED_RESULT;
  size_t GetAndUpdateOnlineUserByStreamID(fastotv::stream_id_t sid) override;

  common::Error ClientActivate(const fastotv::commands_info::LoginInfo& uauth,
                               fastotv::commands_info::DevicesInfo* dev) override WARN_UNUSED_RESULT;
  common::Error ClientLogin(fastotv::user_id_t uid,
                            const std::string& password,
                            fastotv::device_id_t dev,
                            base::ServerDBAuthInfo* ser) override WARN_UNUSED_RESULT;
  common::Error ClientLogin(const fastotv::commands_info::AuthInfo& uauth,
                            base::ServerDBAuthInfo* ser) override WARN_UNUSED_RESULT;
  common::Error ClientGetChannels(const fastotv::commands_info::AuthInfo& auth,
                                  fastotv::commands_info::ChannelsInfo* chans,
                                  fastotv::commands_info::VodsInfo* vods,
                                  fastotv::commands_info::ChannelsInfo* pchans,
                                  fastotv::commands_info::VodsInfo* pvods,
                                  fastotv::commands_info::CatchupsInfo* catchups) override WARN_UNUSED_RESULT;

  common::Error ClientFindHttpDirectoryOrUrlForChannel(const fastotv::commands_info::AuthInfo& auth,
                                                       fastotv::stream_id_t sid,
                                                       fastotv::channel_id_t cid,
                                                       http_directory_t* directory,
                                                       common::uri::Url* url) override WARN_UNUSED_RESULT;

  common::Error SetFavorite(const base::ServerDBAuthInfo& auth,
                            const fastotv::commands_info::FavoriteInfo& favorite) override WARN_UNUSED_RESULT;
  common::Error SetRecent(const base::ServerDBAuthInfo& auth,
                          const fastotv::commands_info::RecentStreamTimeInfo& recent) override WARN_UNUSED_RESULT;
  common::Error SetInterruptTime(const base::ServerDBAuthInfo& auth,
                                 const fastotv::commands_info::InterruptStreamTimeInfo& inter) override
      WARN_UNUSED_RESULT;

  common::Error FindStream(const base::ServerDBAuthInfo& auth,
                           fastotv::stream_id_t sid,
                           fastotv::commands_info::ChannelInfo* chan) const override WARN_UNUSED_RESULT;
  common::Error FindVod(const base::ServerDBAuthInfo& auth,
                        fastotv::stream_id_t sid,
                        fastotv::commands_info::VodInfo* vod) const override WARN_UNUSED_RESULT;
  common::Error FindCatchup(const base::ServerDBAuthInfo& auth,
                            fastotv::stream_id_t sid,
                            fastotv::commands_info::CatchupInfo* cat) const override WARN_UNUSED_RESULT;

  common::Error CreateCatchup(const base::ServerDBAuthInfo& auth,
                              fastotv::stream_id_t sid,
                              const std::string& title,
                              fastotv::timestamp_t start,
                              fastotv::timestamp_t stop,
                              fastotv::commands_info::CatchupInfo* cat,
                              bool* is_created) override WARN_UNUSED_RESULT;

  common::Error RemoveUserStream(const base::ServerDBAuthInfo& auth,
                                 fastotv::stream_id_t sid) override WARN_UNUSED_RESULT;
  common::Error AddUserStream(const base::ServerDBAuthInfo& auth, fastotv::stream_id_t sid) override WARN_UNUSED_RESULT;

  common::Error RemoveUserVod(const base::ServerDBAuthInfo& auth, fastotv::stream_id_t sid) override WARN_UNUSED_RESULT;
  common::Error AddUserVod(const base::ServerDBAuthInfo& auth, fastotv::stream_id_t sid) override WARN_UNUSED_RESULT;

  common::Error RemoveUserCatchup(const base::ServerDBAuthInfo& auth,
                                  fastotv::stream_id_t sid) override WARN_UNUSED_RESULT;
  common::Error AddUserCatchup(const base::ServerDBAuthInfo& auth,
                               fastotv::stream_id_t sid) override WARN_UNUSED_RESULT;

 private:
  common::Error CreateOrFindCatchup(const fastotv::commands_info::ChannelInfo& based_on,
                                    const std::string& title,
                                    fastotv::timestamp_t start,
                                    fastotv::timestamp_t stop,
                                    fastotv::commands_info::CatchupInfo* cat,
                                    bool* is_created) WARN_UNUSED_RESULT;

  common::Error ClientLoginImpl(fastotv::user_id_t uid,
                                const fastotv::commands_info::ServerAuthInfo& auth,
                                const bson_t* doc) WARN_UNUSED_RESULT;

  std::mutex connections_mutex_;
  inner_connections_t connections_;

  mongoc_client_t* client_;
  mongoc_collection_t* subscribers_;
  mongoc_collection_t* servers_;
  mongoc_collection_t* streams_;
  const common::net::HostAndPort catchup_host_;
  const common::file_system::ascii_directory_string_path catchups_http_root_;
};

}  // namespace mongo
}  // namespace server
}  // namespace fastocloud
