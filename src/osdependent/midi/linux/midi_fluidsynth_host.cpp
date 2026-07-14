#include "midi_fluidsynth_host.h"

#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <algorithm>

namespace
{
struct FluidApi
{
	void *lib=nullptr;

	using fluid_settings_ptr=void *;
	using fluid_synth_ptr=void *;
	using fluid_audio_driver_ptr=void *;

	fluid_settings_ptr (*new_fluid_settings)(void)=nullptr;
	void (*delete_fluid_settings)(fluid_settings_ptr settings)=nullptr;
	fluid_synth_ptr (*new_fluid_synth)(fluid_settings_ptr settings)=nullptr;
	void (*delete_fluid_synth)(fluid_synth_ptr synth)=nullptr;
	fluid_audio_driver_ptr (*new_fluid_audio_driver)(fluid_settings_ptr settings,fluid_synth_ptr synth)=nullptr;
	void (*delete_fluid_audio_driver)(fluid_audio_driver_ptr driver)=nullptr;
	int (*fluid_settings_setnum)(fluid_settings_ptr settings,const char *name,double val)=nullptr;
	int (*fluid_settings_setint)(fluid_settings_ptr settings,const char *name,int val)=nullptr;
	int (*fluid_settings_setstr)(fluid_settings_ptr settings,const char *name,const char *str)=nullptr;
	int (*fluid_synth_sfload)(fluid_synth_ptr synth,const char *filename,int reset_presets)=nullptr;
	int (*fluid_synth_noteon)(fluid_synth_ptr synth,int chan,int key,int vel)=nullptr;
	int (*fluid_synth_noteoff)(fluid_synth_ptr synth,int chan,int key)=nullptr;
	int (*fluid_synth_cc)(fluid_synth_ptr synth,int chan,int ctrl,int val)=nullptr;
	int (*fluid_synth_bank_select)(fluid_synth_ptr synth,int chan,int bank)=nullptr;
	int (*fluid_synth_program_change)(fluid_synth_ptr synth,int chan,int prognum)=nullptr;
	int (*fluid_synth_program_select)(fluid_synth_ptr synth,int chan,int sfont_id,int bank_num,int preset_num)=nullptr;
	int (*fluid_synth_pitch_bend)(fluid_synth_ptr synth,int chan,int val)=nullptr;
	int (*fluid_synth_channel_pressure)(fluid_synth_ptr synth,int chan,int val)=nullptr;
	int (*fluid_synth_sysex)(fluid_synth_ptr synth,const char *data,int len,char *response,int *response_len,int *handled,int dryrun)=nullptr;
	int (*fluid_synth_write_s16)(fluid_synth_ptr synth,int len,void *lout,int loff,int lincr,void *rout,int roff,int rincr)=nullptr;
	int (*fluid_synth_set_gain)(fluid_synth_ptr synth,float gain)=nullptr;
	int (*fluid_synth_set_channel_type)(fluid_synth_ptr synth,int chan,int type)=nullptr;
	int (*fluid_synth_system_reset)(fluid_synth_ptr synth)=nullptr;
	int (*fluid_synth_all_sounds_off)(fluid_synth_ptr synth,int chan)=nullptr;
	int (*fluid_synth_all_notes_off)(fluid_synth_ptr synth,int chan)=nullptr;
	int (*fluid_synth_set_reverb_full)(fluid_synth_ptr synth,int set,double roomsize,double damping,double width,double level)=nullptr;
	int (*fluid_synth_set_reverb)(fluid_synth_ptr synth,double roomsize,double damping,double width,double level)=nullptr;
	int (*fluid_synth_set_chorus_full)(fluid_synth_ptr synth,int set,int nr,double level,double speed,double depth_ms,int type)=nullptr;
	int (*fluid_synth_set_chorus)(fluid_synth_ptr synth,int nr,double level,double speed,double depth_ms,int type)=nullptr;
};

FluidApi g_api;
std::mutex g_mutex;
FluidApi::fluid_settings_ptr g_settings=nullptr;
FluidApi::fluid_synth_ptr g_synth=nullptr;
FluidApi::fluid_audio_driver_ptr g_audio_driver=nullptr;
bool g_active=false;
bool g_mix_in_app=false;
int g_sample_rate=44100;
int g_master_volume_percent=100;
double g_gs_master_gain=0.3; // GS Master Volume SysEx scaled to 0..0.3
std::string g_soundfont_path;
std::string g_loaded_soundfont;
int g_sfont_id=0;
int g_channel_program[16]{};
int g_channel_bank_msb[16]{};
bool g_channel_bank_explicit[16]{};
bool g_channel_is_drum[16]{};
std::vector<int16_t> g_mix_left;
std::vector<int16_t> g_mix_right;

constexpr double kSynthMaxGain=0.3;
constexpr int kAudioPeriodSize=512;
constexpr int kAudioPeriods=4;
constexpr int kSynthCpuCores=2;

template <typename T>
bool BindSymbol(FluidApi &api,const char *name,T &out)
{
	if(nullptr==api.lib)
	{
		return false;
	}
	void *sym=dlsym(api.lib,name);
	if(nullptr==sym)
	{
		return false;
	}
	out=reinterpret_cast<T>(sym);
	return true;
}

bool BindFluidApi(FluidApi &api)
{
	const bool required=
	    BindSymbol(api,"new_fluid_settings",api.new_fluid_settings) &&
	    BindSymbol(api,"delete_fluid_settings",api.delete_fluid_settings) &&
	    BindSymbol(api,"new_fluid_synth",api.new_fluid_synth) &&
	    BindSymbol(api,"delete_fluid_synth",api.delete_fluid_synth) &&
	    BindSymbol(api,"new_fluid_audio_driver",api.new_fluid_audio_driver) &&
	    BindSymbol(api,"delete_fluid_audio_driver",api.delete_fluid_audio_driver) &&
	    BindSymbol(api,"fluid_settings_setnum",api.fluid_settings_setnum) &&
	    BindSymbol(api,"fluid_settings_setint",api.fluid_settings_setint) &&
	    BindSymbol(api,"fluid_settings_setstr",api.fluid_settings_setstr) &&
	    BindSymbol(api,"fluid_synth_sfload",api.fluid_synth_sfload) &&
	    BindSymbol(api,"fluid_synth_noteon",api.fluid_synth_noteon) &&
	    BindSymbol(api,"fluid_synth_noteoff",api.fluid_synth_noteoff) &&
	    BindSymbol(api,"fluid_synth_cc",api.fluid_synth_cc) &&
	    BindSymbol(api,"fluid_synth_bank_select",api.fluid_synth_bank_select) &&
	    BindSymbol(api,"fluid_synth_program_change",api.fluid_synth_program_change) &&
	    BindSymbol(api,"fluid_synth_pitch_bend",api.fluid_synth_pitch_bend) &&
	    BindSymbol(api,"fluid_synth_channel_pressure",api.fluid_synth_channel_pressure) &&
	    BindSymbol(api,"fluid_synth_sysex",api.fluid_synth_sysex) &&
	    BindSymbol(api,"fluid_synth_write_s16",api.fluid_synth_write_s16) &&
	    BindSymbol(api,"fluid_synth_set_gain",api.fluid_synth_set_gain);
	if(false==required)
	{
		return false;
	}
	BindSymbol(api,"fluid_synth_set_channel_type",api.fluid_synth_set_channel_type);
	BindSymbol(api,"fluid_synth_system_reset",api.fluid_synth_system_reset);
	BindSymbol(api,"fluid_synth_all_sounds_off",api.fluid_synth_all_sounds_off);
	BindSymbol(api,"fluid_synth_all_notes_off",api.fluid_synth_all_notes_off);
	BindSymbol(api,"fluid_synth_program_select",api.fluid_synth_program_select);
	// Optional FX APIs (name differs across FluidSynth 1.x / 2.x).
	if(false==BindSymbol(api,"fluid_synth_set_reverb_full",api.fluid_synth_set_reverb_full))
	{
		BindSymbol(api,"fluid_synth_set_reverb",api.fluid_synth_set_reverb);
	}
	if(false==BindSymbol(api,"fluid_synth_set_chorus_full",api.fluid_synth_set_chorus_full))
	{
		BindSymbol(api,"fluid_synth_set_chorus",api.fluid_synth_set_chorus);
	}
	return true;
}

bool TryLoadFluidLibrary(FluidApi &api)
{
	static const char *const kCandidates[]={
	    "libfluidsynth.so.3",
	    "libfluidsynth.so",
	    "fluidsynth.so.3",
	    nullptr};

	for(int i=0; nullptr!=kCandidates[i]; ++i)
	{
		void *lib=dlopen(kCandidates[i],RTLD_NOW|RTLD_LOCAL);
		if(nullptr==lib)
		{
			continue;
		}
		FluidApi candidate{};
		candidate.lib=lib;
		if(true!=BindFluidApi(candidate))
		{
			dlclose(lib);
			continue;
		}
		api=candidate;
		return true;
	}
	return false;
}

bool FileExists(const char *path)
{
	if(nullptr==path || '\0'==path[0])
	{
		return false;
	}
	FILE *fp=fopen(path,"rb");
	if(nullptr==fp)
	{
		return false;
	}
	fclose(fp);
	return true;
}

void AppendCandidatePath(std::vector<std::string> &paths,const char *path)
{
	if(true!=FileExists(path))
	{
		return;
	}
	for(const auto &existing : paths)
	{
		if(existing==path)
		{
			return;
		}
	}
	paths.push_back(path);
}

std::vector<std::string> BuildSoundFontCandidates(void)
{
	std::vector<std::string> paths;
	if(false==g_soundfont_path.empty())
	{
		AppendCandidatePath(paths,g_soundfont_path.c_str());
	}
	if(const char *env=getenv("TOWNSQT_MIDI_SOUNDFONT"))
	{
		AppendCandidatePath(paths,env);
	}
	if(const char *env=getenv("FLUIDSYNTH_SOUNDFONT"))
	{
		AppendCandidatePath(paths,env);
	}
	AppendCandidatePath(paths,"/usr/share/soundfonts/FluidR3_GM.sf2");
	AppendCandidatePath(paths,"/usr/share/soundfonts/default.sf2");
	AppendCandidatePath(paths,"/usr/share/sounds/sf2/FluidR3_GM.sf2");
	AppendCandidatePath(paths,"/usr/share/sounds/sf2/default.sf2");
	const char *home=getenv("HOME");
	if(nullptr!=home)
	{
		std::string user_path=std::string(home)+"/.soundfonts/default.sf2";
		AppendCandidatePath(paths,user_path.c_str());
	}
	return paths;
}

void ApplySynthSettingsLocked(void)
{
	if(nullptr==g_settings)
	{
		return;
	}
	if(nullptr!=g_api.fluid_settings_setnum)
	{
		g_api.fluid_settings_setnum(g_settings,"synth.sample-rate",g_sample_rate);
		g_api.fluid_settings_setnum(g_settings,"synth.gain",g_gs_master_gain);
	}
	if(nullptr!=g_api.fluid_settings_setint)
	{
		// Roland device ID 16 (0x10): accept GS/XG SysEx from Towns games.
		g_api.fluid_settings_setint(g_settings,"synth.device-id",16);
		g_api.fluid_settings_setint(g_settings,"synth.polyphony",256);
		// SC-55 / MSGS: retrigger same key on drum channels after note-off.
		g_api.fluid_settings_setint(g_settings,"synth.note-cut",1);
		g_api.fluid_settings_setint(g_settings,"audio.period-size",kAudioPeriodSize);
		g_api.fluid_settings_setint(g_settings,"audio.periods",kAudioPeriods);
		g_api.fluid_settings_setint(g_settings,"synth.cpu-cores",kSynthCpuCores);
	}
	if(nullptr!=g_api.fluid_settings_setstr)
	{
		// Portamento time via CC#5 concave curve (XG/GS), not linear 14-bit.
		g_api.fluid_settings_setstr(g_settings,"synth.portamento-time","xg-gs");
	}
}

void ResetChannelRoutingStateLocked(void);
void InjectGsResetSysExLocked(void);
void OnGsResetDetectedLocked(void);
void ApplyMasterVolumeLocked(void);
void ApplyGsMasterVolumeSysExLocked(int val);
void ApplyGsReverbMacroLocked(int type);
void ApplyGsChorusMacroLocked(int type);
void HandleLongNewSongInitSysexLocked(const char *data,int len);
bool IsLongNewSongInitSysex(const char *data,int len);
bool IsStandaloneGsResetSysex(const char *data,int len);
bool SysexContainsGsResetLocked(const char *data,int len);
bool IsRolandDt1Sysex(const char *data,int len);
void ApplyChannelBankLocked(int chan,int bank_msb);
void ReapplyCachedGsBankBeforeProgramLocked(int chan);
void ApplyChannelProgramLocked(int chan,int prog);
void ApplySc55RhythmPartDrumKitLocked(int chan,int kit);
int GsPartBlockToMidiChannelIndex(unsigned char part_addr);
void ApplyRhythmPartModeLocked(int chan,int mode);
void ApplyCc0Locked(int chan,int value);
void DispatchControlChangeLocked(int chan,int cc,int value);
bool ShouldUseRhythmDrumKitSemanticsLocked(int chan);

bool ShouldUseRhythmDrumKitSemanticsLocked(int chan)
{
	// Part 10 is always rhythm on GS/SC-55. Other parts via USE RHYTHM SysEx.
	if(9==chan)
	{
		return true;
	}
	return g_channel_is_drum[chan];
}

void ApplyCc0Locked(int chan,int value)
{
	if(nullptr==g_synth || chan<0 || 15<chan)
	{
		return;
	}
	const int msb=value & 0x7f;
	// Fixed MAP / promoted rhythm: CC#0 = drum kit in bank 128 (no host bank cache).
	if(true==ShouldUseRhythmDrumKitSemanticsLocked(chan))
	{
		ApplySc55RhythmPartDrumKitLocked(chan,msb);
		return;
	}
	// Melodic: cache only. FluidSynth receives CC#0 immediately before the next PC.
	g_channel_bank_msb[chan]=msb;
	g_channel_bank_explicit[chan]=true;
}

void DispatchControlChangeLocked(int chan,int cc,int value)
{
	if(nullptr==g_synth || chan<0 || 15<chan)
	{
		return;
	}
	if(32==cc)
	{
		// GS: bank select LSB ignored.
		return;
	}
	// GS: CC126/127 (mono/poly) perform All Sound Off on SC-55.
	if(0x7e==cc || 0x7f==cc)
	{
		if(nullptr!=g_api.fluid_synth_all_notes_off)
		{
			g_api.fluid_synth_all_notes_off(g_synth,chan);
		}
		return;
	}
	if(0x7b==cc || 0x78==cc)
	{
		if(nullptr!=g_api.fluid_synth_all_notes_off)
		{
			g_api.fluid_synth_all_notes_off(g_synth,chan);
		}
		return;
	}
	if(0x79==cc)
	{
		g_channel_bank_msb[chan]=0;
		g_channel_bank_explicit[chan]=false;
		return;
	}
	if(0==cc)
	{
		ApplyCc0Locked(chan,value);
		return;
	}
	if(nullptr!=g_api.fluid_synth_cc)
	{
		g_api.fluid_synth_cc(g_synth,chan,cc,value);
	}
}

void ApplySc55RhythmPartDrumKitLocked(int chan,int kit)
{
	if(nullptr==g_synth || chan<0 || 15<chan)
	{
		return;
	}
	constexpr int kChannelTypeDrum=1;
	constexpr int kGsDrumBank=128;
	const int drum_kit=kit & 0x7f;
	g_channel_is_drum[chan]=true;
	g_channel_program[chan]=drum_kit;
	g_channel_bank_msb[chan]=0;
	g_channel_bank_explicit[chan]=false;
	if(nullptr!=g_api.fluid_synth_set_channel_type)
	{
		g_api.fluid_synth_set_channel_type(g_synth,chan,kChannelTypeDrum);
	}
	if(0<g_sfont_id && nullptr!=g_api.fluid_synth_program_select)
	{
		g_api.fluid_synth_program_select(
		    g_synth,
		    chan,
		    g_sfont_id,
		    kGsDrumBank,
		    drum_kit);
	}
	else
	{
		if(nullptr!=g_api.fluid_synth_bank_select)
		{
			g_api.fluid_synth_bank_select(g_synth,chan,kGsDrumBank);
		}
		if(nullptr!=g_api.fluid_synth_program_change)
		{
			g_api.fluid_synth_program_change(g_synth,chan,drum_kit);
		}
	}
}


int GsPartBlockToMidiChannelIndex(unsigned char part_addr)
{
	// Roland GS Part block: 40 1n ...
	// 10=Part10(ch10), 11-19=Part1-9(ch1-9), 1A-1F=Part11-16(ch11-16).
	if(0x10==part_addr)
	{
		return 9;
	}
	if(0x11<=part_addr && part_addr<=0x19)
	{
		return int(part_addr)-0x11;
	}
	if(0x1a<=part_addr && part_addr<=0x1f)
	{
		return 10+(int(part_addr)-0x1a);
	}
	return -1;
}


const unsigned char *SysexPayload(const char *data,int len,int *payload_len)
{
	if(nullptr==data || len<=0 || nullptr==payload_len)
	{
		return nullptr;
	}
	int begin=0;
	int end=len;
	if(0xf0==static_cast<unsigned char>(data[0]))
	{
		begin=1;
	}
	if(0<end && 0xf7==static_cast<unsigned char>(data[end-1]))
	{
		--end;
	}
	if(end<=begin)
	{
		return nullptr;
	}
	*payload_len=end-begin;
	return reinterpret_cast<const unsigned char *>(data+begin);
}

bool IsGmSystemOnSysex(const char *data,int len)
{
	int payload_len=0;
	const unsigned char *payload=SysexPayload(data,len,&payload_len);
	if(nullptr==payload || payload_len<4)
	{
		return false;
	}
	// Universal Non-Realtime: F0 7E <dev> 09 01 F7 (GM On) or ... 09 03 (GM2 On)
	if(0x7e!=payload[0] || 0x09!=payload[2])
	{
		return false;
	}
	return 0x01==payload[3] || 0x03==payload[3];
}

bool IsRolandGsResetSysex(const char *data,int len)
{
	int payload_len=0;
	const unsigned char *payload=SysexPayload(data,len,&payload_len);
	if(nullptr==payload || payload_len<9)
	{
		return false;
	}
	// Roland GS reset: F0 41 <dev> 42 12 40 00 7F 00 41 F7
	return 0x41==payload[0] &&
	       0x42==payload[2] &&
	       0x12==payload[3] &&
	       0x40==payload[4] &&
	       0x00==payload[5] &&
	       0x7f==payload[6];
}

bool IsStandaloneGsResetSysex(const char *data,int len)
{
	if(false==IsRolandGsResetSysex(data,len))
	{
		return false;
	}
	int payload_len=0;
	const unsigned char *payload=SysexPayload(data,len,&payload_len);
	// F0 41 <dev> 42 12 40 00 7F 00 <ck> F7 only (no trailing DT1 bulk).
	return nullptr!=payload && payload_len<=10;
}

bool IsLongNewSongInitSysex(const char *data,int len)
{
	if(nullptr==data || len<=0)
	{
		return false;
	}
	if(true==IsStandaloneGsResetSysex(data,len))
	{
		return false;
	}
	// GS reset + trailing DT1 part parameters (long new-song init burst).
	if(true==SysexContainsGsResetLocked(data,len))
	{
		return true;
	}
	// Multi-part DT1 init in one message (no embedded GS reset).
	return true==IsRolandDt1Sysex(data,len) && 24<=len;
}

bool IsRolandDt1Sysex(const char *data,int len)
{
	int payload_len=0;
	const unsigned char *payload=SysexPayload(data,len,&payload_len);
	if(nullptr==payload || payload_len<4)
	{
		return false;
	}
	// Roland DT1: F0 41 <dev> 42 12 <addr> <data> ... F7
	return 0x41==payload[0] &&
	       0x42==payload[2] &&
	       0x12==payload[3];
}





void ResetChannelRoutingStateLocked(void)
{
	std::fill(std::begin(g_channel_program),std::end(g_channel_program),0);
	std::fill(std::begin(g_channel_bank_msb),std::end(g_channel_bank_msb),0);
	std::fill(std::begin(g_channel_bank_explicit),std::end(g_channel_bank_explicit),false);
	std::fill(std::begin(g_channel_is_drum),std::end(g_channel_is_drum),false);
	g_channel_is_drum[9]=true;
}





void ApplyChannelBankLocked(int chan,int bank_msb)
{
	if(nullptr==g_synth || chan<0 || 15<chan)
	{
		return;
	}
	if(true==ShouldUseRhythmDrumKitSemanticsLocked(chan))
	{
		ApplySc55RhythmPartDrumKitLocked(chan,bank_msb);
		return;
	}
	g_channel_bank_msb[chan]=bank_msb;
	g_channel_bank_explicit[chan]=true;
	constexpr int kChannelTypeMelodic=0;
	g_channel_is_drum[chan]=false;
	if(nullptr!=g_api.fluid_synth_set_channel_type)
	{
		g_api.fluid_synth_set_channel_type(g_synth,chan,kChannelTypeMelodic);
	}
	ApplyChannelProgramLocked(chan,g_channel_program[chan]);
}


void ReapplyCachedGsBankBeforeProgramLocked(int chan)
{
	if(nullptr==g_synth)
	{
		return;
	}
	if(nullptr!=g_api.fluid_synth_cc)
	{
		g_api.fluid_synth_cc(g_synth,chan,0,g_channel_bank_msb[chan] & 0x7f);
	}
	else if(nullptr!=g_api.fluid_synth_bank_select)
	{
		g_api.fluid_synth_bank_select(g_synth,chan,g_channel_bank_msb[chan] & 0x7f);
	}
}

void ApplyChannelProgramLocked(int chan,int prog)
{
	if(nullptr==g_synth || chan<0 || 15<chan)
	{
		return;
	}
	const int program=prog & 0x7f;
	// Rhythm part: PC selects drum kit in bank 128 (same as CC#0 kit select).
	if(true==ShouldUseRhythmDrumKitSemanticsLocked(chan))
	{
		ApplySc55RhythmPartDrumKitLocked(chan,program);
		return;
	}

	g_channel_program[chan]=program;
	g_channel_is_drum[chan]=false;
	constexpr int kChannelTypeMelodic=0;
	if(nullptr!=g_api.fluid_synth_set_channel_type)
	{
		g_api.fluid_synth_set_channel_type(g_synth,chan,kChannelTypeMelodic);
	}
	ReapplyCachedGsBankBeforeProgramLocked(chan);
	if(nullptr!=g_api.fluid_synth_program_change)
	{
		g_api.fluid_synth_program_change(g_synth,chan,program);
	}
	g_channel_bank_explicit[chan]=false;
}

void ApplyRhythmPartModeLocked(int chan,int mode)
{
	if(chan<0 || 15<chan)
	{
		return;
	}
	// GS 40 1x 15: 00=OFF, 01=MAP1, 02=MAP2. Ignore other values (scan false positives).
	if(1==mode || 2==mode)
	{
		int kit=g_channel_program[chan];
		if(true==g_channel_bank_explicit[chan])
		{
			kit=g_channel_bank_msb[chan];
			g_channel_bank_explicit[chan]=false;
			g_channel_bank_msb[chan]=0;
		}
		ApplySc55RhythmPartDrumKitLocked(chan,kit);
		return;
	}
	if(0!=mode)
	{
		return;
	}
	// Part 10 (MIDI ch10): GS keeps rhythm; ignore USE RHYTHM=OFF.
	if(9==chan)
	{
		return;
	}
	g_channel_is_drum[chan]=false;
	constexpr int kChannelTypeMelodic=0;
	if(nullptr!=g_api.fluid_synth_set_channel_type)
	{
		g_api.fluid_synth_set_channel_type(g_synth,chan,kChannelTypeMelodic);
	}
	if(0<g_sfont_id && nullptr!=g_api.fluid_synth_program_select)
	{
		g_api.fluid_synth_program_select(
		    g_synth,
		    chan,
		    g_sfont_id,
		    0,
		    g_channel_program[chan] & 0x7f);
	}
}

void HandleRolandDt1AddressByteLocked(
    unsigned char ah,
    unsigned char am,
    unsigned char al,
    unsigned char value)
{
	if(0x40!=ah)
	{
		return;
	}
	if(0x00==am && 0x04==al)
	{
		ApplyGsMasterVolumeSysExLocked(value);
		return;
	}
	if(0x00==am && 0x7f==al)
	{
		return;
	}
	if(0x01==am && 0x30==al && value<=7)
	{
		ApplyGsReverbMacroLocked(value);
		return;
	}
	if(0x01==am && 0x38==al && value<=7)
	{
		ApplyGsChorusMacroLocked(value);
		return;
	}
	if(0x10!=(am & 0xf0))
	{
		return;
	}
	const int chan=GsPartBlockToMidiChannelIndex(am);
	if(chan<0)
	{
		return;
	}
	if(0x15==al)
	{
		if(value<=2)
		{
			ApplyRhythmPartModeLocked(chan,value);
		}
		return;
	}
	if(0x00==al)
	{
		ApplyChannelBankLocked(chan,value);
	}
	else if(0x01==al)
	{
		ApplyChannelProgramLocked(chan,value);
	}
}

void WalkRolandDt1PayloadLocked(const unsigned char *payload,int payload_len)
{
	// DT1: 41 <dev> 42 12 <ah am al> <data...> <sum>
	// Multi-byte data writes consecutive addresses (al++), so USE RHYTHM at
	// 40 1x 15 may appear only as offset 0x15 from start 40 1x 00 — not as
	// the byte pattern 40 1x 15. Byte-scan misses those; address walk does not.
	if(nullptr==payload || payload_len<8)
	{
		return;
	}
	for(int cmd=0; cmd+7<payload_len; ++cmd)
	{
		if(0x41!=payload[cmd] || 0x42!=payload[cmd+2] || 0x12!=payload[cmd+3])
		{
			continue;
		}
		unsigned char ah=payload[cmd+4];
		unsigned char am=payload[cmd+5];
		unsigned char al=payload[cmd+6];
		const int data_begin=cmd+7;
		const int data_end=payload_len-1; // exclude trailing checksum of this frame approx
		if(data_end<=data_begin)
		{
			continue;
		}
		// Stop before next DT1 header if present inside the same SysEx.
		int end=data_end;
		for(int j=data_begin+1; j+3<data_end; ++j)
		{
			if(0x41==payload[j] && 0x42==payload[j+2] && 0x12==payload[j+3])
			{
				end=j-1; // previous byte was prior checksum
				if(end<data_begin)
				{
					end=data_begin;
				}
				break;
			}
		}
		for(int i=data_begin; i<end; ++i)
		{
			HandleRolandDt1AddressByteLocked(ah,am,al,payload[i]);
			++al;
			if(0x80<=al)
			{
				al=0;
				++am;
				if(0x80<=am)
				{
					am=0;
					++ah;
				}
			}
		}
		cmd=end; // loop ++ continues after this frame
	}
}

void ApplyRolandDt1SysexLocked(const char *data,int len)
{
	int payload_len=0;
	const unsigned char *payload=SysexPayload(data,len,&payload_len);
	if(nullptr==payload)
	{
		return;
	}
	WalkRolandDt1PayloadLocked(payload,payload_len);
}


void OnGsResetDetectedLocked(void)
{
	if(nullptr==g_synth)
	{
		return;
	}
	if(nullptr!=g_api.fluid_synth_all_sounds_off)
	{
		if(0!=g_api.fluid_synth_all_sounds_off(g_synth,-1))
		{
			for(int ch=0; ch<16; ++ch)
			{
				g_api.fluid_synth_all_sounds_off(g_synth,ch);
			}
		}
	}
	if(nullptr!=g_api.fluid_synth_system_reset)
	{
		g_api.fluid_synth_system_reset(g_synth);
	}
	InjectGsResetSysExLocked();
	constexpr int kReverbSetAll=0x0f;
	if(nullptr!=g_api.fluid_synth_set_reverb_full)
	{
		g_api.fluid_synth_set_reverb_full(g_synth,kReverbSetAll,0.2,0.0,0.5,0.9);
	}
	else if(nullptr!=g_api.fluid_synth_set_reverb)
	{
		g_api.fluid_synth_set_reverb(g_synth,0.2,0.0,0.5,0.9);
	}
	constexpr int kChorusSetAll=0x1f;
	constexpr int kFluidChorusModSin=0;
	if(nullptr!=g_api.fluid_synth_set_chorus_full)
	{
		g_api.fluid_synth_set_chorus_full(
		    g_synth,kChorusSetAll,3,2.0,0.3,8.0,kFluidChorusModSin);
	}
	else if(nullptr!=g_api.fluid_synth_set_chorus)
	{
		g_api.fluid_synth_set_chorus(g_synth,3,2.0,0.3,8.0,kFluidChorusModSin);
	}
	for(int chan=0; chan<16; ++chan)
	{
		if(nullptr!=g_api.fluid_synth_pitch_bend)
		{
			g_api.fluid_synth_pitch_bend(g_synth,chan,8192);
		}
		if(nullptr!=g_api.fluid_synth_cc)
		{
			g_api.fluid_synth_cc(g_synth,chan,1,0);
			g_api.fluid_synth_cc(g_synth,chan,11,127);
			g_api.fluid_synth_cc(g_synth,chan,32,0);
		}
	}
	if(0<g_sfont_id && nullptr!=g_api.fluid_synth_program_select)
	{
		constexpr int kChannelTypeMelodic=0;
		constexpr int kChannelTypeDrum=1;
		g_api.fluid_synth_program_select(g_synth,9,g_sfont_id,128,0);
		if(nullptr!=g_api.fluid_synth_set_channel_type)
		{
			g_api.fluid_synth_set_channel_type(g_synth,9,kChannelTypeDrum);
		}
		for(int chan=0; chan<16; ++chan)
		{
			if(9==chan)
			{
				continue;
			}
			g_api.fluid_synth_program_select(g_synth,chan,g_sfont_id,0,0);
			if(nullptr!=g_api.fluid_synth_set_channel_type)
			{
				g_api.fluid_synth_set_channel_type(g_synth,chan,kChannelTypeMelodic);
			}
		}
	}
	ResetChannelRoutingStateLocked();
}


void ApplyGsMasterVolumeSysExLocked(int val)
{
	const int v=std::clamp(val,0,127);
	g_gs_master_gain=(static_cast<double>(v)/127.0)*kSynthMaxGain;
	if(nullptr!=g_settings && nullptr!=g_api.fluid_settings_setnum)
	{
		g_api.fluid_settings_setnum(g_settings,"synth.gain",g_gs_master_gain);
	}
	ApplyMasterVolumeLocked();
}

void ApplyGsReverbMacroLocked(int type)
{
	if(nullptr==g_synth || type<0 || 7<type)
	{
		return;
	}
	// Approximate SC-55 Reverb Macro 0..7 → FluidSynth freeverb params.
	static const struct
	{
		double room;
		double damp;
		double width;
		double level;
	} kPreset[8]={
	    {0.20,0.00,0.5,0.90}, // 0 Room1
	    {0.35,0.10,0.5,0.85}, // 1 Room2
	    {0.50,0.20,0.6,0.80}, // 2 Room3
	    {0.65,0.30,0.7,0.75}, // 3 Hall1
	    {0.80,0.40,0.8,0.70}, // 4 Hall2
	    {0.30,0.00,0.5,0.90}, // 5 Plate
	    {0.55,0.00,1.0,0.55}, // 6 Delay
	    {0.55,0.00,100.0,0.55}, // 7 Panning Delay
	};
	const auto &p=kPreset[type];
	constexpr int kSetAll=0x0f;
	if(nullptr!=g_api.fluid_synth_set_reverb_full)
	{
		g_api.fluid_synth_set_reverb_full(g_synth,kSetAll,p.room,p.damp,p.width,p.level);
	}
	else if(nullptr!=g_api.fluid_synth_set_reverb)
	{
		g_api.fluid_synth_set_reverb(g_synth,p.room,p.damp,p.width,p.level);
	}
	else if(nullptr!=g_api.fluid_settings_setnum && nullptr!=g_settings)
	{
		g_api.fluid_settings_setnum(g_settings,"synth.reverb.room-size",p.room);
		g_api.fluid_settings_setnum(g_settings,"synth.reverb.damp",p.damp);
		g_api.fluid_settings_setnum(g_settings,"synth.reverb.width",p.width);
		g_api.fluid_settings_setnum(g_settings,"synth.reverb.level",p.level);
	}
}

void ApplyGsChorusMacroLocked(int type)
{
	if(nullptr==g_synth || type<0 || 7<type)
	{
		return;
	}
	// Approximate SC-55 Chorus Macro 0..7.
	static const struct
	{
		int nr;
		double level;
		double speed;
		double depth_ms;
		int wave; // 0=sine, 1=triangle
	} kPreset[8]={
	    {3,0.40,0.30,5.0,0},  // 0 Chorus1
	    {3,0.50,0.35,6.0,0},  // 1 Chorus2
	    {4,0.55,0.40,7.0,0},  // 2 Chorus3
	    {4,0.60,0.45,8.0,0},  // 3 Chorus4
	    {4,0.65,0.40,9.0,1},  // 4 Feedback Chorus
	    {2,0.70,0.50,5.0,1},  // 5 Flanger
	    {2,0.35,0.25,3.0,0},  // 6 Short Delay
	    {2,0.45,0.30,4.0,1},  // 7 Short Delay FB
	};
	const auto &p=kPreset[type];
	constexpr int kSetAll=0x1f;
	if(nullptr!=g_api.fluid_synth_set_chorus_full)
	{
		g_api.fluid_synth_set_chorus_full(
		    g_synth,kSetAll,p.nr,p.level,p.speed,p.depth_ms,p.wave);
	}
	else if(nullptr!=g_api.fluid_synth_set_chorus)
	{
		g_api.fluid_synth_set_chorus(
		    g_synth,p.nr,p.level,p.speed,p.depth_ms,p.wave);
	}
	else if(nullptr!=g_api.fluid_settings_setnum && nullptr!=g_settings)
	{
		g_api.fluid_settings_setnum(g_settings,"synth.chorus.level",p.level);
		g_api.fluid_settings_setnum(g_settings,"synth.chorus.speed",p.speed);
		g_api.fluid_settings_setnum(g_settings,"synth.chorus.depth",p.depth_ms);
		if(nullptr!=g_api.fluid_settings_setint)
		{
			g_api.fluid_settings_setint(g_settings,"synth.chorus.nr",p.nr);
		}
	}
}


void HandleLongNewSongInitSysexLocked(const char *data,int len)
{
	if(nullptr==g_synth || nullptr==data || len<=0)
	{
		return;
	}
	if(true==SysexContainsGsResetLocked(data,len))
	{
		OnGsResetDetectedLocked();
		if(true==IsRolandDt1Sysex(data,len))
		{
			ApplyRolandDt1SysexLocked(data,len);
		}
		return;
	}
	if(nullptr==g_api.fluid_synth_sysex)
	{
		return;
	}
	int handled=0;
	int response_len=0;
	char response[16]{};
	g_api.fluid_synth_sysex(
	    g_synth,
	    data,
	    len,
	    response,
	    &response_len,
	    &handled,
	    0);
	// Hardware: DT1 overwrites only written addresses; do not wipe host routing.
	if(true==IsRolandDt1Sysex(data,len))
	{
		ApplyRolandDt1SysexLocked(data,len);
	}
}


bool SysexContainsGsResetLocked(const char *data,int len)
{
	if(true==IsRolandGsResetSysex(data,len))
	{
		return true;
	}
	int payload_len=0;
	const unsigned char *payload=SysexPayload(data,len,&payload_len);
	if(nullptr==payload)
	{
		return false;
	}
	for(int i=0; i+4<payload_len; ++i)
	{
		if(0x40==payload[i] &&
		   0x00==payload[i+1] &&
		   0x7f==payload[i+2] &&
		   0x00==payload[i+3] &&
		   0x41==payload[i+4])
		{
			return true;
		}
	}
	return false;
}


void DispatchSysExLocked(const char *data,int len)
{
	if(nullptr==g_synth || nullptr==data || len<=0)
	{
		return;
	}
	if(true==IsGmSystemOnSysex(data,len))
	{
		OnGsResetDetectedLocked();
		return;
	}
	if(true==IsStandaloneGsResetSysex(data,len))
	{
		OnGsResetDetectedLocked();
		return;
	}
	if(true==IsLongNewSongInitSysex(data,len))
	{
		HandleLongNewSongInitSysexLocked(data,len);
		return;
	}
	if(nullptr==g_api.fluid_synth_sysex)
	{
		return;
	}
	int handled=0;
	int response_len=0;
	char response[16]{};
	g_api.fluid_synth_sysex(
	    g_synth,
	    data,
	    len,
	    response,
	    &response_len,
	    &handled,
	    0);
	if(true==IsRolandDt1Sysex(data,len))
	{
		ApplyRolandDt1SysexLocked(data,len);
	}
}


void InjectGsResetSysExLocked(void)
{
	if(nullptr==g_synth || nullptr==g_api.fluid_synth_sysex)
	{
		return;
	}
	// Roland GS reset: F0 41 10 42 12 40 00 7F 00 41 F7
	static const char kGsReset[]={
	    static_cast<char>(0xf0),
	    0x41,0x10,0x42,0x12,0x40,0x00,0x7f,0x00,0x41,
	    static_cast<char>(0xf7)};
	int handled=0;
	int response_len=0;
	char response[16]{};
	g_api.fluid_synth_sysex(
	    g_synth,
	    kGsReset,
	    static_cast<int>(sizeof(kGsReset)),
	    response,
	    &response_len,
	    &handled,
	    0);
}


int ShortMidiMessageLength(unsigned char status)
{
	const unsigned char type=status & 0xf0;
	switch(type)
	{
	case 0xc0:
	case 0xd0:
		return 2;
	case 0xf0:
		switch(status)
		{
		case 0xf1:
		case 0xf3:
			return 2;
		case 0xf2:
			return 3;
		default:
			return 1;
		}
	default:
		if(0xf0<=status)
		{
			return 1;
		}
		return 3;
	}
}

bool LoadSoundFontLocked(void)
{
	if(nullptr==g_synth || nullptr==g_api.fluid_synth_sfload)
	{
		return false;
	}
	for(const auto &path : BuildSoundFontCandidates())
	{
		const int sfont_id=g_api.fluid_synth_sfload(g_synth,path.c_str(),1);
		if(0<=sfont_id)
		{
			g_sfont_id=sfont_id;
			g_loaded_soundfont=path;
			OnGsResetDetectedLocked();
			const char *base=path.c_str();
			for(const char *p=base; '\0'!=*p; ++p)
			{
				if('/'==*p || '\\'==*p)
				{
					base=p+1;
				}
			}
			std::fprintf(stderr,"TownsEMU MIDI: loaded SoundFont %s\n",base);
			return true;
		}
	}
	std::fprintf(stderr,
	    "TownsEMU MIDI: FluidSynth is active but no SoundFont was found. "
	    "Set TOWNSQT_MIDI_SOUNDFONT or install a .sf2 file.\n");
	return false;
}

void StartStandaloneAudioDriverLocked(void)
{
	if(true==g_mix_in_app || nullptr!=g_audio_driver || nullptr==g_synth || nullptr==g_settings)
	{
		return;
	}
	if(nullptr==g_api.new_fluid_audio_driver)
	{
		return;
	}
	g_api.fluid_settings_setstr(g_settings,"audio.driver","alsa");
	g_audio_driver=g_api.new_fluid_audio_driver(g_settings,g_synth);
	if(nullptr==g_audio_driver)
	{
		std::fprintf(stderr,"TownsEMU MIDI: FluidSynth ALSA audio driver failed to start.\n");
	}
}

void ShutdownLocked(void)
{
	if(nullptr!=g_audio_driver && nullptr!=g_api.delete_fluid_audio_driver)
	{
		g_api.delete_fluid_audio_driver(g_audio_driver);
		g_audio_driver=nullptr;
	}
	if(nullptr!=g_synth && nullptr!=g_api.delete_fluid_synth)
	{
		g_api.delete_fluid_synth(g_synth);
		g_synth=nullptr;
	}
	if(nullptr!=g_settings && nullptr!=g_api.delete_fluid_settings)
	{
		g_api.delete_fluid_settings(g_settings);
		g_settings=nullptr;
	}
	if(nullptr!=g_api.lib)
	{
		dlclose(g_api.lib);
	}
	g_api={};
	g_active=false;
	g_sfont_id=0;
	g_loaded_soundfont.clear();
}

void DispatchShortMessageLocked(const unsigned char cmdBuf[3])
{
	if(nullptr==g_synth)
	{
		return;
	}
	const unsigned char status=cmdBuf[0];
	if(0==(status & 0x80))
	{
		return;
	}
	if(0xff==status)
	{
		OnGsResetDetectedLocked();
		return;
	}
	const int msg_len=ShortMidiMessageLength(status);
	const unsigned char type=status & 0xf0;
	const int chan=status & 0x0f;
	switch(type)
	{
	case 0x80:
		if(2<=msg_len)
		{
			g_api.fluid_synth_noteoff(g_synth,chan,cmdBuf[1]);
		}
		break;
	case 0x90:
		if(3<=msg_len)
		{
			if(0==cmdBuf[2])
			{
				g_api.fluid_synth_noteoff(g_synth,chan,cmdBuf[1]);
			}
			else
			{
				g_api.fluid_synth_noteon(g_synth,chan,cmdBuf[1],cmdBuf[2]);
			}
		}
		break;
	case 0xa0:
		break;
	case 0xb0:
		if(3<=msg_len)
		{
			DispatchControlChangeLocked(chan,cmdBuf[1],cmdBuf[2]);
		}
		break;
	case 0xc0:
		if(2<=msg_len)
		{
			ApplyChannelProgramLocked(chan,cmdBuf[1]);
		}
		break;
	case 0xd0:
		if(2<=msg_len)
		{
			g_api.fluid_synth_channel_pressure(g_synth,chan,cmdBuf[1]);
		}
		break;
	case 0xe0:
		if(3<=msg_len)
		{
			const int val=(int(cmdBuf[2])<<7)|int(cmdBuf[1]);
			g_api.fluid_synth_pitch_bend(g_synth,chan,val);
		}
		break;
	default:
		break;
	}
}

void ApplyMasterVolumeLocked(void)
{
	if(nullptr==g_synth || nullptr==g_api.fluid_synth_set_gain)
	{
		return;
	}
	// UI slider × GS Master Volume SysEx (val/127 × 0.3).
	const float gain=std::max(
	    0.0f,
	    static_cast<float>(g_master_volume_percent)/100.0f*
	        static_cast<float>(g_gs_master_gain));
	g_api.fluid_synth_set_gain(g_synth,gain);
}
}

bool MidiFluidSynthHost::TryInitialize(void)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if(true==g_active)
	{
		return true;
	}
	if(false==TryLoadFluidLibrary(g_api))
	{
		return false;
	}
	g_settings=g_api.new_fluid_settings();
	ApplySynthSettingsLocked();
	g_synth=g_api.new_fluid_synth(g_settings);
	if(nullptr==g_settings || nullptr==g_synth)
	{
		ShutdownLocked();
		return false;
	}
	LoadSoundFontLocked();
	if(true!=g_mix_in_app)
	{
		StartStandaloneAudioDriverLocked();
	}
	ApplyMasterVolumeLocked();
	g_active=true;
	std::fprintf(stderr,
	    "TownsEMU MIDI: using FluidSynth (%s).\n",
	    BackendName());
	return true;
}

bool MidiFluidSynthHost::IsLibraryAvailable(void)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if(true==g_active)
	{
		return true;
	}
	FluidApi probe{};
	if(true!=TryLoadFluidLibrary(probe))
	{
		return false;
	}
	if(nullptr!=probe.lib)
	{
		dlclose(probe.lib);
	}
	return true;
}

bool MidiFluidSynthHost::IsActive(void)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_active;
}

const char *MidiFluidSynthHost::BackendName(void)
{
	return "FluidSynth";
}

void MidiFluidSynthHost::SetMixInApplicationAudio(bool enabled)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	g_mix_in_app=enabled;
	if(true==g_active && true==enabled && nullptr!=g_audio_driver)
	{
		g_api.delete_fluid_audio_driver(g_audio_driver);
		g_audio_driver=nullptr;
	}
}

void MidiFluidSynthHost::SetSampleRate(int sample_rate_hz)
{
	if(sample_rate_hz<=0)
	{
		return;
	}
	std::lock_guard<std::mutex> lock(g_mutex);
	g_sample_rate=sample_rate_hz;
	if(true==g_active)
	{
		ApplySynthSettingsLocked();
	}
	else if(nullptr!=g_settings)
	{
		ApplySynthSettingsLocked();
	}
}

void MidiFluidSynthHost::SetSoundFontPath(const char *path)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	g_soundfont_path=(nullptr!=path) ? path : "";
	if(true==g_active)
	{
		LoadSoundFontLocked();
	}
}

void MidiFluidSynthHost::SetMasterVolumePercent(int percent)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	g_master_volume_percent=std::clamp(percent,0,100);
	if(true==g_active)
	{
		ApplyMasterVolumeLocked();
	}
}

void MidiFluidSynthHost::SendShortMessage(const unsigned char cmdBuf[3])
{
	if(nullptr==cmdBuf)
	{
		return;
	}
	std::lock_guard<std::mutex> lock(g_mutex);
	if(false==g_active)
	{
		return;
	}
	DispatchShortMessageLocked(cmdBuf);
}

void MidiFluidSynthHost::SendExclusiveMessage(const unsigned char *data,int len)
{
	if(nullptr==data || len<=0 || false==g_active)
	{
		return;
	}
	std::vector<char> sysex;
	sysex.reserve(static_cast<size_t>(len)+2);
	sysex.push_back(static_cast<char>(0xf0));
	sysex.insert(sysex.end(),data,data+len);
	sysex.push_back(static_cast<char>(0xf7));

	std::lock_guard<std::mutex> lock(g_mutex);
	if(nullptr==g_synth)
	{
		return;
	}
	DispatchSysExLocked(sysex.data(),static_cast<int>(sysex.size()));
}

void MidiFluidSynthHost::ResetPlaybackState(void)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if(false==g_active || nullptr==g_synth)
	{
		return;
	}
	OnGsResetDetectedLocked();
}

void MidiFluidSynthHost::MixInterleavedS16(int16_t *stream,int frame_count)
{
	if(nullptr==stream || frame_count<=0)
	{
		return;
	}
	std::lock_guard<std::mutex> lock(g_mutex);
	if(false==g_active || false==g_mix_in_app || nullptr==g_synth)
	{
		return;
	}
	if(static_cast<int>(g_mix_left.size())<frame_count)
	{
		g_mix_left.resize(static_cast<size_t>(frame_count));
		g_mix_right.resize(static_cast<size_t>(frame_count));
	}
	if(0!=g_api.fluid_synth_write_s16(
	       g_synth,
	       frame_count,
	       g_mix_left.data(),0,1,
	       g_mix_right.data(),0,1))
	{
		return;
	}
	for(int i=0; i<frame_count; ++i)
	{
		const int mix_l=int(g_mix_left[static_cast<size_t>(i)]);
		const int mix_r=int(g_mix_right[static_cast<size_t>(i)]);
		const int l=int(stream[i*2])+mix_l;
		const int r=int(stream[i*2+1])+mix_r;
		if(l<-32768)
		{
			stream[i*2]=-32768;
		}
		else if(32767<l)
		{
			stream[i*2]=32767;
		}
		else
		{
			stream[i*2]=static_cast<int16_t>(l);
		}
		if(r<-32768)
		{
			stream[i*2+1]=-32768;
		}
		else if(32767<r)
		{
			stream[i*2+1]=32767;
		}
		else
		{
			stream[i*2+1]=static_cast<int16_t>(r);
		}
	}
}
