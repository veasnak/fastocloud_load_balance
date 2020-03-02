/*  Copyright (C) 2014-2020 FastoGT. All right reserved.
    This file is part of FastoTV.
    FastoTV is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    FastoTV is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with FastoTV. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <fastotv/commands_info/auth_info.h>

namespace fastocloud {
namespace server {
namespace base {

class ServerDBAuthInfo : public fastotv::commands_info::ServerAuthInfo {
 public:
  typedef fastotv::commands_info::ServerAuthInfo base_class;

  ServerDBAuthInfo();
  ServerDBAuthInfo(fastotv::user_id_t uid, const ServerAuthInfo& auth);

  bool IsValid() const;
  fastotv::user_id_t GetUserID() const;
  bool Equals(const ServerDBAuthInfo& auth) const;

 protected:
  common::Error DoDeSerialize(json_object* serialized) override;
  common::Error SerializeFields(json_object* deserialized) const override;

 private:
  fastotv::user_id_t uid_;
};

}  // namespace base
}  // namespace server
}  // namespace fastocloud
