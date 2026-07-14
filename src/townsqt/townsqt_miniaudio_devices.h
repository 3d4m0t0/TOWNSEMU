#pragma once

#include "miniaudio.h"

#include <string>
#include <vector>

namespace TownsQtMiniaudioDevices
{
struct Entry
{
	std::string name;
	std::string id;
};

bool IsAutoBackend(const char *backend_name);
bool InitContext(const char *backend_name,ma_context *context);
std::vector<Entry> ListPlayback(const char *backend_name);
bool ResolvePlaybackIdInContext(ma_context *context,const char *saved_name,ma_device_id *out_id);
}
