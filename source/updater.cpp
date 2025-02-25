// Copyright (c) 2020 Michael Fabian Dirks <info@xaymar.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "updater.hpp"
#include "version.hpp"
#include "configuration.hpp"
#include "plugin.hpp"
#include "util/util-logging.hpp"

#include "warning-disable.hpp"
#include <fstream>
#include <mutex>
#include <regex>
#include <string_view>
#include "warning-enable.hpp"

#ifdef _DEBUG
#define ST_PREFIX "<%s> "
#define D_LOG_ERROR(x, ...) P_LOG_ERROR(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_WARNING(x, ...) P_LOG_WARN(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_INFO(x, ...) P_LOG_INFO(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_DEBUG(x, ...) P_LOG_DEBUG(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#else
#define ST_PREFIX "<updater> "
#define D_LOG_ERROR(...) P_LOG_ERROR(ST_PREFIX __VA_ARGS__)
#define D_LOG_WARNING(...) P_LOG_WARN(ST_PREFIX __VA_ARGS__)
#define D_LOG_INFO(...) P_LOG_INFO(ST_PREFIX __VA_ARGS__)
#define D_LOG_DEBUG(...) P_LOG_DEBUG(ST_PREFIX __VA_ARGS__)
#endif

// TODO:
// - Cache result in the configuration directory (not as a configuration value).
// - Move 'autoupdater.last_checked_at' to out of the configuration.
// - Figure out if nightly updates are viable at all.

#define ST_CFG_DATASHARING "updater.datasharing"
#define ST_CFG_AUTOMATION "updater.automation"
#define ST_CFG_CHANNEL "updater.channel"
#define ST_CFG_LASTCHECKEDAT "updater.lastcheckedat"

streamfx::version_stage streamfx::stage_from_string(std::string_view str)
{
	if (str == "a") {
		return version_stage::ALPHA;
	} else if (str == "b") {
		return version_stage::BETA;
	} else if (str == "c") {
		return version_stage::CANDIDATE;
	} else {
		return version_stage::STABLE;
	}
}

std::string_view streamfx::stage_to_string(version_stage t)
{
	switch (t) {
	case version_stage::ALPHA:
		return "a";
	case version_stage::BETA:
		return "b";
	case version_stage::CANDIDATE:
		return "c";
	default:
	case version_stage::STABLE:
		return ".";
	}
}

void streamfx::to_json(nlohmann::json& json, const version_stage& stage)
{
	json = stage_to_string(stage);
}

void streamfx::from_json(const nlohmann::json& json, version_stage& stage)
{
	stage = stage_from_string(json.get<std::string>());
}

streamfx::version_info::version_info()
	: major(0), minor(0), patch(0), tweak(0), stage(version_stage::STABLE), url(""), name("")
{}

streamfx::version_info::version_info(const std::string text) : version_info()
{
	// text can be:
	// 0.0.0 (Stable)
	// 0.0.0a0 (Testing)
	// 0.0.0b0 (Testing)
	// 0.0.0c0 (Testing)
	// 0.0.0_0 (Development)
	static const std::regex re_version(
		"([0-9]+)\\.([0-9]+)\\.([0-9]+)(([\\._abc]{1,1})([0-9]+|)|)(-g([0-9a-fA-F]{8,8})|)");
	std::smatch matches;
	if (std::regex_match(text, matches, re_version,
						 std::regex_constants::match_any | std::regex_constants::match_continuous)) {
		major = static_cast<uint16_t>(strtoul(matches[1].str().c_str(), nullptr, 10));
		minor = static_cast<uint16_t>(strtoul(matches[2].str().c_str(), nullptr, 10));
		patch = static_cast<uint16_t>(strtoul(matches[3].str().c_str(), nullptr, 10));
		if (matches.size() >= 5) {
			stage = stage_from_string(matches[5].str());
			tweak = static_cast<uint16_t>(strtoul(matches[6].str().c_str(), nullptr, 10));
		}
	} else {
		throw std::invalid_argument("Provided string is not a version.");
	}
}

void streamfx::to_json(nlohmann::json& json, const version_info& info)
{
	auto version     = nlohmann::json::object();
	version["major"] = info.major;
	version["minor"] = info.minor;
	version["patch"] = info.patch;
	version["type"]  = info.stage;
	version["tweak"] = info.tweak;
	json["version"]  = version;
	json["url"]      = info.url;
	json["name"]     = info.name;
}

void streamfx::from_json(const nlohmann::json& json, version_info& info)
{
	if (json.find("html_url") == json.end()) {
		auto version = json.at("version");
		info.major   = version.at("major").get<uint16_t>();
		info.minor   = version.at("minor").get<uint16_t>();
		info.patch   = version.at("patch").get<uint16_t>();
		info.stage   = version.at("type");
		info.tweak   = version.at("tweak").get<uint16_t>();
		info.url     = json.at("url").get<std::string>();
		info.name    = json.at("name").get<std::string>();
	} else {
		// This is a response from GitHub.

		// Retrieve entries from the release object.
		auto entry_tag_name = json.find("tag_name");
		auto entry_name     = json.find("name");
		auto entry_url      = json.find("html_url");
		if ((entry_tag_name == json.end()) || (entry_name == json.end()) || (entry_url == json.end())) {
			throw std::runtime_error("JSON is missing one or more required keys.");
		}

		// Parse the information.
		std::string tag_name = entry_tag_name->get<std::string>();
		info                 = {tag_name};
		info.url             = entry_url->get<std::string>();
		info.name            = entry_name->get<std::string>();
	}
}

bool streamfx::version_info::is_older_than(const version_info other)
{
	// 'true' if other is newer, otherwise false.

	// 1. Compare Major version:
	//     A. Ours is greater: Remote is older.
	//     B. Theirs is greater: Remote is newer.
	//     C. Continue the check.
	if (major > other.major)
		return false;
	if (major < other.major)
		return true;

	// 2. Compare Minor version:
	//     A. Ours is greater: Remote is older.
	//     B. Theirs is greater: Remote is newer.
	//     C. Continue the check.
	if (minor > other.minor)
		return false;
	if (minor < other.minor)
		return true;

	// 3. Compare Patch version:
	//     A. Ours is greater: Remote is older.
	//     B. Theirs is greater: Remote is newer.
	//     C. Continue the check.
	if (patch > other.patch)
		return false;
	if (patch < other.patch)
		return true;

	// 4. Compare Type:
	//     A. Outs is smaller: Remote is older.
	//     B. Theirs is smaller: Remote is newer.
	//     C. Continue the check.
	if (stage < other.stage)
		return false;
	if (stage > other.stage)
		return true;

	// 5. Compare Tweak:
	//    A. Ours is greater or equal: Remote is older or identical.
	//    B. Remote is newer
	if (tweak >= other.tweak)
		return false;

	return true;
}

streamfx::version_info::operator std::string()
{
	std::vector<char> buffer(25, 0);
	if (stage != version_stage::STABLE) {
		auto types = stage_to_string(stage);
		int  len   = snprintf(buffer.data(), buffer.size(), "%" PRIu16 ".%" PRIu16 ".%" PRIu16 "%.1s%" PRIu16, major,
							  minor, patch, types.data(), tweak);
		return std::string(buffer.data(), buffer.data() + len);
	} else {
		int len = snprintf(buffer.data(), buffer.size(), "%" PRIu16 ".%" PRIu16 ".%" PRIu16, major, minor, patch);
		return std::string(buffer.data(), buffer.data() + len);
	}
}

void streamfx::updater::task(streamfx::util::threadpool::task_data_t)
{
	try {
		auto query_fn = [](std::vector<char>& buffer) {
			static constexpr std::string_view ST_API_URL =
				"https://api.github.com/repos/Xaymar/obs-StreamFX/releases?per_page=25&page=1";

			streamfx::util::curl curl;
			size_t               buffer_offset = 0;

			// Set headers (User-Agent is needed so Github can contact us!).
			curl.set_header("User-Agent", "StreamFX Updater v" STREAMFX_VERSION_STRING);
			curl.set_header("Accept", "application/vnd.github.v3+json");

			// Set up request.
			curl.set_option(CURLOPT_HTTPGET, true); // GET
			curl.set_option(CURLOPT_POST, false);   // Not POST
			curl.set_option(CURLOPT_URL, ST_API_URL);
			curl.set_option(CURLOPT_TIMEOUT, 30); // 10s until we fail.

			// Callbacks
			curl.set_write_callback([&buffer, &buffer_offset](void* data, size_t s1, size_t s2) {
				size_t size = s1 * s2;
				if (buffer.size() < (size + buffer_offset))
					buffer.resize(buffer_offset + size);

				memcpy(buffer.data() + buffer_offset, data, size);
				buffer_offset += size;

				return s1 * s2;
			});
			//std::bind(&streamfx::updater::task_write_cb, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)

			// Clear any unknown data and reserve 64KiB of memory.
			buffer.clear();
			buffer.reserve(0xFFFF);

			// Finally, execute the request.
			D_LOG_DEBUG("Querying for latest releases...", "");
			if (CURLcode res = curl.perform(); res != CURLE_OK) {
				D_LOG_ERROR("Performing query failed with error: %s", curl_easy_strerror(res));
				throw std::runtime_error(curl_easy_strerror(res));
			}

			int32_t status_code = 0;
			if (CURLcode res = curl.get_info(CURLINFO_HTTP_CODE, status_code); res != CURLE_OK) {
				D_LOG_ERROR("Retrieving status code failed with error: %s", curl_easy_strerror(res));
				throw std::runtime_error(curl_easy_strerror(res));
			}
			D_LOG_DEBUG("API returned status code %d.", status_code);

			if (status_code != 200) {
				D_LOG_ERROR("API returned unexpected status code %d.", status_code);
				throw std::runtime_error("Request failed due to one or more reasons.");
			}
		};
		auto parse_fn = [this](nlohmann::json json) {
			// Check if it was parsed as an object.
			if (json.type() != nlohmann::json::value_t::array) {
				throw std::runtime_error("Invalid response from API.");
			}

			// Decide on the latest version for all update channels.
			std::lock_guard<decltype(_lock)> lock(_lock);
			_updates.clear();
			for (auto obj : json) {
				try {
					auto info = obj.get<streamfx::version_info>();

					switch (info.stage) {
					case version_stage::STABLE:
						if (get_update_info(version_stage::STABLE).is_older_than(info)) {
							_updates.emplace(version_stage::STABLE, info);
						}
						[[fallthrough]];
					case version_stage::CANDIDATE:
						if (get_update_info(version_stage::CANDIDATE).is_older_than(info)) {
							_updates.emplace(version_stage::CANDIDATE, info);
						}
						[[fallthrough]];
					case version_stage::BETA:
						if (get_update_info(version_stage::BETA).is_older_than(info)) {
							_updates.emplace(version_stage::BETA, info);
						}
						[[fallthrough]];
					case version_stage::ALPHA:
						if (get_update_info(version_stage::ALPHA).is_older_than(info)) {
							_updates.emplace(version_stage::ALPHA, info);
						}
					}

				} catch (const std::exception& ex) {
					D_LOG_DEBUG("Failed to parse entry, error: %s", ex.what());
				}
			}
		};

		{ // Query and parse the response.
			nlohmann::json json;

			// Query the API or parse a crafted response.
			auto debug_path = streamfx::config_file_path("github_release_query_response.json");
			if (std::filesystem::exists(debug_path)) {
				std::ifstream fs{debug_path};
				json = nlohmann::json::parse(fs);
				fs.close();
			} else {
				std::vector<char> buffer;
				query_fn(buffer);
				json = nlohmann::json::parse(buffer.begin(), buffer.end());
			}

			// Parse the JSON response from the API.
			parse_fn(json);
		}

		// Print all update information to the log file.
		D_LOG_INFO("Current Version: %s", static_cast<std::string>(_current_info).c_str());
		D_LOG_INFO("Latest Stable Version: %s",
				   static_cast<std::string>(get_update_info(version_stage::STABLE)).c_str());
		D_LOG_INFO("Latest Candidate Version: %s",
				   static_cast<std::string>(get_update_info(version_stage::CANDIDATE)).c_str());
		D_LOG_INFO("Latest Beta Version: %s", static_cast<std::string>(get_update_info(version_stage::BETA)).c_str());
		D_LOG_INFO("Latest Alpha Version: %s", static_cast<std::string>(get_update_info(version_stage::ALPHA)).c_str());
		if (is_update_available()) {
			D_LOG_INFO("Update is available.", "");
		}

		// Notify listeners of the update.
		events.refreshed.call(*this);
	} catch (const std::exception& ex) {
		// Notify about the error.
		std::string message = ex.what();
		events.error.call(*this, message);
	}
}

bool streamfx::updater::can_check()
{
#ifdef _DEBUG
	return true;
#else
	auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
	auto threshold = (_lastcheckedat + std::chrono::minutes(10));
	return (now > threshold);
#endif
}

void streamfx::updater::load()
{
	std::lock_guard<decltype(_lock)> lock(_lock);
	if (auto config = streamfx::configuration::instance(); config) {
		auto dataptr = config->get();

		if (obs_data_has_user_value(dataptr.get(), ST_CFG_DATASHARING))
			_data_sharing_allowed = obs_data_get_bool(dataptr.get(), ST_CFG_DATASHARING);
		if (obs_data_has_user_value(dataptr.get(), ST_CFG_AUTOMATION))
			_automation = obs_data_get_bool(dataptr.get(), ST_CFG_AUTOMATION);
		if (obs_data_has_user_value(dataptr.get(), ST_CFG_CHANNEL))
			_channel = static_cast<version_stage>(obs_data_get_int(dataptr.get(), ST_CFG_CHANNEL));
		if (obs_data_has_user_value(dataptr.get(), ST_CFG_LASTCHECKEDAT))
			_lastcheckedat = std::chrono::seconds(obs_data_get_int(dataptr.get(), ST_CFG_LASTCHECKEDAT));
	}
}

void streamfx::updater::save()
{
	if (auto config = streamfx::configuration::instance(); config) {
		auto dataptr = config->get();

		obs_data_set_bool(dataptr.get(), ST_CFG_DATASHARING, _data_sharing_allowed);
		obs_data_set_bool(dataptr.get(), ST_CFG_AUTOMATION, _automation);
		obs_data_set_int(dataptr.get(), ST_CFG_CHANNEL, static_cast<long long>(_channel));
		obs_data_set_int(dataptr.get(), ST_CFG_LASTCHECKEDAT, static_cast<long long>(_lastcheckedat.count()));

		config->save();
	}
}

streamfx::updater::updater()
	: _lock(), _task(),

	  _data_sharing_allowed(false), _automation(true), _channel(version_stage::STABLE), _lastcheckedat(),

	  _current_info(), _updates(), _dirty(false)
{
	// Load information from configuration.
	load();

	// Build current version information.
	try {
		_current_info = {STREAMFX_VERSION_STRING};
	} catch (...) {
		D_LOG_ERROR("Failed to parse current version information, results may be inaccurate.", "");
	}
}

streamfx::updater::~updater()
{
	save();
}

bool streamfx::updater::is_data_sharing_allowed()
{
	return _data_sharing_allowed;
}

void streamfx::updater::set_data_sharing_allowed(bool value)
{
	_dirty                = true;
	_data_sharing_allowed = value;
	events.gdpr_changed(*this, _data_sharing_allowed);

	{
		std::lock_guard<decltype(_lock)> lock(_lock);
		save();
	}

	D_LOG_INFO("User %s the processing of data.", _data_sharing_allowed ? "allowed" : "disallowed");
}

bool streamfx::updater::is_automated()
{
	return _automation;
}

void streamfx::updater::set_automation(bool value)
{
	_automation = value;
	events.automation_changed(*this, _automation);

	{
		std::lock_guard<decltype(_lock)> lock(_lock);
		save();
	}

	D_LOG_INFO("Automatic checks at launch are now %s.", value ? "enabled" : "disabled");
}

streamfx::version_stage streamfx::updater::get_channel()
{
	return _channel;
}

void streamfx::updater::set_channel(version_stage value)
{
	std::lock_guard<decltype(_lock)> lock(_lock);

	_dirty   = true;
	_channel = value;
	events.channel_changed(*this, _channel);

	save();

	D_LOG_INFO("Update channel changed to '%s'.", stage_to_string(value).data());
}

void streamfx::updater::refresh()
{
	if (!_task.expired() || !is_data_sharing_allowed()) {
		return;
	}

	if (can_check()) {
		std::lock_guard<decltype(_lock)> lock(_lock);

		// Update last checked time.
		_lastcheckedat =
			std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
		save();

		// Spawn a new task.
		_task = streamfx::threadpool()->push(std::bind(&streamfx::updater::task, this, std::placeholders::_1), nullptr);
	} else {
		events.refreshed(*this);
	}
}

bool streamfx::updater::is_update_available()
{
	return _current_info.is_older_than(get_update_info());
}

bool streamfx::updater::is_update_available(version_stage channel)
{
	return _current_info.is_older_than(get_update_info(channel));
}

streamfx::version_info streamfx::updater::get_current_info()
{
	return _current_info;
}

streamfx::version_info streamfx::updater::get_update_info()
{
	return get_update_info(_channel);
}

streamfx::version_info streamfx::updater::get_update_info(version_stage channel)
{
	std::lock_guard<decltype(_lock)> lock(_lock);
	if (auto iter = _updates.find(channel); iter != _updates.end()) {
		return iter->second;
	} else {
		return {};
	}
}

std::shared_ptr<streamfx::updater> streamfx::updater::instance()
{
	static std::weak_ptr<streamfx::updater> _instance;
	static std::mutex                       _lock;

	std::lock_guard<std::mutex> lock(_lock);
	if (_instance.expired()) {
		auto ptr  = std::make_shared<streamfx::updater>();
		_instance = ptr;
		return ptr;
	} else {
		return _instance.lock();
	}
}
