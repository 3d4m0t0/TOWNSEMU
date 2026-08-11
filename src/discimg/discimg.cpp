/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <cstdio>

#include "discimg.h"
#include "discimg_chd.h"
#include "cpputil.h"


static char skipBuf[4096];


// Uncomment for verbose output.
// #define DEBUG_DISCIMG

/* static */ DiscImage::MinSecFrm DiscImage::MinSecFrm::Zero(void)
{
	MinSecFrm MSF;
	MSF.min=0;
	MSF.sec=0;
	MSF.frm=0;
	return MSF;
}
/* static */ DiscImage::MinSecFrm DiscImage::MinSecFrm::TwoSeconds(void)
{
	MinSecFrm MSF;
	MSF.min=0;
	MSF.sec=2;
	MSF.frm=0;
	return MSF;
}


////////////////////////////////////////////////////////////

static std::string GetExtension(const std::string &fName)
{
	auto pos = fName.rfind('.');
	if (pos == std::string::npos) {
		return "";
	}

	return fName.substr(pos);
}
static void Capitalize(std::string &str)
{
	for(auto &c : str)
	{
		if('a'<=c && c<='z')
		{
			c+=('A'-'a');
		}
	}
}


////////////////////////////////////////////////////////////


DiscImage::DiscImage()
{
	CleanUp();
}
/* static */ const char *DiscImage::ErrorCodeToText(unsigned int errCode)
{
	switch(errCode)
	{
	case ERROR_NOERROR:
		return "No error";
	case ERROR_UNSUPPORTED:
		return "Unsupported file format.";
	case ERROR_CANNOT_OPEN:
		return "Cannot open image file.";
	case ERROR_NOT_YET_SUPPORTED:
		return "File format not yet supported. (I'm working on it!)";
	case ERROR_SECTOR_SIZE:
		return "Binary size is not integer multiple of the sector length.";
	case ERROR_TOO_FEW_ARGS:
		return "Too few arguments.";
	case ERROR_UNSUPPORTED_TRACK_TYPE:
		return "Unsupported track type.";
	case ERROR_SECTOR_LENGTH_NOT_GIVEN:
		return "Sector length of the data track is not given.";
	case ERROR_TRACK_INFO_WITHOTU_TRACK:
		return "Track information is given without a track.";
	case ERROR_INCOMPLETE_MSF:
		return "Incomplete MM:SS:FF.";
	case ERROR_BINARY_FILE_NOT_FOUND:
		return "Image binary file not found.";
	case ERROR_FIRST_TRACK_NOT_STARTING_AT_00_00_00:
		return "First track not starting at 00:00:00";
	case ERROR_BINARY_SIZE_NOT_SECTOR_TIMES_INTEGER:
		return "Binary size is not integer multiple of sector lengths.";
	case ERROR_NUM_MULTI_BIN_NOT_EQUAL_TO_NUM_TRACKS:
		return "Number of binary files and the number of tracks in multi-bin CUE file do not match.";
	case ERROR_MULTI_BIN_DATA_NOT_2352_PER_SEC:
		return "All tracks must use 2352 bytes/sector in multi-bin CUE file.";
	case ERROR_MDS_MEDIA_TYPE:
		return "Unsupported MDS Media Type.";
	case ERROR_MDS_MULTI_SESSION_UNSUPPORTED:
		return "MDS Multi-Session Unsupported.";
	case ERROR_MDS_MULTI_FILE_UNSUPPORTED:
		return "MDS Multi-File Unsupported.";
	case ERROR_MDS_MODE2_UNSUPPORTED:
		return "MDS Mode 2 Unsupported.";
	case ERROR_MDS_FILE_SIZE_DOES_NOT_MAKE_SENSE:
		return "MDF Binary File Size does not make sense.";
	case ERROR_MDS_UNEXPECTED_NUMBER:
		return "MDS Unexpected Number.";
	}
	return "Undefined error.";
}
void DiscImage::CleanUp(void)
{
	if(nullptr!=chdBackend_)
	{
		delete chdBackend_;
		chdBackend_=nullptr;
	}
	chdAudioByteSwap_=false;
	fileType=FILETYPE_NONE;
	totalBinLength=0;
	fName="";
	num_sectors=0;
	binaries.clear();
	tracks.clear();
	layout.clear();
	binaryCache.clear();
	identityCached_=false;
	cachedIdentity_=DiscIdentity();
	fileIoStream_.close();
	fileIoOpenName_.clear();
}
unsigned int DiscImage::Open(const std::string &fName)
{
	auto ext=GetExtension(fName);
	Capitalize(ext);
	if(".CUE"==ext)
	{
		return OpenCUE(fName);
	}
	if(".BIN"==ext)
	{
		auto withoutExt=cpputil::RemoveExtension(fName.c_str());
		auto cue=withoutExt+".cue";
		auto CUE=withoutExt+".CUE";
		auto Cue=withoutExt+".Cue";
		if(cpputil::FileExists(cue))
		{
			return OpenCUE(cue);
		}
		if(cpputil::FileExists(CUE))
		{
			return OpenCUE(CUE);
		}
		if(cpputil::FileExists(Cue))
		{
			return OpenCUE(Cue);
		}
	}
	if(".ISO"==ext)
	{
		return OpenISO(fName);
	}
	if(".MDS"==ext)
	{
		return OpenMDS(fName);
	}
	if(".MDF"==ext)
	{
		auto withoutExt=cpputil::RemoveExtension(fName.c_str());
		auto mds=withoutExt+".mds";
		auto MDS=withoutExt+".MDS";
		auto Mds=withoutExt+".Mds";
		if(cpputil::FileExists(mds))
		{
			return OpenMDS(mds);
		}
		if(cpputil::FileExists(MDS))
		{
			return OpenMDS(MDS);
		}
		if(cpputil::FileExists(Mds))
		{
			return OpenMDS(Mds);
		}
	}
	if(".CCD"==ext)
	{
		return OpenCCD(fName);
	}
	if(".CHD"==ext)
	{
		return OpenCHD(fName);
	}
	return ERROR_UNSUPPORTED;
}
unsigned int DiscImage::OpenCUE(const std::string &fName)
{
	std::ifstream ifp;
	ifp.open(fName);
	if(true!=ifp.is_open())
	{
		return ERROR_CANNOT_OPEN;
	}

	CleanUp();
	this->fName=fName;
	fileType=FILETYPE_CUE;

	// https://en.wikipedia.org/wiki/Cue_sheet_(computing)
	enum
	{
		CMD_FILE,
		CMD_TRACK,
		CMD_INDEX,
		CMD_PREGAP,
		CMD_POSTGAP,
	};
	std::unordered_map <std::string,int> cmdMap;
	cmdMap["FILE"]=CMD_FILE;
	cmdMap["TRACK"]=CMD_TRACK;
	cmdMap["INDEX"]=CMD_INDEX;
	cmdMap["PREGAP"]=CMD_PREGAP;
	cmdMap["POSTGAP"]=CMD_POSTGAP;

	while(true!=ifp.eof())
	{
		std::string line;
		std::getline(ifp,line);

		std::vector <std::string> argv=cpputil::Parser(line.c_str());
		if(0<argv.size())
		{
			cpputil::Capitalize(argv[0]);
			auto found=cmdMap.find(argv[0]);
			if(cmdMap.end()==found)
			{
				continue;
			}
			switch(found->second)
			{
			case CMD_FILE:
				if(2<=argv.size())
				{
					Binary bin;
					bin.fName=argv[1];
					binaries.push_back(bin);
				}
				break;
			case CMD_TRACK:
				if(3<=argv.size())
				{
					Track t;
					cpputil::Capitalize(argv[2]);
					if("AUDIO"==argv[2])
					{
						t.trackType=TRACK_AUDIO;
						t.sectorLength=2352;
					}
					else if(true==cpputil::StrStartsWith(argv[2],"MODE1"))
					{
						t.trackType=TRACK_MODE1_DATA;
						auto sectLenStr=cpputil::StrSkip(argv[2].c_str(),"/");
						if(nullptr!=sectLenStr)
						{
							t.sectorLength=cpputil::Atoi(sectLenStr);
						}
						else
						{
							return ERROR_SECTOR_LENGTH_NOT_GIVEN;
						}
					}
					tracks.push_back(t);
				}
				else
				{
					return ERROR_TOO_FEW_ARGS;
				}
				break;
			case CMD_INDEX:
				if(0==tracks.size())
				{
					return ERROR_TRACK_INFO_WITHOTU_TRACK;
				}
				if(3<=argv.size())
				{
					auto indexType=cpputil::Atoi(argv[1].c_str());
					MinSecFrm msf;
					if(true!=StrToMSF(msf,argv[2].c_str()))
					{
						return ERROR_INCOMPLETE_MSF;
					}
					if(0==indexType)
					{
						tracks.back().index00=msf;
					}
					else if(1==indexType)
					{
						tracks.back().start=msf;
					}
				}
				else
				{
					return ERROR_TOO_FEW_ARGS;
				}
				break;
			case CMD_PREGAP:
				if(0==tracks.size())
				{
					return ERROR_TRACK_INFO_WITHOTU_TRACK;
				}
				if(2<=argv.size())
				{
					MinSecFrm msf;
					if(true!=StrToMSF(msf,argv[1].c_str()))
					{
						return ERROR_INCOMPLETE_MSF;
					}
					tracks.back().preGap=msf;
				}
				else
				{
					return ERROR_TOO_FEW_ARGS;
				}
				break;
			case CMD_POSTGAP:
				if(0==tracks.size())
				{
					return ERROR_TRACK_INFO_WITHOTU_TRACK;
				}
				if(2<=argv.size())
				{
					MinSecFrm msf;
					if(true!=StrToMSF(msf,argv[1].c_str()))
					{
						return ERROR_INCOMPLETE_MSF;
					}
					tracks.back().postGap=msf;
				}
				else
				{
					return ERROR_TOO_FEW_ARGS;
				}
				break;
			default:
				std::cout << "Unrecognized: " << line << std::endl;
				break;
			}
		}
	}
	return OpenCUEPostProcess();
}
unsigned int DiscImage::OpenCUEPostProcess(void)
{
	if(0==tracks.size())
	{
		return ERROR_NOERROR;
	}
	if(MinSecFrm::Zero()!=tracks.front().start ||
	   MinSecFrm::Zero()!=tracks.front().preGap ||
	   MinSecFrm::Zero()!=tracks.front().index00)
	{
		return ERROR_FIRST_TRACK_NOT_STARTING_AT_00_00_00;
	}


	if(1<binaries.size()) // Multi-bin (1)
	{
		if(binaries.size()!=tracks.size())
		{
			// The number of binaries needs to match the number of tracks.
			return ERROR_NUM_MULTI_BIN_NOT_EQUAL_TO_NUM_TRACKS;
		}
		if(2352!=tracks[0].sectorLength)
		{
			// Non-2352 bytes/sec not supported in multi-bin format.
			return ERROR_MULTI_BIN_DATA_NOT_2352_PER_SEC;
		}
		// I have absolutely no idea if I am interpreting it right.
		// Multi-bin CUE files are so inconsistent.
		for(int trk=0; trk<tracks.size(); ++trk)
		{
			// I hope it is a correct interpretation.
			binaries[trk].bytesToSkip=2352*MSFtoHSG(tracks[trk].preGap+tracks[trk].start);
		}
	}


	uint64_t binLength=0;
	for(auto &bin : binaries)
	{
		std::vector <std::string> binFileCandidate;
		{
			std::string path,file;
			cpputil::SeparatePathFile(path,file,fName);
			binFileCandidate.push_back(path+bin.fName);
		}
		{
			std::string base=cpputil::RemoveExtension(fName.c_str());
			binFileCandidate.push_back(base+".BIN");
			binFileCandidate.push_back(base+".IMG");
			binFileCandidate.push_back(base+".bin");
			binFileCandidate.push_back(base+".img");
			binFileCandidate.push_back(base+".Bin");
			binFileCandidate.push_back(base+".Img");
		}
		bin.fName="";
		for(auto fn : binFileCandidate)
		{
			auto len=cpputil::FileSize(fn);
			if(0<len)
			{
				binLength+=len;
				bin.fName=fn;
				bin.fileSize=len;
				break;
			}
		}
	}
	if(0==binLength)
	{
		return ERROR_BINARY_FILE_NOT_FOUND;
	}
	this->totalBinLength=binLength;


	if(1<binaries.size()) // Multi-bin (2)
	{
		// Information in multi-bin CUE file is very erratic and unreliable.
		// I need to re-calculate based on the binary file sizes.
		// Whoever designed .CUE data format did a poor job by having PREGAP and INDEX 00.
		// INDEX 00 shouldn't have existed at all.  So confusing.
		unsigned int startSector=0;
		uint64_t locationInFile=0;
		for(int i=0; i<tracks.size(); ++i)
		{
			uint32_t numBytes=binaries[i].fileSize;
			unsigned int numSec=numBytes/tracks[i].sectorLength;
			unsigned int numPreGapSec=binaries[i].bytesToSkip/tracks[i].sectorLength;
			tracks[i].start=HSGtoMSF(startSector+numPreGapSec);
			tracks[i].end=HSGtoMSF(startSector+numSec-numPreGapSec-1);
			tracks[i].locationInFile=locationInFile;
			binaries[i].byteOffsetInDisc=locationInFile + binaries[i].bytesToSkip;
			locationInFile+=numBytes;
			startSector+=numSec;
		}
	}

	if(1==tracks.size())
	{
		unsigned int numSec=(unsigned int)binLength/tracks[0].sectorLength;
		if(0!=(binLength%tracks[0].sectorLength))
		{
			return ERROR_BINARY_SIZE_NOT_SECTOR_TIMES_INTEGER;
		}
		--numSec;
		tracks[0].end=HSGtoMSF(numSec);
		tracks[0].locationInFile=0;
	}
	else
	{
		if(true!=TryAnalyzeTracksWithProbablyCorrectInterpretation() &&
		   true!=TryAnalyzeTracksWithAbsurdCUEInterpretation() &&
		   true!=TryAnalyzeTracksWithMoreReasonableCUEInterpretation())
		{
			// All interpretations failed.
			return ERROR_BINARY_SIZE_NOT_SECTOR_TIMES_INTEGER;
		}
	}

	if(0<tracks.size())
	{
		num_sectors=tracks.back().end.ToHSG()+1;  // LastSectorNumber+1
	}

	MakeLayoutFromTracksAndBinaryFiles();

#ifdef DEBUG_DISCIMG
	for(auto L : layout)
	{
		std::cout << "LAYOUT ";
		switch(L.layoutType)
		{
		case LAYOUT_DATA:
			std::cout << "DATA  ";
			break;
		case LAYOUT_AUDIO:
			std::cout << "AUDIO ";
			break;
		case LAYOUT_GAP:
			std::cout << "GAP   ";
			break;
		case LAYOUT_END:
			std::cout << "END   ";
			break;
		default:
			std::cout << "????? ";
			break;
		}
		std::cout << L.startHSG << " " << L.locationInFile << " " << totalBinLength << std::endl;
	}
#endif

	return ERROR_NOERROR;
}
void DiscImage::MakeLayoutFromTracksAndBinaryFiles(void)
{
	for(long long int i=0; i<tracks.size(); ++i)
	{
		DiscLayout L;
		switch(tracks[i].trackType)
		{
		case TRACK_MODE1_DATA:
		case TRACK_MODE2_DATA:
			L.layoutType=LAYOUT_DATA;
			break;
		case TRACK_AUDIO:
			L.layoutType=LAYOUT_AUDIO;
			break;
		}
		L.numSectors=tracks[i].end.ToHSG()-tracks[i].start.ToHSG()+1;
		L.sectorLength=tracks[i].sectorLength;
		L.startHSG=tracks[i].start.ToHSG();
		L.indexToBinary=(1==binaries.size() ? 0 : i);
		auto locationInFile=tracks[i].locationInFile;
		auto preGapInHSG=tracks[i].preGap.ToHSG();
		if(0<preGapInHSG)
		{
			DiscLayout preGap;
			preGap.layoutType=LAYOUT_GAP;
			preGap.sectorLength=tracks[i].preGapSectorLength;
			preGap.startHSG=tracks[i].start.ToHSG()-preGapInHSG;
			preGap.numSectors=preGapInHSG;
			preGap.locationInFile=locationInFile;
			preGap.indexToBinary=(1==binaries.size() ? 0 : i);
			layout.push_back(preGap);
			locationInFile+=tracks[i].preGapSectorLength*preGapInHSG;
		}
		L.startHSG=tracks[i].start.ToHSG();
		L.locationInFile=locationInFile;
		layout.push_back(L);
	}
	{
		DiscLayout L;
		L.layoutType=LAYOUT_END;
		L.sectorLength=0;
		L.numSectors=0;
		L.indexToBinary=(1==binaries.size() ? 0 : binaries.size()-1);
		if(0<layout.size())
		{
			L.startHSG=layout.back().startHSG+layout.back().numSectors;
			L.locationInFile=layout.back().locationInFile+layout.back().sectorLength*layout.back().numSectors;
		}
		else
		{
			L.startHSG=0;
			L.locationInFile=0;
		}
		layout.push_back(L);
	}
}
bool DiscImage::TryAnalyzeTracksWithProbablyCorrectInterpretation(void)
{
	// Interpretation based on .CUE file written by Alcohol 52% and CD Manipulator.
	// Other image-ripping program may write different .CUE, but I don't care.

	// Looks like if keyword PREGAP exists it means that pre-gap sectors are not
	// written in the binary, but insert 2 second of silence when it plays.
	// POSTGAP is similar, but it is after the track.
	// Implication is the length of the disc is the length calculated from the binary
	// plus length specified by PREGAP and POSTGAP.

	// Looks like if the track has PREGAP, from the CD-player point of view, 
	// PREGAP+INDEX 01 is the starting time of the track.
	// Therefore, if it says INDEX 01 10:00:00, and if PREGAP 00:02:00
	// existed before or on that track, it needs to be presented as 10:02:00.

	// Need to set:
	//   preGapSectorLength
	//   start
	//   end
	//   locationInFile


	// If no PREGAP, same as TryAnalyzeTracksWithAbsurdCUEInterpretation.


	// If this interpretation works, sectors of PREGAP are not stored in the binary.
	// Therefore, pre-gap sector length does not matter.
	// But, the total sector count of PREGAP needs to be added to the number of sectors
	// calculated from the binary size.
	MinSecFrm totalPREGAP;
	totalPREGAP.Set(0,0,0);
	size_t pointerInBinary=0;
	for(size_t i=0; i<tracks.size(); ++i)
	{
		tracks[i].preGapSectorLength=0;
		totalPREGAP+=tracks[i].preGap;

		tracks[i].locationInFile=pointerInBinary;
		if(i+1<tracks.size())
		{
			auto trackLen=tracks[i+1].start-tracks[i].start;
			pointerInBinary+=(tracks[i].sectorLength*trackLen.ToHSG());
		}

		tracks[i].start+=totalPREGAP;  // Displace the track-start MSF.
		tracks[i].preGap.FromHSG(0); // Then forget about this cursed PREGAP.
	}


	// pointerInBinary should be pointing to the pointer for the last track here.
	// Then, the number of sectors of the last track must be:
	auto lastTrackNumBytes=totalBinLength-pointerInBinary;
	if(lastTrackNumBytes%tracks.back().sectorLength || 0==lastTrackNumBytes)
	{
		// Inconsistency.  Remaining bytes must be integer times last track's sector length.
		return false;
	}
	auto lastTrackNumSec=lastTrackNumBytes/tracks.back().sectorLength;

	tracks.back().end.FromHSG(lastTrackNumSec-1);
	tracks.back().end+=tracks.back().start;

	for(size_t i=0; i+1<tracks.size(); ++i)
	{
		tracks[i].end=tracks[i+1].start;
		tracks[i].end.Decrement();
	}

	return true;
}
bool DiscImage::TryAnalyzeTracksWithAbsurdCUEInterpretation(void)
{
	for(long long int i=0; i<(int)tracks.size(); ++i)
	{
		// Why I say this interpretation is absurd.
		// Why pre-gap sector length of this sector needs to be the sector length of the previous track?
		tracks[i].preGapSectorLength=(0==i ? 0 : tracks[i-1].sectorLength);
	}

	long long int prevTrackSizeInBytes=0;
	for(long long int i=0; i<(int)tracks.size(); ++i)
	{
		long long int trackLength=0,gapLength=0;
		if(i+1<tracks.size())
		{
			auto endMSF=tracks[i+1].start-tracks[i+1].preGap;
			auto endHSG=endMSF.ToHSG();
			tracks[i].end=HSGtoMSF(endHSG-1);
			trackLength=(endHSG-tracks[i].start.ToHSG())*tracks[i].sectorLength;
		}
		if(1<=i)
		{
			tracks[i].locationInFile=tracks[i-1].locationInFile+prevTrackSizeInBytes;
			auto preGap=tracks[i].preGap.ToHSG();
			gapLength=preGap*tracks[i].preGapSectorLength;
		}
		prevTrackSizeInBytes=trackLength+gapLength;
	}

	auto lastTrackBytes=totalBinLength-tracks.back().locationInFile;
	if(0!=(lastTrackBytes%tracks.back().sectorLength))
	{
		return false;
	}
	auto lastTrackNumSec=(totalBinLength-tracks.back().locationInFile)/tracks.back().sectorLength;
	auto lastSectorHSG=MSFtoHSG(tracks.back().start)+lastTrackNumSec-1;
	tracks.back().end=HSGtoMSF((unsigned int)lastSectorHSG);

	return true;
}
bool DiscImage::TryAnalyzeTracksWithMoreReasonableCUEInterpretation(void)
{
	// More straight-forward interpretation, but I am not sure if it is correct.
	for(long long int i=0; i<(int)tracks.size(); ++i)
	{
		tracks[i].preGapSectorLength=2352;
	}

	for(long long int i=0; i<tracks.size(); ++i)
	{
		if(i+1<tracks.size())
		{
			tracks[i].end=HSGtoMSF(tracks[i+1].start.ToHSG()-tracks[i+1].preGap.ToHSG()-1);
		}
		tracks[i].locationInFile=(tracks[i].start.ToHSG()-tracks[i].preGap.ToHSG())*2352;
	}

	auto lastTrackBytes=totalBinLength-tracks.back().locationInFile;
	if(0!=(lastTrackBytes%tracks.back().sectorLength))
	{
		return false;
	}
	auto lastTrackNumSec=(totalBinLength-tracks.back().locationInFile)/tracks.back().sectorLength;
	auto lastSectorHSG=MSFtoHSG(tracks.back().start)+lastTrackNumSec-1;
	tracks.back().end=HSGtoMSF((unsigned int)lastSectorHSG);

	return true;
}

unsigned int DiscImage::OpenISO(const std::string &fName)
{
	std::ifstream ifp;
	ifp.open(fName,std::ios::binary);
	if(true!=ifp.is_open())
	{
		return ERROR_CANNOT_OPEN;
	}

	auto begin=ifp.tellg();
	ifp.seekg(0,std::ios::end);
	auto end=ifp.tellg();

	auto fSize=end-begin;

	fSize&=(~2047); // Some image-generation tools adds extra bytes that need to be ignored.
	// if(0!=fSize%2048)
	// {
	// 	return ERROR_SECTOR_SIZE;
	// }

	CleanUp();


	fileType=FILETYPE_ISO;
	this->fName=fName;

	Binary bin;
	bin.fName=fName;
	binaries.push_back(bin);

	num_sectors=(unsigned int)(fSize/2048);

	tracks.resize(1);
	tracks[0].trackType=TRACK_MODE1_DATA;
	tracks[0].sectorLength=2048;
	tracks[0].start=MinSecFrm::Zero();
	tracks[0].end=HSGtoMSF(num_sectors);
	tracks[0].postGap=MinSecFrm::Zero();

	return ERROR_NOERROR;
}

unsigned int DiscImage::OpenMDS(const std::string &fName)
{
	std::ifstream ifp(fName,std::ios::binary);
	if(true==ifp.is_open())
	{
		uint64_t mdfSize=0;
		std::string mdfFName;
		std::string fNameBase=cpputil::RemoveExtension(fName.c_str());
		const std::string MDFCandidate[]=
		{
			fNameBase+".MDF",
			fNameBase+".mdf",
			fNameBase+".Mdf"
		};
		for(auto fn : MDFCandidate)
		{
			auto s=cpputil::FileSize(fn);
			if(0!=s)
			{
				mdfSize=s;
				mdfFName=fn;
				break;
			}
		}
		if(0==mdfSize)
		{
			return ERROR_BINARY_FILE_NOT_FOUND;
		}



		unsigned char MDSHeaderBytes[0x58];
		ifp.read((char *)MDSHeaderBytes,0x58);

		std::string FileID;
		for(int i=0; i<16; ++i)
		{
			FileID.push_back(MDSHeaderBytes[i]);
		}
		unsigned int mediaType=cpputil::GetWord(MDSHeaderBytes+0x12);
		unsigned int nSessions=cpputil::GetWord(MDSHeaderBytes+0x14);
		uint32_t sessionOffset=cpputil::GetDword(MDSHeaderBytes+0x50);
		std::cout << "Media Type:" << mediaType << std::endl;
		std::cout << "Number of Sessions:" << nSessions << std::endl;
		std::cout << "Session Offset:" << cpputil::Itox(sessionOffset) << std::endl;
		if(0!=mediaType && 1!=mediaType && 2!=mediaType)
		{
			return ERROR_MDS_MEDIA_TYPE;
		}
		if(1!=nSessions)
		{
			return ERROR_MDS_MULTI_SESSION_UNSUPPORTED;
		}



		ifp.seekg(sessionOffset,ifp.beg);
		unsigned char MDSSessionBytes[0x18];
		ifp.read((char *)MDSSessionBytes,0x18);
		uint32_t startSec=cpputil::GetDword(MDSSessionBytes);
		uint32_t sectorCount=cpputil::GetDword(MDSSessionBytes+0x04);
		// According to https://problemkaputt.de/psx-spx.htm#cdromdiskimagesmdsmdfalcohol120,
		// MDSSessionBytes+0x04 is the end sector.  I suspect it is a sector count including the first 2 seconds (150 sectors).

		// The interpretation of the sector count is in the dark again.
		// Does it include the first two seconds where TOC and other information is stored?
		// Does it include PreGap sectors?
		// There is absolutely no answer, and there even is a chance that nobody has ever
		// defined it.
		// In BIN/CUE, it was impossible to know the sector count other than dividing the
		// binary size by the sector length.
		// However, in MDS/MDF it is more reasonable to assume that the sector count is
		// the start sector of the last track plus the number of sectors of the last track.
		// The number of sectors of the last track can be calculated by subtracting
		// .MDF file size minus start position in file of the last track.
		// This start position in file was critically missing in BIN/CUE format.

		unsigned int sessionID=cpputil::GetWord(MDSSessionBytes+0x08);
		unsigned int nDataBlocks=MDSSessionBytes[0x0A];
		unsigned int nLeadInInfo=MDSSessionBytes[0x0B];
		unsigned int firstTrack=cpputil::GetWord(MDSSessionBytes+0x0C);
		unsigned int lastTrack=cpputil::GetWord(MDSSessionBytes+0x0E);
		unsigned int dataBlockOffset=cpputil::GetDword(MDSSessionBytes+0x14);
		std::cout << "Start Sector:" << cpputil::Itox(startSec) << std::endl;
		std::cout << "Sector Count:" << sectorCount << std::endl;
		std::cout << "Session ID:" << sessionID << std::endl;
		std::cout << "Num Data Blocks:" << nDataBlocks << std::endl;
		std::cout << "Num Lead In:" << nLeadInInfo << std::endl;
		std::cout << "First Track:" << firstTrack << std::endl;
		std::cout << "Last Track:" << lastTrack << std::endl;
		std::cout << "Data Block Offset:" << dataBlockOffset << std::endl;

		if(0xFFFFFFFF-149!=startSec)
		{
			// Start sector needs to be -150
			return ERROR_UNSUPPORTED;
		}



		CleanUp();
		this->fName=fName;
		fileType=FILETYPE_MDS;

		num_sectors=sectorCount-150; // This interpretation is questionable.

		bool first=true;
		uint32_t prevFileNameOffset=0;

		ifp.seekg(dataBlockOffset,ifp.beg);
		for(unsigned int i=0; i<nDataBlocks; ++i)
		{
			unsigned char dataBlockBytes[0x50];
			ifp.read((char *)dataBlockBytes,0x50);

			unsigned int trackMode=dataBlockBytes[0];
			// A9:Audio        2352 bytes/sec
			// AA:Mode1        2048 bytes/sec
			// AB:Mode2        2336 bytes/sec
			// AC:Mode2_Form1  2048 bytes/sec
			// AD:Mode2_Form2  ?
			// EC:Mode2        2448 bytes/sec
			unsigned int numSubChannels=dataBlockBytes[1]; // 8-> +60h bytes?
			unsigned int ADR=dataBlockBytes[2]; // What is it?
			unsigned int trackNum=dataBlockBytes[3];
			unsigned int min=dataBlockBytes[0x09]; // Non-BCD
			unsigned int sec=dataBlockBytes[0x0A];
			unsigned int frm=dataBlockBytes[0x0b];

			std::cout << std::endl;
			std::cout << "Track Mode:" << cpputil::Ubtox(trackMode) << std::endl;
			std::cout << "Num Sub Channels:" << numSubChannels << std::endl;
			std::cout << "ADR(What is it?):" << ADR << std::endl;
			std::cout << "Track:" << trackNum << std::endl;
			std::cout << "Min Sec Frm:" << min << " " << sec << " " << frm << std::endl;

			if(0xA0<=dataBlockBytes[0x04]) // Lead-In Info.  What is it?
			{
				std::cout << "Lead In Info" << std::endl;
			}
			else
			{
				uint32_t offset=cpputil::GetDword(dataBlockBytes+0x0C);
				unsigned int sectorSize=cpputil::GetWord(dataBlockBytes+0x10);
				uint32_t startSector=cpputil::GetDword(dataBlockBytes+0x24);
				uint32_t MDFOffset=cpputil::GetDword(dataBlockBytes+0x28);
				uint32_t numFileNames=cpputil::GetDword(dataBlockBytes+0x30);
				uint32_t fileNameOffset=cpputil::GetDword(dataBlockBytes+0x34);

				std::cout << "Offset:" << offset << std::endl;
				std::cout << "Sector Size:" << sectorSize << std::endl;
				std::cout << "Start Sector:" << startSector << std::endl;
				std::cout << "Offset in MDF:" << MDFOffset << std::endl;
				std::cout << "Number of File Names:" << numFileNames << std::endl;
				std::cout << "File Name Offset:" << fileNameOffset << std::endl;

				if(true==first)
				{
					prevFileNameOffset=fileNameOffset;
					first=false;
				}
				else
				{
					if(prevFileNameOffset!=fileNameOffset)
					{
						return ERROR_MDS_MULTI_FILE_UNSUPPORTED;
					}
				}

				Track trk;
				switch(trackMode)
				{
				case 0xA9:
					trk.trackType=TRACK_AUDIO;
					break;
				case 0xAA:
					trk.trackType=TRACK_MODE1_DATA;
					break;
				default:
					return ERROR_MDS_MODE2_UNSUPPORTED;
				}
				trk.sectorLength=sectorSize;
				trk.preGapSectorLength=sectorSize; // I hope BIN/CUE's absurd pre-gap sector length doesn't haunt MDS.
				trk.locationInFile=MDFOffset;
				trk.start=HSGtoMSF(startSector);
				tracks.push_back(trk);
			}
		}


		// Does the file size make sense?
		for(int t=0; t+1<tracks.size(); ++t)
		{
			uint32_t trackEndSector;
			trackEndSector=MSFtoHSG(tracks[t+1].start)-1;

			tracks[t].end=HSGtoMSF(trackEndSector);
			uint32_t trackStartSector=MSFtoHSG(tracks[t].start);
		}

		// See comments above.  It makes more sense to calculate the number of sectors
		// based on the last track location in file and the binary size.
		if(tracks.back().locationInFile<mdfSize)
		{
			unsigned int lastTrackBytes=mdfSize-tracks.back().locationInFile;
			unsigned int lastTrackNumSec=lastTrackBytes/tracks.back().sectorLength;
			num_sectors=MSFtoHSG(tracks.back().start)+lastTrackNumSec;
			tracks.back().end=HSGtoMSF(num_sectors-1);
		}
		else
		{
			return ERROR_MDS_BINARY_TOO_SHORT;
		}

		Binary bin;
		bin.fName=mdfFName;
		binaries.push_back(bin);



		MakeLayoutFromTracksAndBinaryFiles();

		return ERROR_NOERROR;
	}
	return ERROR_CANNOT_OPEN;
}

unsigned int DiscImage::OpenCCD(const std::string &fName)
{
	std::ifstream ifp(fName,std::ios::binary);
	if(true==ifp.is_open())
	{
		uint64_t binSize=0;
		std::string binFName;
		std::string fNameBase=cpputil::RemoveExtension(fName.c_str());
		const std::string binCandidate[]=
		{
			fNameBase+".img",
			fNameBase+".bin",
		};
		for(auto fn : binCandidate)
		{
			auto s=cpputil::FileSize(fn);
			if(0!=s)
			{
				binSize=s;
				binFName=fn;
				break;
			}
		}
		if(0==binSize)
		{
			return ERROR_BINARY_FILE_NOT_FOUND;
		}

		CleanUp();
		this->fName=fName;
		fileType=FILETYPE_CCD;

		totalBinLength=binSize;
		num_sectors=binSize/DEFAULT_SECTOR_SIZE;

		int state=0;
		while(true!=ifp.eof())
		{
			std::string str;
			std::getline(ifp,str);

			for(auto &c : str)
			{
				c=toupper(c);
			}

			size_t i;
			bool squareBracket=false;
			for(i=0; i<str.size(); ++i)
			{
				if('['==str[i])
				{
					squareBracket=true;
					break;
				}
			}

			if(true==squareBracket) // Found '['
			{
				state=0; // Tentatively change to not in [TRACK]
				for(; i<str.size(); ++i)
				{
					if(true==cpputil::StrStartsWith(str.data()+i,"TRACK"))
					{
						Track trk;
						trk.trackType=TRACK_AUDIO; // Tentative.
						trk.sectorLength=DEFAULT_SECTOR_SIZE; // Fixed?
						trk.preGapSectorLength=0; // Doesn't matter.
						trk.locationInFile=0; // Tentative
						trk.start.FromHSG(0); // Tentative.
						tracks.push_back(trk);
						state=1;
					}
				}
			}
			else
			{
				switch(state)
				{
				case 0:
					break;
				case 1:  // Reading [TRACK ?]
					{
						size_t i=0;
						int cmd=0;
						for(; i<str.size(); ++i)
						{
							if(true==cpputil::StrStartsWith(str.data()+i,"MODE"))
							{
								cmd=1;
								i+=4;
								break;
							}
							else if(true==cpputil::StrStartsWith(str.data()+i,"INDEX"))
							{
								cmd=2;
								i+=5;
								break;
							}
						}
						if(1==cmd)
						{
							for(; i<str.size(); ++i)
							{
								if('0'<=str[i] && str[i]<='9')
								{
									switch(str[i])
									{
									case '0':
										tracks.back().trackType=TRACK_AUDIO;
										break;
									case '1':
										tracks.back().trackType=TRACK_MODE1_DATA;
										break;
									case '2':
										tracks.back().trackType=TRACK_MODE2_DATA;
										break;
									}
									break;
								}
							}
						}
						else if(2==cmd)
						{
							int indexType=-1;
							for(; i<str.size(); ++i)
							{
								if('0'<=str[i] && str[i]<='9')
								{
									indexType=cpputil::Atoi(str.c_str()+i);
									break;
								}
							}
							if(1==indexType) // Index Type 1 is all I care about.
							{
								bool passedEqual=false;
								for(; i<str.size(); ++i)
								{
									if('='==str[i])
									{
										passedEqual=true;
										break;
									}
								}
								if(true==passedEqual)
								{
									for(; i<str.size(); ++i)
									{
										if('0'<=str[i] && str[i]<='9')
										{
											size_t hsg=cpputil::Atoi(str.c_str()+i);
											tracks.back().start.FromHSG(hsg);
											tracks.back().locationInFile=DEFAULT_SECTOR_SIZE*hsg;
											break;
										}
									}
								}
							}
						}
					}
					break;
				}
			}
		}

		for(size_t i=0; i+1<tracks.size(); ++i)
		{
			tracks[i].end=tracks[i+1].start;
			tracks[i].end.Decrement();
		}
		tracks.back().end.FromHSG(num_sectors-1);

		Binary bin;
		bin.fName=binFName;
		binaries.push_back(bin);

		MakeLayoutFromTracksAndBinaryFiles();

		return ERROR_NOERROR;
	}
	return ERROR_CANNOT_OPEN;
}

unsigned int DiscImage::OpenCHD(const std::string &fName)
{
	CleanUp();

	chdBackend_=new DiscImageChdBackend;
	std::string error_message;
	if(true!=chdBackend_->Open(fName,error_message))
	{
		std::cout << "DiscImage::OpenCHD failed: " << error_message << std::endl;
		CleanUp();
		return ERROR_CANNOT_OPEN;
	}

	std::vector<DiscImageChdTrack> chd_tracks;
	uint64_t total_bin_length=0;
	bool need_audio_byte_swap=false;
	if(true!=DiscImageParseChdTracks(
	       chdBackend_->Handle(),
	       chdBackend_->BytesPerFrame(),
	       need_audio_byte_swap,
	       chd_tracks,
	       total_bin_length,
	       error_message))
	{
		std::cout << "DiscImage::OpenCHD failed: " << error_message << std::endl;
		CleanUp();
		return ERROR_UNSUPPORTED;
	}

	this->fName=fName;
	fileType=FILETYPE_CHD;
	chdAudioByteSwap_=true;
	totalBinLength=total_bin_length;

	const uint32_t bytes_per_frame=chdBackend_->BytesPerFrame();
	for(const auto &chd_track : chd_tracks)
	{
		Track trk;
		trk.trackType=chd_track.track_type;
		// CHD stores CD_FRAME_SIZE (often 2448) units with per-track padding; byte
		// offsets must come from the CHD layout (location_in_file), not start_hsg*bpf.
		trk.sectorLength=bytes_per_frame;
		trk.preGapSectorLength=chd_track.sector_length;
		trk.locationInFile=chd_track.location_in_file;
		trk.preGap.FromHSG(0);
		trk.start.FromHSG(chd_track.start_hsg);
		trk.end.FromHSG(chd_track.end_hsg);
		// In-stream pregap (pgtype V*): present in the CHD at location_in_file, but
		// .CUE TOC/play uses INDEX 01.  Map startHSG to INDEX 01 audio so CDDA matches CUE.
		if(0<chd_track.pgdatasize && 0<chd_track.pregap &&
		   chd_track.end_hsg>=chd_track.start_hsg+chd_track.pregap)
		{
			trk.locationInFile+=static_cast<uint64_t>(chd_track.pregap)*bytes_per_frame;
			trk.end.FromHSG(chd_track.end_hsg-chd_track.pregap);
		}
		tracks.push_back(trk);
	}
	num_sectors=(true!=tracks.empty() ? tracks.back().end.ToHSG()+1 : 0);

	Binary bin;
	bin.fName=fName;
	bin.fileSize=total_bin_length;
	binaries.push_back(bin);

	MakeLayoutFromTracksAndBinaryFiles();
	return ERROR_NOERROR;
}

bool DiscImage::ReadBinaryBytes(const Binary &bin,uint64_t offset,unsigned char *buf,size_t len) const
{
	if(nullptr==buf || 0==len)
	{
		return false;
	}
	if(FILETYPE_CHD==fileType && nullptr!=chdBackend_)
	{
		return chdBackend_->Read(offset,buf,len);
	}
	// Keep one open handle under the mutex: CDDA prefetch of a multi-minute track
	// used to open/seek/close per 4-frame chunk and stall PLAY status for the guest.
	std::lock_guard<std::mutex> lock(fileIoMutex_);
	if(fileIoOpenName_!=bin.fName || true!=fileIoStream_.is_open())
	{
		fileIoStream_.close();
		fileIoStream_.clear();
		fileIoStream_.open(bin.fName,std::ios::binary);
		fileIoOpenName_=bin.fName;
		if(true!=fileIoStream_.is_open())
		{
			fileIoOpenName_.clear();
			return false;
		}
	}
	fileIoStream_.clear();
	fileIoStream_.seekg(static_cast<std::streamoff>(offset),std::ios::beg);
	fileIoStream_.read(reinterpret_cast<char *>(buf),static_cast<std::streamsize>(len));
	return fileIoStream_.gcount()==static_cast<std::streamsize>(len);
}

void DiscImage::ApplyChdAudioByteSwap(unsigned char *wave,size_t size) const
{
	if(true!=chdAudioByteSwap_ || nullptr==wave || 0==size)
	{
		return;
	}
	for(size_t i=0; i+1<size; i+=2)
	{
		const unsigned char b=wave[i];
		wave[i]=wave[i+1];
		wave[i+1]=b;
	}
}

bool DiscImage::CacheBinary(void)
{
	if(0<binaries.size())
	{
		if(FILETYPE_CHD==fileType && nullptr!=chdBackend_)
		{
			binaryCache.resize(static_cast<size_t>(totalBinLength));
			if(true!=ReadBinaryBytes(binaries[0],0,binaryCache.data(),binaryCache.size()))
			{
				binaryCache.clear();
				return false;
			}
			return true;
		}
		std::ifstream ifp;
		ifp.open(binaries[0].fName,std::ios::binary);
		if(true==ifp.is_open())
		{
			ifp.seekg(0,std::ios::end);
			auto fSize=ifp.tellg();

			ifp.seekg(0,std::ios::beg);
			binaryCache.resize(fSize);

			ifp.read((char *)binaryCache.data(),fSize);

			return true;
		}
	}
	return false;
}

unsigned int DiscImage::GetNumTracks(void) const
{
	return (unsigned int)tracks.size();
}
unsigned int DiscImage::GetNumSectors(void) const
{
	return num_sectors;
}
const std::vector <DiscImage::Track> &DiscImage::GetTracks(void) const
{
	return tracks;
}
/* static */ DiscImage::MinSecFrm DiscImage::HSGtoMSF(unsigned int HSG)
{
	MinSecFrm MSF;
	MSF.FromHSG(HSG);
	return MSF;
}
/* static */ unsigned int DiscImage::MSFtoHSG(MinSecFrm MSF)
{
	return MSF.ToHSG();
}
/* static */ unsigned int DiscImage::BinToBCD(unsigned int bin)
{
	unsigned int high=bin/10;
	unsigned int low=bin%10;
	return (high<<4)+low;
}
/* static */ unsigned int DiscImage::BCDToBin(unsigned int bin)
{
	unsigned int high=(bin>>4);
	unsigned int low=(bin&15);
	return high*10+low;
}

std::vector <unsigned char> DiscImage::ReadSectorMODE1(unsigned int HSG,unsigned int numSec) const
{
	std::vector <unsigned char> data;
	if(0==numSec || 0==binaries.size())
	{
		return data;
	}

	data.resize(numSec*MODE1_BYTES_PER_SECTOR);
	unsigned int dataPointer=0;
	for(unsigned int sec=0; sec<numSec; ++sec)
	{
		const unsigned int curHSG=HSG+sec;
		uint64_t fileOffset=0;
		unsigned int sectorLength=0;
		unsigned int indexToBinary=0;
		int layoutType=LAYOUT_DATA;
		if(true!=LocateSectorInLayout(curHSG,fileOffset,sectorLength,indexToBinary,layoutType))
		{
			data.clear();
			return data;
		}
		if(LAYOUT_GAP==layoutType)
		{
			memset(data.data()+dataPointer,0,MODE1_BYTES_PER_SECTOR);
			dataPointer+=MODE1_BYTES_PER_SECTOR;
			continue;
		}
		if(LAYOUT_AUDIO==layoutType)
		{
			data.clear();
			return data;
		}

		const auto &bin=binaries[indexToBinary];
		if(MODE1_BYTES_PER_SECTOR==sectorLength)
		{
			if(true!=ReadBinaryBytes(bin,fileOffset,data.data()+dataPointer,MODE1_BYTES_PER_SECTOR))
			{
				data.clear();
				return data;
			}
		}
		else if(MODE1_BYTES_PER_SECTOR+16<=sectorLength)
		{
			unsigned char sectorBuf[4096];
			if(sectorLength>sizeof(sectorBuf) || true!=ReadBinaryBytes(bin,fileOffset,sectorBuf,sectorLength))
			{
				data.clear();
				return data;
			}
			memcpy(data.data()+dataPointer,sectorBuf+16,MODE1_BYTES_PER_SECTOR);
		}
		else
		{
			data.clear();
			return data;
		}
		dataPointer+=MODE1_BYTES_PER_SECTOR;
	}
	return data;
}

std::vector <unsigned char> DiscImage::ReadSectorRAW(unsigned int HSG,unsigned int numSec) const
{
	std::vector <unsigned char> data;
	if(0==numSec || 0==binaries.size())
	{
		return data;
	}

	data.resize(numSec*RAW_BYTES_PER_SECTOR);
	unsigned int dataPointer=0;
	for(unsigned int sec=0; sec<numSec; ++sec)
	{
		const unsigned int curHSG=HSG+sec;
		uint64_t fileOffset=0;
		unsigned int sectorLength=0;
		unsigned int indexToBinary=0;
		int layoutType=LAYOUT_DATA;
		if(true!=LocateSectorInLayout(curHSG,fileOffset,sectorLength,indexToBinary,layoutType))
		{
			data.clear();
			return data;
		}
		if(LAYOUT_GAP==layoutType || LAYOUT_AUDIO==layoutType)
		{
			memset(data.data()+dataPointer,0,RAW_BYTES_PER_SECTOR);
			dataPointer+=RAW_BYTES_PER_SECTOR;
			continue;
		}

		const auto &bin=binaries[indexToBinary];
		if(MODE1_BYTES_PER_SECTOR==sectorLength)
		{
			memset(data.data()+dataPointer,0,RAW_BYTES_PER_SECTOR);
			if(true!=ReadBinaryBytes(bin,fileOffset,data.data()+dataPointer+4,MODE1_BYTES_PER_SECTOR))
			{
				data.clear();
				return data;
			}
		}
		else if(RAW_BYTES_PER_SECTOR+12<=sectorLength)
		{
			unsigned char sectorBuf[4096];
			if(sectorLength>sizeof(sectorBuf) || true!=ReadBinaryBytes(bin,fileOffset,sectorBuf,sectorLength))
			{
				data.clear();
				return data;
			}
			memcpy(data.data()+dataPointer,sectorBuf+12,RAW_BYTES_PER_SECTOR);
		}
		else
		{
			memset(data.data()+dataPointer,0,RAW_BYTES_PER_SECTOR);
		}
		dataPointer+=RAW_BYTES_PER_SECTOR;
	}
	return data;
}

std::vector <unsigned char> DiscImage::ReadSectorMODE2(unsigned int HSG,unsigned int numSec) const
{
	std::vector <unsigned char> data;
	if(0==numSec || 0==binaries.size())
	{
		return data;
	}

	data.resize(numSec*RAW_BYTES_PER_SECTOR);
	unsigned int dataPointer=0;
	for(unsigned int sec=0; sec<numSec; ++sec)
	{
		const unsigned int curHSG=HSG+sec;
		uint64_t fileOffset=0;
		unsigned int sectorLength=0;
		unsigned int indexToBinary=0;
		int layoutType=LAYOUT_DATA;
		if(true!=LocateSectorInLayout(curHSG,fileOffset,sectorLength,indexToBinary,layoutType))
		{
			data.clear();
			return data;
		}
		if(LAYOUT_GAP==layoutType || LAYOUT_AUDIO==layoutType)
		{
			memset(data.data()+dataPointer,0,RAW_BYTES_PER_SECTOR);
			dataPointer+=RAW_BYTES_PER_SECTOR;
			continue;
		}

		const auto &bin=binaries[indexToBinary];
		if(MODE1_BYTES_PER_SECTOR==sectorLength)
		{
			memset(data.data()+dataPointer,0,RAW_BYTES_PER_SECTOR);
			if(true!=ReadBinaryBytes(bin,fileOffset,data.data()+dataPointer+4,MODE2_BYTES_PER_SECTOR))
			{
				data.clear();
				return data;
			}
		}
		else if(MODE2_BYTES_PER_SECTOR+16<=sectorLength)
		{
			unsigned char sectorBuf[4096];
			if(sectorLength>sizeof(sectorBuf) || true!=ReadBinaryBytes(bin,fileOffset,sectorBuf,sectorLength))
			{
				data.clear();
				return data;
			}
			memcpy(data.data()+dataPointer,sectorBuf+16,MODE2_BYTES_PER_SECTOR);
		}
		else
		{
			memset(data.data()+dataPointer,0,RAW_BYTES_PER_SECTOR);
		}
		dataPointer+=RAW_BYTES_PER_SECTOR;
	}
	return data;
}

bool DiscImage::LocateSectorInLayout(
    unsigned int HSG,
    uint64_t &fileOffset,
    unsigned int &sectorLength,
    unsigned int &indexToBinary,
    int &layoutType) const
{
	if(0<layout.size())
	{
		for(int i=0; i+1<(int)layout.size(); ++i)
		{
			if(LAYOUT_END==layout[i].layoutType)
			{
				continue;
			}

			unsigned int segEnd=layout[i+1].startHSG;
			if(LAYOUT_END==layout[i+1].layoutType)
			{
				segEnd=layout[i].startHSG+layout[i].numSectors;
			}
			if(HSG<layout[i].startHSG || segEnd<=HSG)
			{
				continue;
			}

			fileOffset=layout[i].locationInFile+static_cast<uint64_t>(layout[i].sectorLength)*(HSG-layout[i].startHSG);
			sectorLength=layout[i].sectorLength;
			indexToBinary=layout[i].indexToBinary;
			layoutType=layout[i].layoutType;
			return true;
		}
	}

	if(0<tracks.size())
	{
		for(unsigned int i=0; i<tracks.size(); ++i)
		{
			if(tracks[i].trackType!=TRACK_MODE1_DATA && tracks[i].trackType!=TRACK_MODE2_DATA)
			{
				continue;
			}
			const unsigned int startHSG=tracks[i].start.ToHSG();
			const unsigned int endHSG=tracks[i].end.ToHSG();
			if(startHSG<=HSG && HSG<=endHSG)
			{
				fileOffset=tracks[i].locationInFile+static_cast<uint64_t>(tracks[i].sectorLength)*(HSG-startHSG);
				sectorLength=tracks[i].sectorLength;
				indexToBinary=(1==binaries.size() ? 0 : i);
				layoutType=LAYOUT_DATA;
				return true;
			}
		}
	}
	return false;
}

int DiscImage::GetTrackFromMSF(MinSecFrm MSF) const
{
	if(0<tracks.size())
	{
		int tLow=0,tHigh=(int)tracks.size()-1;
		while(tLow<tHigh)
		{
			auto tMid=tLow+(tHigh-tLow)/2;
			if(MSF<tracks[tMid].start)
			{
				tHigh=tMid;
			}
			else if(tracks[tMid].end<MSF)
			{
				if(tMid==tLow)
				{
					++tLow;
				}
				else
				{
					tLow=tMid;
				}
			}
			else
			{
				return tMid+1;
			}
		}
		return tLow+1;
	}
	return -1;
}

std::vector <unsigned char> DiscImage::GetWave(MinSecFrm startMSF,MinSecFrm endMSF) const
{
	std::vector <unsigned char> wave;
	if(0==tracks.size() || !(startMSF<endMSF) || layout.size()<2)
	{
		return wave;
	}

	const unsigned int startHSG=startMSF.ToHSG();
	const unsigned int endHSG=endMSF.ToHSG();
	if(endHSG<=startHSG)
	{
		return wave;
	}

	for(int i=0; i+1<(int)layout.size(); ++i)
	{
		if(LAYOUT_END==layout[i].layoutType)
		{
			continue;
		}

		unsigned int segStartHSG=layout[i].startHSG;
		unsigned int segEndHSG=layout[i+1].startHSG;
		if(LAYOUT_END==layout[i+1].layoutType)
		{
			segEndHSG=segStartHSG+layout[i].numSectors;
		}

		if(endHSG<=segStartHSG || startHSG>=segEndHSG)
		{
			continue;
		}

		const unsigned int clipStartHSG=(startHSG>segStartHSG ? startHSG : segStartHSG);
		const unsigned int clipEndHSG=(endHSG<segEndHSG ? endHSG : segEndHSG);
		if(clipEndHSG<=clipStartHSG)
		{
			continue;
		}

		const auto layoutType=layout[i].layoutType;
		const auto layoutSectorLength=layout[i].sectorLength;
		const auto &bin=binaries[layout[i].indexToBinary];

		const uint64_t readFrom=layout[i].locationInFile
		    +static_cast<uint64_t>(layoutSectorLength)*(clipStartHSG-segStartHSG);
		const uint64_t readTo=layout[i].locationInFile
		    +static_cast<uint64_t>(layoutSectorLength)*(clipEndHSG-segStartHSG);
		if(readFrom>=readTo)
		{
			continue;
		}

		if(layoutSectorLength<=AUDIO_SECTOR_SIZE)
		{
			uint64_t readSize=(readTo-readFrom)&(~3ULL);
			if(0==readSize || readSize>256u*1024u*1024u)
			{
				continue;
			}
			const size_t curSize=wave.size();
			if(curSize+static_cast<size_t>(readSize)<curSize)
			{
				continue;
			}
			wave.resize(curSize+static_cast<size_t>(readSize));
			memset(wave.data()+curSize,0,static_cast<size_t>(readSize));
			if(LAYOUT_AUDIO==layoutType)
			{
				const uint64_t binOffset=readFrom-bin.byteOffsetInDisc+bin.bytesToSkip;
				if(true==ReadBinaryBytes(bin,binOffset,wave.data()+curSize,static_cast<size_t>(readSize)))
				{
					ApplyChdAudioByteSwap(wave.data()+curSize,static_cast<size_t>(readSize));
				}
			}
		}
		else
		{
			const uint64_t numFrames=(readTo-readFrom)/layoutSectorLength;
			uint64_t readSize=numFrames*AUDIO_SECTOR_SIZE;
			readSize&=(~3ULL);
			if(0==readSize || readSize>256u*1024u*1024u)
			{
				continue;
			}
			const size_t curPos=wave.size();
			if(curPos+static_cast<size_t>(readSize)<curPos)
			{
				continue;
			}
			wave.resize(curPos+static_cast<size_t>(readSize));
			memset(wave.data()+curPos,0,static_cast<size_t>(readSize));

			if(LAYOUT_AUDIO==layoutType)
			{
				const uint64_t base_offset=readFrom-bin.byteOffsetInDisc+bin.bytesToSkip;
				size_t out_pos=curPos;
				for(uint64_t filePos=readFrom; filePos<readTo; filePos+=layoutSectorLength)
				{
					if(out_pos+AUDIO_SECTOR_SIZE>wave.size())
					{
						break;
					}
					const uint64_t sector_offset=base_offset+(filePos-readFrom);
					if(true!=ReadBinaryBytes(bin,sector_offset,wave.data()+out_pos,AUDIO_SECTOR_SIZE))
					{
						break;
					}
					out_pos+=AUDIO_SECTOR_SIZE;
				}
				if(out_pos>curPos)
				{
					ApplyChdAudioByteSwap(wave.data()+curPos,out_pos-curPos);
				}
			}
		}
	}

	return wave;
}

DiscImage::TrackTime DiscImage::DiscTimeToTrackTime(MinSecFrm discMSF) const
{
	TrackTime trackTime;
	for(int i=0; i<tracks.size(); ++i)
	{
		if(tracks[i].start<=discMSF && discMSF<=tracks[i].end)
		{
			trackTime.track=i+1;
			trackTime.MSF=discMSF-tracks[i].start;
			break;
		}
	}
	return trackTime;
}

/* static */ bool DiscImage::StrToMSF(MinSecFrm &msf,const char str[])
{
	msf.min=cpputil::Atoi(str);
	str=cpputil::StrSkip(str,":");
	if(nullptr==str)
	{
		return false;
	}
	msf.sec=cpputil::Atoi(str);
	str=cpputil::StrSkip(str,":");
	if(nullptr==str)
	{
		return false;
	}
	msf.frm=cpputil::Atoi(str);
	return true;
}

namespace
{
unsigned int Fnv1a32Update(unsigned int h,unsigned char b)
{
	h^=b;
	h*=16777619u;
	return h;
}

unsigned int Fnv1a32Finish(const std::string &s)
{
	unsigned int h=2166136261u;
	for(unsigned char b : s)
	{
		h=Fnv1a32Update(h,b);
	}
	return h;
}

unsigned int FirstDataTrackBaseHSG(const DiscImage &disc)
{
	for(const auto &trk : disc.GetTracks())
	{
		if(DiscImage::TRACK_MODE1_DATA==trk.trackType ||
		   DiscImage::TRACK_MODE2_DATA==trk.trackType)
		{
			DiscImage::MinSecFrm msf=trk.start;
			msf.Add(trk.preGap);
			return msf.ToHSG();
		}
	}
	return 0;
}

bool IsIso9660PrimaryVolumeDescriptor(const std::vector<unsigned char> &sec)
{
	if(DiscImage::MODE1_BYTES_PER_SECTOR!=sec.size())
	{
		return false;
	}
	if(1!=sec[0])
	{
		return false;
	}
	return 0==memcmp(sec.data()+1,"CD001",5) && 1==sec[6];
}

unsigned int ReadLe32(const unsigned char *p)
{
	return (unsigned int)p[0]
	     | ((unsigned int)p[1]<<8)
	     | ((unsigned int)p[2]<<16)
	     | ((unsigned int)p[3]<<24);
}

std::string NormalizeIso9660Name(const char *name,size_t len)
{
	if(nullptr==name || 0==len)
	{
		return std::string();
	}
	// Special directory entries "." / ".."
	if(1==len && (0==name[0] || 1==name[0]))
	{
		return std::string();
	}
	std::string out;
	out.reserve(len);
	for(size_t i=0; i<len; ++i)
	{
		unsigned char c=(unsigned char)name[i];
		if(';'==c)
		{
			break; // strip ;version
		}
		if('.'==c && i+1==len)
		{
			break; // trailing "." before version is sometimes present alone
		}
		if(c>='a' && c<='z')
		{
			c=(unsigned char)(c-'a'+'A');
		}
		out.push_back((char)c);
	}
	while(true!=out.empty() && ('.'==out.back() || ' '==out.back()))
	{
		out.pop_back();
	}
	return out;
}
}

std::string DiscImage::TrimIso9660Field(const char *buf,size_t len)
{
	if(nullptr==buf || 0==len)
	{
		return std::string();
	}
	size_t end=len;
	while(0<end && (' '==buf[end-1] || 0==buf[end-1]))
	{
		--end;
	}
	return std::string(buf,end);
}

std::vector<unsigned char> DiscImage::ReadUserData2048(unsigned int HSG) const
{
	{
		const auto mode1=ReadSectorMODE1(HSG,1);
		if(DiscImage::MODE1_BYTES_PER_SECTOR==mode1.size())
		{
			return mode1;
		}
	}
	{
		const auto raw=ReadSectorRAW(HSG,1);
		if(DiscImage::AUDIO_SECTOR_SIZE<=raw.size())
		{
			// Mode2 Form1 user data.
			return std::vector<unsigned char>(raw.begin()+24,raw.begin()+24+MODE1_BYTES_PER_SECTOR);
		}
		if(DiscImage::MODE1_BYTES_PER_SECTOR+16<=raw.size())
		{
			return std::vector<unsigned char>(raw.begin()+16,raw.begin()+16+MODE1_BYTES_PER_SECTOR);
		}
	}
	return std::vector<unsigned char>();
}

std::vector<std::string> DiscImage::CollectIso9660RootNames(
    const std::vector<unsigned char> &pvdSec,unsigned int pvdHSG) const
{
	std::vector<std::string> names;
	if(DiscImage::MODE1_BYTES_PER_SECTOR!=pvdSec.size() || pvdHSG<16u)
	{
		return names;
	}
	// Root directory record starts at PVD offset 156.
	const unsigned char *rootRec=pvdSec.data()+156;
	if(34>rootRec[0])
	{
		return names;
	}
	const unsigned int rootLba=ReadLe32(rootRec+2);
	const unsigned int rootBytes=ReadLe32(rootRec+10);
	if(0==rootLba || 0==rootBytes)
	{
		return names;
	}
	// Map ISO LBA → disc HSG using the PVD's known ISO LBA (16).
	const unsigned int hsgOfLba0=pvdHSG-16u;
	const unsigned int numSec=(rootBytes+MODE1_BYTES_PER_SECTOR-1u)/MODE1_BYTES_PER_SECTOR;
	constexpr unsigned int kMaxRootSectors=32; // enough for typical Towns roots
	const unsigned int readSec=std::min(numSec,kMaxRootSectors);
	for(unsigned int i=0; i<readSec; ++i)
	{
		const auto sec=ReadUserData2048(hsgOfLba0+rootLba+i);
		if(DiscImage::MODE1_BYTES_PER_SECTOR!=sec.size())
		{
			break;
		}
		size_t off=0;
		while(off+33<=sec.size())
		{
			const unsigned char recLen=sec[off];
			if(0==recLen)
			{
				// Records don't cross sector boundaries; pad to next sector.
				break;
			}
			if(off+recLen>sec.size() || recLen<34)
			{
				break;
			}
			const unsigned char nameLen=sec[off+32];
			if(0<nameLen && off+33u+nameLen<=sec.size())
			{
				const std::string nm=NormalizeIso9660Name(
				    (const char *)sec.data()+off+33,nameLen);
				if(true!=nm.empty())
				{
					names.push_back(nm);
				}
			}
			off+=recLen;
		}
	}
	std::sort(names.begin(),names.end());
	names.erase(std::unique(names.begin(),names.end()),names.end());
	return names;
}

namespace
{
std::string UpperAsciiCopy(std::string s)
{
	for(char &c : s)
	{
		if(c>='a' && c<='z')
		{
			c=(char)(c-'a'+'A');
		}
	}
	return s;
}

std::vector<std::string> SplitIsoRelPath(const std::string &relPath)
{
	std::vector<std::string> parts;
	std::string cur;
	for(char c : relPath)
	{
		if('\\'==c || '/'==c)
		{
			if(true!=cur.empty())
			{
				parts.push_back(cur);
				cur.clear();
			}
			continue;
		}
		cur.push_back(c);
	}
	if(true!=cur.empty())
	{
		parts.push_back(cur);
	}
	return parts;
}

struct IsoDirEnt
{
	std::string name;
	unsigned int lba=0;
	unsigned int bytes=0;
	bool isDir=false;
};
}

// ReadUserData2048 is private — use it from member functions only.
bool DiscImage::FindIso9660File(
    const std::string &relPath,
    unsigned int &hsgOfLba0,
    unsigned int &fileLba,
    unsigned int &fileBytes) const
{
	hsgOfLba0=0;
	fileLba=0;
	fileBytes=0;
	if(true==relPath.empty() ||
	   DiscImage::FILETYPE_NONE==fileType || 0==GetNumSectors())
	{
		return false;
	}
	std::string wantPath=UpperAsciiCopy(relPath);
	for(char &c : wantPath)
	{
		if('/'==c)
		{
			c='\\';
		}
	}
	while(true!=wantPath.empty() &&
	      (wantPath.size()>=2 && (('A'<=wantPath[0] && wantPath[0]<='Z') ||
	                             ('0'<=wantPath[0] && wantPath[0]<='9')) &&
	       ':'==wantPath[1]))
	{
		// Strip DOS drive prefix (Q:\...).
		wantPath.erase(0,2);
		while(true!=wantPath.empty() && ('\\'==wantPath.front() || '/'==wantPath.front()))
		{
			wantPath.erase(wantPath.begin());
		}
	}
	while(true!=wantPath.empty() && ('\\'==wantPath.front() || '/'==wantPath.front()))
	{
		wantPath.erase(wantPath.begin());
	}
	const auto parts=SplitIsoRelPath(wantPath);
	if(true==parts.empty())
	{
		return false;
	}

	const unsigned int dataBase=FirstDataTrackBaseHSG(*this);
	const unsigned int scanBases[]={
		dataBase,
		HSG_BASE,
		0u,
	};
	std::vector<unsigned char> pvdSec;
	unsigned int pvdHSG=0;
	for(const unsigned int base : scanBases)
	{
		for(unsigned int lba=0; lba<=64; ++lba)
		{
			const unsigned int hsg=base+lba;
			const auto sec=ReadUserData2048(hsg);
			if(true!=IsIso9660PrimaryVolumeDescriptor(sec))
			{
				continue;
			}
			pvdSec=sec;
			pvdHSG=hsg;
			break;
		}
		if(true!=pvdSec.empty())
		{
			break;
		}
	}
	if(true==pvdSec.empty() || pvdHSG<16u)
	{
		return false;
	}
	const unsigned char *rootRec=pvdSec.data()+156;
	if(34>rootRec[0])
	{
		return false;
	}
	unsigned int curLba=ReadLe32(rootRec+2);
	unsigned int curBytes=ReadLe32(rootRec+10);
	if(0==curLba || 0==curBytes)
	{
		return false;
	}
	hsgOfLba0=pvdHSG-16u;

	auto listDir=[&](unsigned int dirLba,unsigned int dirBytes,
	                 std::vector<IsoDirEnt> &out)->bool
	{
		out.clear();
		constexpr unsigned int kMaxDirSectors=64;
		const unsigned int numSec=(dirBytes+MODE1_BYTES_PER_SECTOR-1u)/MODE1_BYTES_PER_SECTOR;
		const unsigned int readSec=std::min(numSec,kMaxDirSectors);
		for(unsigned int i=0; i<readSec; ++i)
		{
			const auto sec=ReadUserData2048(hsgOfLba0+dirLba+i);
			if(DiscImage::MODE1_BYTES_PER_SECTOR!=sec.size())
			{
				break;
			}
			size_t off=0;
			while(off+33<=sec.size())
			{
				const unsigned char recLen=sec[off];
				if(0==recLen)
				{
					break;
				}
				if(off+recLen>sec.size() || recLen<34)
				{
					break;
				}
				const unsigned char flags=sec[off+25];
				const unsigned char nameLen=sec[off+32];
				if(0<nameLen && off+33u+nameLen<=sec.size())
				{
					const std::string nm=NormalizeIso9660Name(
					    (const char *)sec.data()+off+33,nameLen);
					if(true!=nm.empty())
					{
						IsoDirEnt ent;
						ent.name=nm;
						ent.lba=ReadLe32(sec.data()+off+2);
						ent.bytes=ReadLe32(sec.data()+off+10);
						ent.isDir=(0!=(flags&0x02));
						out.push_back(ent);
					}
				}
				off+=recLen;
			}
		}
		return true!=out.empty();
	};

	// Path with directories: walk components.
	if(1<parts.size())
	{
		for(size_t pi=0; pi+1<parts.size(); ++pi)
		{
			std::vector<IsoDirEnt> ents;
			if(true!=listDir(curLba,curBytes,ents))
			{
				return false;
			}
			bool foundDir=false;
			for(const auto &ent : ents)
			{
				if(true==ent.isDir && ent.name==parts[pi])
				{
					curLba=ent.lba;
					curBytes=ent.bytes;
					foundDir=true;
					break;
				}
			}
			if(true!=foundDir)
			{
				return false;
			}
		}
		std::vector<IsoDirEnt> ents;
		if(true!=listDir(curLba,curBytes,ents))
		{
			return false;
		}
		for(const auto &ent : ents)
		{
			if(true!=ent.isDir && ent.name==parts.back())
			{
				fileLba=ent.lba;
				fileBytes=ent.bytes;
				return 0!=fileLba && 0!=fileBytes;
			}
		}
		return false;
	}

	// Basename-only: BFS search (depth-limited). Ambiguous → fail.
	const std::string &want=parts[0];
	struct DirQ
	{
		unsigned int lba;
		unsigned int bytes;
		unsigned int depth;
	};
	std::vector<DirQ> queue;
	queue.push_back({curLba,curBytes,0});
	unsigned int foundLba=0,foundBytes=0;
	unsigned int hitCount=0;
	constexpr unsigned int kMaxDepth=6;
	constexpr unsigned int kMaxDirs=64;
	unsigned int visited=0;
	for(size_t qi=0; qi<queue.size() && visited<kMaxDirs; ++qi)
	{
		const DirQ cur=queue[qi];
		++visited;
		std::vector<IsoDirEnt> ents;
		if(true!=listDir(cur.lba,cur.bytes,ents))
		{
			continue;
		}
		for(const auto &ent : ents)
		{
			if(true==ent.isDir)
			{
				if(cur.depth<kMaxDepth)
				{
					queue.push_back({ent.lba,ent.bytes,cur.depth+1});
				}
				continue;
			}
			if(ent.name==want)
			{
				++hitCount;
				foundLba=ent.lba;
				foundBytes=ent.bytes;
			}
		}
	}
	if(1!=hitCount)
	{
		// 0 = missing; >1 = ambiguous (e.g. launcher + game same basename).
		return false;
	}
	fileLba=foundLba;
	fileBytes=foundBytes;
	return 0!=fileLba && 0!=fileBytes;
}

unsigned int DiscImage::HashIso9660FilePrefix(
    const std::string &relPath,unsigned int maxBytes) const
{
	if(true==relPath.empty() || 0==maxBytes)
	{
		return 0;
	}
	unsigned int hsgOfLba0=0,fileLba=0,fileBytes=0;
	if(true!=FindIso9660File(relPath,hsgOfLba0,fileLba,fileBytes))
	{
		return 0;
	}
	const unsigned int toHash=std::min(fileBytes,maxBytes);
	const unsigned int nSec=(toHash+MODE1_BYTES_PER_SECTOR-1u)/MODE1_BYTES_PER_SECTOR;
	unsigned int h=2166136261u;
	unsigned int remaining=toHash;
	for(unsigned int i=0; i<nSec && 0<remaining; ++i)
	{
		const auto sec=ReadUserData2048(hsgOfLba0+fileLba+i);
		if(DiscImage::MODE1_BYTES_PER_SECTOR!=sec.size())
		{
			return 0;
		}
		const unsigned int n=std::min(remaining,(unsigned int)sec.size());
		for(unsigned int b=0; b<n; ++b)
		{
			h=Fnv1a32Update(h,sec[b]);
		}
		remaining-=n;
	}
	return h;
}

unsigned int DiscImage::ComputeTocHash32(void) const
{
	std::ostringstream oss;
	oss << "v1|ft=" << fileType << "|nt=" << GetNumTracks() << "|ns=" << GetNumSectors();
	int trackNum=1;
	for(const auto &trk : tracks)
	{
		oss << "|T" << trackNum++
		    << ":ty=" << trk.trackType
		    << ",sl=" << trk.sectorLength
		    << ",psl=" << trk.preGapSectorLength
		    << ",s=" << trk.start.Encode()
		    << ",e=" << trk.end.Encode()
		    << ",pg=" << trk.preGap.Encode()
		    << ",i0=" << trk.index00.Encode();
	}
	return Fnv1a32Finish(oss.str());
}

DiscIdentity DiscImage::ComputeIdentity(bool allowDiscIO) const
{
	if(true==identityCached_)
	{
		return cachedIdentity_;
	}
	DiscIdentity id;
	if(DiscImage::FILETYPE_NONE==fileType || 0==GetNumSectors())
	{
		cachedIdentity_=id;
		identityCached_=true;
		return id;
	}
	id.valid=true;
	id.numTracks=GetNumTracks();
	id.numSectors=GetNumSectors();
	for(const auto &trk : tracks)
	{
		if(TRACK_AUDIO==trk.trackType)
		{
			++id.numAudioTracks;
		}
		else if(TRACK_MODE1_DATA==trk.trackType || TRACK_MODE2_DATA==trk.trackType)
		{
			++id.numDataTracks;
		}
	}
	id.tocHash32=ComputeTocHash32();
	{
		char hex[16];
		snprintf(hex,sizeof(hex),"%08x",id.tocHash32);
		id.tocHashHex=hex;
	}

	if(true!=allowDiscIO)
	{
		// Do not cache a TOC-only stub — a later full scan should replace it.
		return id;
	}

	const unsigned int dataBase=FirstDataTrackBaseHSG(*this);
	const unsigned int scanBases[]={
		dataBase,
		HSG_BASE,
		0u,
	};
	std::vector<unsigned char> pvdSec;
	for(const unsigned int base : scanBases)
	{
		for(unsigned int lba=0; lba<=64; ++lba)
		{
			const unsigned int hsg=base+lba;
			const auto sec=ReadUserData2048(hsg);
			if(true!=IsIso9660PrimaryVolumeDescriptor(sec))
			{
				continue;
			}
			id.hasIso9660=true;
			id.pvdSectorHSG=hsg;
			id.systemIdentifier=TrimIso9660Field(
			    (const char *)sec.data()+8,32);
			id.volumeLabel=TrimIso9660Field(
			    (const char *)sec.data()+40,32);
			if(true!=id.volumeLabel.empty())
			{
				id.contentKey=id.volumeLabel+"|"+id.systemIdentifier;
				id.contentHash32=Fnv1a32Finish(id.contentKey);
				char hex[16];
				snprintf(hex,sizeof(hex),"%08x",id.contentHash32);
				id.contentHashHex=hex;
				id.hasContentId=true;
			}
			pvdSec=sec;
			break;
		}
		if(true==id.hasIso9660)
		{
			break;
		}
	}

	if(true==id.hasIso9660 && true!=pvdSec.empty())
	{
		std::vector<std::string> rootNames=CollectIso9660RootNames(pvdSec,id.pvdSectorHSG);
		unsigned int rootHash32=0;
		if(true!=rootNames.empty())
		{
			std::ostringstream rootOss;
			for(size_t i=0; i<rootNames.size(); ++i)
			{
				if(0!=i)
				{
					rootOss << '|';
				}
				rootOss << rootNames[i];
			}
			rootHash32=Fnv1a32Finish(rootOss.str());
		}

		std::ostringstream oss;
		oss << "v1|na=" << id.numAudioTracks
		    << "|nd=" << id.numDataTracks
		    << "|rh=" << rootHash32;
		if(0!=id.numAudioTracks || 0!=id.numDataTracks || 0!=rootHash32)
		{
			id.fingerprintHash32=Fnv1a32Finish(oss.str());
			char hex[16];
			snprintf(hex,sizeof(hex),"%08x",id.fingerprintHash32);
			id.fingerprintHashHex=hex;
			id.hasFingerprint=true;
		}
	}
	else if(0!=id.numAudioTracks || 0!=id.numDataTracks)
	{
		std::ostringstream oss;
		oss << "v1|na=" << id.numAudioTracks
		    << "|nd=" << id.numDataTracks
		    << "|rh=0";
		id.fingerprintHash32=Fnv1a32Finish(oss.str());
		char hex[16];
		snprintf(hex,sizeof(hex),"%08x",id.fingerprintHash32);
		id.fingerprintHashHex=hex;
		id.hasFingerprint=true;
	}

	cachedIdentity_=id;
	identityCached_=true;
	return id;
}
