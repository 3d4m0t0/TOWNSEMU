#include "townsqt_miniaudio_devices.h"

#include <cstring>
#include <strings.h>

namespace TownsQtMiniaudioDevices
{
namespace
{
bool NamesEqual(const char *a,const char *b)
{
	if(nullptr==a)
	{
		a="";
	}
	if(nullptr==b)
	{
		b="";
	}
	return 0==std::strcmp(a,b);
}

const char *DeviceIdCString(ma_backend backend,const ma_device_id &id)
{
	switch(backend)
	{
	case ma_backend_pulseaudio:
		return id.pulse;
	case ma_backend_alsa:
		return id.alsa;
	case ma_backend_jack:
		return "";
	default:
		return "";
	}
}

bool DeviceIdMatches(ma_backend backend,const ma_device_info &info,const char *saved)
{
	const char *id_str=DeviceIdCString(backend,info.id);
	return id_str[0]!='\0' && NamesEqual(id_str,saved);
}

bool BackendIsAuto(const char *backend_name)
{
	return nullptr==backend_name || '\0'==backend_name[0] ||
	       0==strcasecmp(backend_name,"auto");
}

bool ParseBackend(const char *backend_name,ma_backend *out)
{
	if(nullptr==out || BackendIsAuto(backend_name))
	{
		return false;
	}
	if(0==strcasecmp(backend_name,"pulse") ||
	   0==strcasecmp(backend_name,"pulseaudio"))
	{
		*out=ma_backend_pulseaudio;
		return true;
	}
	if(0==strcasecmp(backend_name,"alsa"))
	{
		*out=ma_backend_alsa;
		return true;
	}
	if(0==strcasecmp(backend_name,"jack"))
	{
		*out=ma_backend_jack;
		return true;
	}
	return false;
}

bool InitContextImpl(const char *backend_name,ma_context *context)
{
	if(nullptr==context)
	{
		return false;
	}
	ma_backend single{};
	if(ParseBackend(backend_name,&single))
	{
		const ma_backend backends[]={single};
		return MA_SUCCESS==ma_context_init(backends,1,nullptr,context);
	}
	return MA_SUCCESS==ma_context_init(nullptr,0,nullptr,context);
}
}

bool IsAutoBackend(const char *backend_name)
{
	return BackendIsAuto(backend_name);
}

bool InitContext(const char *backend_name,ma_context *context)
{
	return InitContextImpl(backend_name,context);
}

std::vector<Entry> ListPlayback(const char *backend_name)
{
	std::vector<Entry> out;
	ma_context context{};
	if(true!=InitContextImpl(backend_name,&context))
	{
		return out;
	}

	ma_device_info *infos=nullptr;
	ma_uint32 count=0;
	if(MA_SUCCESS!=ma_context_get_devices(&context,&infos,&count,nullptr,nullptr))
	{
		ma_context_uninit(&context);
		return out;
	}

	const ma_backend backend=context.backend;
	out.reserve(count);
	for(ma_uint32 i=0; i<count; ++i)
	{
		if('\0'==infos[i].name[0])
		{
			continue;
		}
		Entry entry;
		entry.name=infos[i].name;
		const char *id_str=DeviceIdCString(backend,infos[i].id);
		if('\0'!=id_str[0])
		{
			entry.id=id_str;
		}
		out.push_back(std::move(entry));
	}

	ma_context_uninit(&context);
	return out;
}

bool ResolvePlaybackIdInContext(ma_context *context,const char *saved_name,ma_device_id *out_id)
{
	if(nullptr==context || nullptr==out_id || nullptr==saved_name || '\0'==saved_name[0])
	{
		return false;
	}

	ma_device_info *infos=nullptr;
	ma_uint32 count=0;
	if(MA_SUCCESS!=ma_context_get_devices(context,&infos,&count,nullptr,nullptr))
	{
		return false;
	}

	const ma_backend backend=context->backend;
	for(ma_uint32 i=0; i<count; ++i)
	{
		if(DeviceIdMatches(backend,infos[i],saved_name))
		{
			*out_id=infos[i].id;
			return true;
		}
	}

	ma_uint32 name_matches=0;
	ma_uint32 name_index=0;
	for(ma_uint32 i=0; i<count; ++i)
	{
		if(NamesEqual(infos[i].name,saved_name))
		{
			name_index=i;
			++name_matches;
		}
	}
	if(1==name_matches)
	{
		*out_id=infos[name_index].id;
		return true;
	}
	return false;
}
}
