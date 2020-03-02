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

#include "base/subscriber_info.h"

namespace fastocloud {
namespace server {
namespace base {

SubscriberInfo::SubscriberInfo() : login_(), current_stream_id_(fastotv::invalid_stream_id) {}

void SubscriberInfo::SetCurrentStreamID(fastotv::stream_id_t sid) {
  current_stream_id_ = sid;
}

fastotv::stream_id_t SubscriberInfo::GetCurrentStreamID() const {
  return current_stream_id_;
}

void SubscriberInfo::SetLoginInfo(login_t login) {
  login_ = login;
}

SubscriberInfo::login_t SubscriberInfo::GetLogin() const {
  return login_;
}

}  // namespace base
}  // namespace server
}  // namespace fastocloud
