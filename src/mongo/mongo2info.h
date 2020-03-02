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

#include <bson.h>

#include <string>
#include <vector>

#include <common/file_system/path.h>

#include <fastotv/commands_info/catchup_info.h>
#include <fastotv/commands_info/channel_info.h>
#include <fastotv/commands_info/vod_info.h>
#include <fastotv/types/output_uri.h>

// stream base part
#define STREAM_ID_FIELD "_id"
#define STREAM_CREATE_DATE_FIELD "created_date"
#define STREAM_GROUP_FIELD "group"
#define STREAM_IARC_FIELD "iarc"
#define STREAM_NAME_FIELD "name"
#define STREAM_CLS_FIELD "_cls"
#define STREAM_HAVE_VIDEO_FIELD "have_video"
#define STREAM_HAVE_AUDIO_FIELD "have_audio"
#define STREAM_PARTS_FIELD "parts"
#define STREAM_PRICE_FIELD "price"
#define STREAM_VISIBLE_FIELD "visible"
#define STREAM_LOG_LEVEL_FIELD "log_level"
#define STREAM_LOOP_FIELD "loop"
#define STREAM_AVFORMAT_FIELD "avformat"
#define STREAM_RESTART_ATTEMPTS_FIELD "restart_attempts"
#define STREAM_AUTO_EXIT_FIELD "auto_exit_time"
#define STREAM_EXTRA_CONFIG_ARGS_FIELD "extra_config_fields"
#define STREAM_VIDEO_PARSER_FIELD "video_parser"
#define STREAM_AUDIO_PARSER_FIELD "audio_parser"
#define STREAM_AUDIO_SELECT_FIELD "audio_select"
// input part
#define STREAM_INPUT_FIELD "input"
// output part
#define STREAM_OUTPUT_FIELD "output"
#define STREAM_OUTPUT_URLS_ID_FIELD "id"
#define STREAM_OUTPUT_URLS_URI_FIELD "uri"
#define STREAM_OUTPUT_URLS_HTTP_ROOT_FIELD "http_root"
#define STREAM_OUTPUT_URLS_HLS_TYPE_FIELD "hls_type"

// vods/series
#define VOD_DESCRIPTION_FIELD "description"
#define VOD_PRVIEW_ICON_FIELD "tvg_logo"
#define VOD_TYPE_FIELD "vod_type"
#define VOD_TRAILER_URL_FIELD "trailer_url"
#define VOD_USER_SCORE_FIELD "user_score"
#define VOD_PRIME_DATE_FIELD "prime_date"
#define VOD_COUNTRY_FIELD "country"
#define VOD_DURATION_FIELD "duration"

// live streams
#define CHANNEL_TVG_ID_FIELD "tvg_id"
#define CHANNEL_TVG_NAME_FIELD "tvg_name"
#define CHANNEL_TVG_LOGO_FIELD "tvg_logo"

// timeshift
#define TIMESHIFT_CHUNK_DURATION_FIELD "timeshift_chunk_duration"
#define TIMESHIFT_CHUNK_LIFE_TIME_FIELD "timeshift_chunk_life_time"

// catchup
#define CATCHUP_START_FIELD "start"
#define CATCHUP_STOP_FIELD "stop"

namespace fastocloud {
namespace server {
namespace mongo {

namespace details {
std::vector<common::uri::Url> MakeUrlsFromOutput(const std::vector<fastotv::OutputUri>& output);
}

struct UserStreamInfo {
  bool favorite = false;
  fastotv::timestamp_t recent = 0;
  fastotv::timestamp_t interruption_time = 0;
  bool priv = false;
};

bool MakeVodInfo(const bson_t* sdoc,
                 fastotv::StreamType st,
                 const UserStreamInfo& uinfo,
                 fastotv::commands_info::VodInfo* cinf);
bool MakeChannelInfo(const bson_t* sdoc,
                     fastotv::StreamType st,
                     const UserStreamInfo& uinfo,
                     fastotv::commands_info::ChannelInfo* cinf);
bool MakeCatchupInfo(const bson_t* sdoc,
                     fastotv::StreamType st,
                     const UserStreamInfo& uinfo,
                     fastotv::commands_info::CatchupInfo* cinf);
bool GetHttpRootFromStream(const bson_t* sdoc,
                           fastotv::StreamType st,
                           fastotv::channel_id_t cid,
                           common::file_system::ascii_directory_string_path* dir);
bool GetUrlFromStream(const bson_t* sdoc, fastotv::StreamType st, fastotv::channel_id_t cid, common::uri::Url* url);

bool GetOutputUrlData(bson_iter_t* iter, std::vector<fastotv::OutputUri>* urls);

}  // namespace mongo
}  // namespace server
}  // namespace fastocloud

namespace common {
std::string ConvertToString(const bson_oid_t* oid);
bool ConvertFromString(const std::string& soid, bson_oid_t* out);
}  // namespace common
