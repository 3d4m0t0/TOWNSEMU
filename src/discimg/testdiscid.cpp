#include "discimg.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

static bool WriteTestIso(const char *path)
{
	std::vector<unsigned char> iso(17u*2048u,0);
	unsigned char *pvd=iso.data()+16u*2048u;
	pvd[0]=1;
	memcpy(pvd+1,"CD001",5);
	pvd[6]=1;
	memcpy(pvd+8,"TOWNS_TEST_SYSTEM               ",32);
	memcpy(pvd+40,"MY_VOLUME_LABEL                 ",32);

	std::ofstream ofp(path,std::ios::binary);
	if(true!=ofp.is_open())
	{
		return false;
	}
	ofp.write((const char *)iso.data(),(std::streamsize)iso.size());
	return true;
}

int main(int ac,char *av[])
{
	const char *path=(ac>=2 ? av[1] : "testdiscid.iso");
	if(true!=WriteTestIso(path))
	{
		fprintf(stderr,"Cannot write test ISO.\n");
		return 1;
	}

	DiscImage disc;
	if(DiscImage::ERROR_NOERROR!=disc.Open(path))
	{
		fprintf(stderr,"Cannot open test ISO.\n");
		return 1;
	}

	const DiscIdentity id=disc.ComputeIdentity();
	if(true!=id.valid)
	{
		fprintf(stderr,"DiscIdentity not valid.\n");
		return 1;
	}
	if(true!=id.hasIso9660)
	{
		fprintf(stderr,"ISO9660 PVD not found.\n");
		return 1;
	}
	if("MY_VOLUME_LABEL"!=id.volumeLabel)
	{
		fprintf(stderr,"Volume label mismatch: [%s]\n",id.volumeLabel.c_str());
		return 1;
	}
	if(true!=id.hasContentId)
	{
		fprintf(stderr,"Content id missing.\n");
		return 1;
	}
	if("MY_VOLUME_LABEL|TOWNS_TEST_SYSTEM"!=id.contentKey)
	{
		fprintf(stderr,"Content key mismatch: [%s]\n",id.contentKey.c_str());
		return 1;
	}
	if("TOWNS_TEST_SYSTEM"!=id.systemIdentifier)
	{
		fprintf(stderr,"System id mismatch: [%s]\n",id.systemIdentifier.c_str());
		return 1;
	}
	if(0==id.tocHash32 || true==id.tocHashHex.empty())
	{
		fprintf(stderr,"TOC hash missing.\n");
		return 1;
	}
	if(true!=id.hasFingerprint || 0==id.fingerprintHash32)
	{
		fprintf(stderr,"Fingerprint missing.\n");
		return 1;
	}
	// Synthetic ISO is a single data track with empty root listing.
	if(0!=id.numAudioTracks || 1!=id.numDataTracks)
	{
		fprintf(stderr,"Track counts unexpected: audio=%u data=%u\n",
		        id.numAudioTracks,id.numDataTracks);
		return 1;
	}

	printf("volume_label=%s\n",id.volumeLabel.c_str());
	printf("system_id=%s\n",id.systemIdentifier.c_str());
	printf("content_key=%s\n",id.contentKey.c_str());
	printf("content_hash=%s\n",id.contentHashHex.c_str());
	printf("fingerprint_hash=%s\n",id.fingerprintHashHex.c_str());
	printf("audio_tracks=%u data_tracks=%u\n",
	       id.numAudioTracks,id.numDataTracks);
	printf("toc_hash=%s\n",id.tocHashHex.c_str());
	printf("pvd_hsg=%u\n",id.pvdSectorHSG);
	printf("tracks=%u sectors=%u\n",id.numTracks,id.numSectors);
	return 0;
}
