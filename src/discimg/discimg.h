/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */
#ifndef DISCIMG_IS_INCLUDED
#define DISCIMG_IS_INCLUDED
/* { */

#include <vector>
#include <string>
#include <cstdint>
#include <mutex>
#include <fstream>

class DiscImageChdBackend;

// MDS/MDF implementation is based on:
//   https://problemkaputt.de/psx-spx.htm#cdromdiskimagesmdsmdfalcohol120

/*! Disc identity for diagnostics and mouse-coord profile lookup.
    Profile files are always keyed by fingerprintHash (fp_%08x.ini):
      FNV-1a of audio/data track counts + hashed ISO9660 root listing
      (stable across CUE/CHD authoring; does not use TOC MSF or raw filenames).
    contentKey/contentHash remain for display when PVD volume id is non-empty.
    tocHash is diagnostic only (varies with image authoring). */
struct DiscIdentity
{
	bool valid=false;
	bool hasIso9660=false;
	/*! Stable profile key when PVD volume id is non-empty (volume|publisher). */
	bool hasContentId=false;
	std::string volumeLabel;
	std::string systemIdentifier;
	std::string contentKey;
	unsigned int contentHash32=0;
	std::string contentHashHex;
	unsigned int pvdSectorHSG=0;
	unsigned int numTracks=0;
	unsigned int numSectors=0;
	unsigned int numAudioTracks=0;
	unsigned int numDataTracks=0;
	bool hasFingerprint=false;
	unsigned int fingerprintHash32=0;
	std::string fingerprintHashHex;
	unsigned int tocHash32=0;
	std::string tocHashHex;
};

class DiscImage
{
public:
	enum
	{
		ERROR_NOERROR,
		ERROR_UNSUPPORTED,
		ERROR_CANNOT_OPEN,
		ERROR_NOT_YET_SUPPORTED,
		ERROR_SECTOR_SIZE,
		ERROR_TOO_FEW_ARGS,
		ERROR_UNSUPPORTED_TRACK_TYPE,
		ERROR_SECTOR_LENGTH_NOT_GIVEN,
		ERROR_TRACK_INFO_WITHOTU_TRACK,
		ERROR_INCOMPLETE_MSF,
		ERROR_FIRST_TRACK_NOT_STARTING_AT_00_00_00,
		ERROR_BINARY_FILE_NOT_FOUND,
		ERROR_BINARY_SIZE_NOT_SECTOR_TIMES_INTEGER,
		ERROR_NUM_MULTI_BIN_NOT_EQUAL_TO_NUM_TRACKS,
		ERROR_MULTI_BIN_DATA_NOT_2352_PER_SEC,
		ERROR_MDS_MEDIA_TYPE,
		ERROR_MDS_MULTI_SESSION_UNSUPPORTED,
		ERROR_MDS_MULTI_FILE_UNSUPPORTED,
		ERROR_MDS_MODE2_UNSUPPORTED,
		ERROR_MDS_FILE_SIZE_DOES_NOT_MAKE_SENSE,
		ERROR_MDS_UNEXPECTED_NUMBER,
		ERROR_MDS_BINARY_TOO_SHORT,
	};
	enum
	{
		HSG_BASE=150,
		MODE1_BYTES_PER_SECTOR=2048,
		MODE2_BYTES_PER_SECTOR=2336,
		RAW_BYTES_PER_SECTOR=2340,
		AUDIO_SECTOR_SIZE=2352,
		AUDIO_SAMPLING_RATE=44100,
		DEFAULT_SECTOR_SIZE=2352,
	};
	enum
	{
		FILETYPE_NONE,
		FILETYPE_ISO,
		FILETYPE_CUE,
		FILETYPE_MDS,
		FILETYPE_CCD,
		FILETYPE_CHD,
	};
	enum
	{
		TRACK_UNKNOWNTYPE,
		TRACK_MODE1_DATA,
		TRACK_MODE2_DATA,
		TRACK_AUDIO,
	};
	class MinSecFrm
	{
	public:
		short min=0,sec=0,frm=0;
		static MinSecFrm Zero(void);
		static MinSecFrm TwoSeconds(void);

		inline void Set(short m,short s,short f)
		{
			min=m;
			sec=s;
			frm=f;
		}


		inline unsigned int Encode(void) const  // Different from HSG
		{
			return (min<<16)|(sec<<8)|frm;
		}

		inline void Increment(void)
		{
			++this->frm;
			if(75<=this->frm)
			{
				this->frm-=75;
				++this->sec;
			}
			if(60<=this->sec)
			{
				this->sec-=60;
				++this->min;
			}
		}
		inline void Decrement(void)
		{
			--this->frm;
			if(this->frm<0)
			{
				this->frm+=75;
				--this->sec;
			}
			if(this->sec<0)
			{
				this->sec+=60;
				--this->min;
			}
		}

		inline void Add(const MinSecFrm incoming)
		{
			this->frm+=incoming.frm;
			if(75<=this->frm)
			{
				this->frm-=75;
				++this->sec;
			}
			this->sec+=incoming.sec;
			if(60<=this->sec)
			{
				this->sec-=60;
				++this->min;
			}
			this->min+=incoming.min;
		}
		inline MinSecFrm &operator+=(const MinSecFrm incoming)
		{
			Add(incoming);
			return *this;
		}


		inline void Sub(const MinSecFrm incoming)
		{
			this->frm-=incoming.frm;
			if(this->frm<0)
			{
				this->frm+=75;
				--this->sec;
			}
			this->sec-=incoming.sec;
			if(this->sec<0)
			{
				this->sec+=60;
				--this->min;
			}
			this->min-=incoming.min;
		}
		inline MinSecFrm &operator-=(const MinSecFrm incoming)
		{
			Sub(incoming);
			return *this;
		}
		inline MinSecFrm operator-(const MinSecFrm incoming)
		{
			auto ret=*this;
			ret.Sub(incoming);
			return ret;
		}

		inline bool operator==(const MinSecFrm incoming) const
		{
			return this->Encode()==incoming.Encode();
		}
		inline bool operator!=(const MinSecFrm incoming) const
		{
			return this->Encode()!=incoming.Encode();
		}
		inline bool operator>(const MinSecFrm incoming) const
		{
			return this->Encode()>incoming.Encode();
		}
		inline bool operator<(const MinSecFrm incoming) const
		{
			return this->Encode()<incoming.Encode();
		}
		inline bool operator>=(const MinSecFrm incoming) const
		{
			return this->Encode()>=incoming.Encode();
		}
		inline bool operator<=(const MinSecFrm incoming) const
		{
			return this->Encode()<=incoming.Encode();
		}


		inline unsigned int ToHSG(void) const
		{
			return (min*60+sec)*75+frm;
		}
		inline void FromHSG(unsigned int HSG)
		{
			frm=HSG%75;
			HSG/=75;
			sec=HSG%60;
			HSG/=60;
			min=HSG;
		}
	};
	class Track
	{
	public:
		unsigned int trackType=TRACK_UNKNOWNTYPE;
		unsigned int sectorLength=2352; // Default 2352 bytes per sector.
		unsigned int preGapSectorLength=2352; // BIN/CUE format sucks.  Utterly confusing.
		// Note about Track::locationInFile
		// If the track has a PREGAP, locationInFile is PREGAP before INDEX01.
		// Therefore PREGAP must belong to this track.
		// However, when the sector length of the PREGAP seems to be of the previous track.
		unsigned long long int locationInFile=0;
		mutable std::vector <unsigned char> dataCache;
		MinSecFrm start,end;  // end must be 1-frame before the next track or the disc length.
		MinSecFrm index00,preGap,postGap; // Wikipedia suggests index00 is preGap.

		inline Track()
		{
			CleanUp();
		}
		inline void CleanUp(void)
		{
			trackType=TRACK_UNKNOWNTYPE;
			sectorLength=2352;
			locationInFile=0;
			dataCache.clear();
			start=MinSecFrm::Zero();
			end=MinSecFrm::Zero();
			preGap=MinSecFrm::Zero();
			postGap=MinSecFrm::Zero();
			index00=MinSecFrm::Zero();
		}
	};

	enum
	{
		LAYOUT_DATA,
		LAYOUT_AUDIO,
		LAYOUT_GAP,
		LAYOUT_END
	};
	/*! 
	Turned out Track is not really good for finding location in file from HSG/MSF.
	It is constructed in OpenCUEPostProcess.
	*/
	class DiscLayout
	{
	public:
		int layoutType;
		unsigned int sectorLength;
		unsigned int startHSG;
		unsigned int numSectors;
		uint64_t locationInFile;
		unsigned int indexToBinary=0;
	};

	/*! To support multi-bin CUE.
	    There seems to be no standard, and the data format of multi-bin CUE files are random.
	    Very difficult to support....
	*/
	class Binary
	{
	public:
		std::string fName;
		uint64_t fileSize=0;

		/*! Handling of PREGAP is one of the biggest mystery in supporting BIN/CUE.
		    Even more nonsense in multi-bin CUE file.
		    Looks like I need to skip bytes if there is a PREGAP.
		    I'm not sure though.
		    Meaning of PREGAP in CUE file in general, not just multi-bin format, looks random and inconsistent to me.
		*/
		uint64_t bytesToSkip=0;

		/*!
		*/
		uint64_t byteOffsetInDisc=0;
	};

	unsigned int fileType=FILETYPE_NONE;
	std::string fName;
	uint64_t totalBinLength=0;
	unsigned int num_sectors;
	std::vector <Binary> binaries;
	std::vector <Track> tracks;
	std::vector <DiscLayout> layout;
	std::vector <unsigned char> binaryCache;
	DiscImageChdBackend *chdBackend_=nullptr;
	bool chdAudioByteSwap_=false;
	mutable DiscIdentity cachedIdentity_;
	mutable bool identityCached_=false;
	/*! Serializes CUE/ISO/MDS file reads (async CDDA GetWave vs MODE ReadSector). */
	mutable std::mutex fileIoMutex_;
	mutable std::ifstream fileIoStream_;
	mutable std::string fileIoOpenName_;

	class TrackTime
	{
	public:
		int track;  // First track=track 1.  Not 0-based.
		MinSecFrm MSF;
	};

	DiscImage();
	static const char *ErrorCodeToText(unsigned int errCode);
	void CleanUp(void);
	unsigned int Open(const std::string &fName);
	unsigned int OpenCUE(const std::string &fName);
private:
	unsigned int OpenCUEPostProcess(void);
	void MakeLayoutFromTracksAndBinaryFiles(void);
	bool TryAnalyzeTracksWithProbablyCorrectInterpretation(void);
	bool TryAnalyzeTracksWithAbsurdCUEInterpretation(void);
	bool TryAnalyzeTracksWithMoreReasonableCUEInterpretation(void);
	bool LocateSectorInLayout(unsigned int HSG,uint64_t &fileOffset,unsigned int &sectorLength,unsigned int &indexToBinary,int &layoutType) const;

public:
	unsigned int OpenISO(const std::string &fName);


public:
	unsigned int OpenMDS(const std::string &fName);


public:
	unsigned int OpenCCD(const std::string &fName);
	unsigned int OpenCHD(const std::string &fName);
	bool ReadBinaryBytes(const Binary &bin,uint64_t offset,unsigned char *buf,size_t len) const;
	void ApplyChdAudioByteSwap(unsigned char *wave,size_t size) const;

	/*! Cache binary file.  It may take large memory.
	    If it is the multi-binary image, it only reads the first binary.
	*/
	bool CacheBinary(void);

	/*! Returns the number of tracks.
	    Returns zero if image is not loaded.
	*/
	unsigned int GetNumTracks(void) const;

	/*! Returns theu number of sectors.
	*/
	unsigned int GetNumSectors(void) const;

	/*! Returns tracks.
	*/
	const std::vector <Track> &GetTracks(void) const;

	/*! Read data sectors MODE1 (2048 bytes/sector).
	    If it cannot be read (no data track, unsupported sector length, go beyond the data-track limit, etc.),
	    it returns zero byte.
	*/
	std::vector <unsigned char> ReadSectorMODE1(unsigned int HSG,unsigned int numSec) const;

	/*! Read data sectors RAW (2340 bytes/sector).
	    If it cannot be read (no data track, unsupported sector length, go beyond the data-track limit, etc.),
	    it returns zero byte.
	*/
	std::vector <unsigned char> ReadSectorRAW(unsigned int HSG,unsigned int numSec) const;

	/*! Read data sectors MODE2 (2336 bytes/sector).
	    If it cannot be read (no data track, unsupported sector length, go beyond the data-track limit, etc.),
	    it returns zero byte.
	*/
	std::vector <unsigned char> ReadSectorMODE2(unsigned int HSG,unsigned int numSec) const;


	/*! Returns the track from MSF.
	    Returns -1 if it is not on any track.
	*/
	int GetTrackFromMSF(MinSecFrm MSF) const;


	/*! Returns the 44KHz wave from start and end MSFs.
	    Can be as large as 700MB.
	*/
	std::vector <unsigned char> GetWave(MinSecFrm startMSF,MinSecFrm endMSF) const;


	/*! Returns track and in-track time from disc-time.
	*/
	TrackTime DiscTimeToTrackTime(MinSecFrm discMSF) const;


	/*! Convert a string MM:SS:FF to MinSecFrm structure.
	*/
	static bool StrToMSF(MinSecFrm &msf,const char str[]);

	static MinSecFrm HSGtoMSF(unsigned int HSG);
	static unsigned int MSFtoHSG(MinSecFrm MSF);
	static unsigned int BinToBCD(unsigned int bin);
	static unsigned int BCDToBin(unsigned int bin);
	inline static MinSecFrm MakeMSF(unsigned int min,unsigned int sec,unsigned int frm);

	/*! ISO9660 PVD + root listing fingerprint + TOC layout hash.
	    Does not depend on host image file name.  Invalid when no disc loaded.
	    When allowDiscIO is false and identity is not yet cached, returns a TOC-only
	    stub without ReadSector (safe to call while CDDA prefetch / MODE owns the disc). */
	DiscIdentity ComputeIdentity(bool allowDiscIO=true) const;
	bool HasCachedIdentity(void) const{return identityCached_;}

	/*! FNV-1a of the first maxBytes of an ISO9660 file.
	    relPath is case-insensitive, may include directories ("GAME\\FOO.EXE")
	    or a bare basename ("FOO.EXE").  Bare basename searches the whole tree
	    (depth-limited); if multiple matches exist, returns 0 (ambiguous).
	    Returns 0 if not found / unreadable. */
	unsigned int HashIso9660FilePrefix(
	    const std::string &relPath,unsigned int maxBytes=4096) const;

private:
	std::vector <unsigned char> ReadUserData2048(unsigned int HSG) const;
	unsigned int ComputeTocHash32(void) const;
	static std::string TrimIso9660Field(const char *buf,size_t len);
	/*! Collect root directory identifiers from a PVD sector (ISO LBA→HSG via pvdHSG). */
	std::vector <std::string> CollectIso9660RootNames(const std::vector <unsigned char> &pvdSec,unsigned int pvdHSG) const;
	/*! Locate ISO9660 file by relative path; on success fills fileLba/fileBytes (ISO LBA). */
	bool FindIso9660File(
	    const std::string &relPath,
	    unsigned int &hsgOfLba0,
	    unsigned int &fileLba,
	    unsigned int &fileBytes) const;
};

inline DiscImage::MinSecFrm operator+(DiscImage::MinSecFrm l,DiscImage::MinSecFrm r)
{
	l+=r;
	return l;
}
inline DiscImage::MinSecFrm DiscImage::MakeMSF(unsigned int min,unsigned int sec,unsigned int frm)
{
	MinSecFrm msf;
	msf.Set(min,sec,frm);
	return msf;
}

/* } */
#endif
