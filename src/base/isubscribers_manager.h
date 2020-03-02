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

#include <common/file_system/path.h>

#include <fastotv/commands_info/catchups_info.h>
#include <fastotv/commands_info/channels_info.h>
#include <fastotv/commands_info/devices_info.h>
#include <fastotv/commands_info/favorite_info.h>
#include <fastotv/commands_info/interrupt_stream_time_info.h>
#include <fastotv/commands_info/recent_stream_time_info.h>
#include <fastotv/commands_info/vods_info.h>

#include "base/subscriber_info.h"

namespace fastocloud {
namespace server {
namespace base {

class ISubscribersManager {
 public:
  typedef common::file_system::ascii_directory_string_path http_directory_t;

  virtual common::Error RegisterInnerConnectionByHost(SubscriberInfo* client,
                                                      const ServerDBAuthInfo& info) WARN_UNUSED_RESULT = 0;
  virtual common::Error UnRegisterInnerConnectionByHost(SubscriberInfo* client) WARN_UNUSED_RESULT = 0;
  virtual common::Error CheckIsLoginClient(SubscriberInfo* client, ServerDBAuthInfo* ser) const WARN_UNUSED_RESULT = 0;

  virtual size_t GetAndUpdateOnlineUserByStreamID(fastotv::stream_id_t sid) = 0;

  virtual common::Error ClientActivate(const fastotv::commands_info::LoginInfo& uauth,
                                       fastotv::commands_info::DevicesInfo* dev) WARN_UNUSED_RESULT = 0;
  virtual common::Error ClientLogin(fastotv::user_id_t uid,
                                    const std::string& password,
                                    fastotv::device_id_t dev,
                                    ServerDBAuthInfo* ser) = 0;
  virtual common::Error ClientLogin(const fastotv::commands_info::AuthInfo& uauth,
                                    ServerDBAuthInfo* ser) WARN_UNUSED_RESULT = 0;
  virtual common::Error ClientGetChannels(const fastotv::commands_info::AuthInfo& auth,
                                          fastotv::commands_info::ChannelsInfo* chans,
                                          fastotv::commands_info::VodsInfo* vods,
                                          fastotv::commands_info::ChannelsInfo* pchans,
                                          fastotv::commands_info::VodsInfo* pvods,
                                          fastotv::commands_info::CatchupsInfo* catchups) WARN_UNUSED_RESULT = 0;
  virtual common::Error ClientFindHttpDirectoryOrUrlForChannel(const fastotv::commands_info::AuthInfo& auth,
                                                               fastotv::stream_id_t sid,
                                                               fastotv::channel_id_t cid,
                                                               http_directory_t* directory,
                                                               common::uri::Url* url) WARN_UNUSED_RESULT = 0;

  virtual common::Error SetFavorite(const base::ServerDBAuthInfo& auth,
                                    const fastotv::commands_info::FavoriteInfo& favorite) WARN_UNUSED_RESULT = 0;
  virtual common::Error SetRecent(const base::ServerDBAuthInfo& auth,
                                  const fastotv::commands_info::RecentStreamTimeInfo& recent) WARN_UNUSED_RESULT = 0;
  virtual common::Error SetInterruptTime(const base::ServerDBAuthInfo& auth,
                                         const fastotv::commands_info::InterruptStreamTimeInfo& inter)
      WARN_UNUSED_RESULT = 0;

  virtual common::Error FindStream(const base::ServerDBAuthInfo& auth,
                                   fastotv::stream_id_t sid,
                                   fastotv::commands_info::ChannelInfo* chan) const WARN_UNUSED_RESULT = 0;
  virtual common::Error FindVod(const base::ServerDBAuthInfo& auth,
                                fastotv::stream_id_t sid,
                                fastotv::commands_info::VodInfo* vod) const WARN_UNUSED_RESULT = 0;
  virtual common::Error FindCatchup(const base::ServerDBAuthInfo& auth,
                                    fastotv::stream_id_t sid,
                                    fastotv::commands_info::CatchupInfo* cat) const WARN_UNUSED_RESULT = 0;

  virtual common::Error CreateCatchup(const base::ServerDBAuthInfo& auth,
                                      fastotv::stream_id_t sid,
                                      const std::string& title,
                                      fastotv::timestamp_t start,
                                      fastotv::timestamp_t stop,
                                      fastotv::commands_info::CatchupInfo* cat,
                                      bool* is_created) WARN_UNUSED_RESULT = 0;

  virtual common::Error RemoveUserStream(const base::ServerDBAuthInfo& auth,
                                         fastotv::stream_id_t sid) WARN_UNUSED_RESULT = 0;

  virtual common::Error AddUserStream(const base::ServerDBAuthInfo& auth,
                                      fastotv::stream_id_t sid) WARN_UNUSED_RESULT = 0;

  virtual common::Error RemoveUserVod(const base::ServerDBAuthInfo& auth,
                                      fastotv::stream_id_t sid) WARN_UNUSED_RESULT = 0;

  virtual common::Error AddUserVod(const base::ServerDBAuthInfo& auth, fastotv::stream_id_t sid) WARN_UNUSED_RESULT = 0;

  virtual common::Error RemoveUserCatchup(const base::ServerDBAuthInfo& auth,
                                          fastotv::stream_id_t sid) WARN_UNUSED_RESULT = 0;

  virtual common::Error AddUserCatchup(const base::ServerDBAuthInfo& auth,
                                       fastotv::stream_id_t sid) WARN_UNUSED_RESULT = 0;

  virtual ~ISubscribersManager();
};

}  // namespace base
}  // namespace server
}  // namespace fastocloud
