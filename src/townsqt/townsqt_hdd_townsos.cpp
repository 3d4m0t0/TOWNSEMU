#include "townsqt_hdd_townsos.h"

#include "cpputil.h"

#include <QFile>

namespace
{
constexpr unsigned long long kImageBytes=
    static_cast<unsigned long long>(kTownsOsHddSizeMb)*1024ull*1024ull;
// Partition VBR at 0x600; FAT1 after one reserved 2048-byte FS sector.
constexpr qint64 kPartitionHostOffset=0x600;
constexpr qint64 kFsSectorBytes=2048;
constexpr qint64 kFatBytes=32*kFsSectorBytes;
constexpr qint64 kFat1Offset=kPartitionHostOffset+kFsSectorBytes;
constexpr qint64 kFat2Offset=kFat1Offset+kFatBytes;
constexpr char kFatMediaSig[4]={'\xfa','\xff','\xff','\xff'};

QByteArray LoadTemplate(const char *alias)
{
	QFile f(QStringLiteral(":/hdd_templates/%1").arg(QLatin1String(alias)));
	if(true!=f.open(QIODevice::ReadOnly))
	{
		return {};
	}
	return f.readAll();
}

bool WriteAll(QFile &out,qint64 offset,const char *data,qint64 len)
{
	if(true!=out.seek(offset))
	{
		return false;
	}
	return len==out.write(data,len);
}
}

bool CreateTownsOsFormattedHdd127Mb(const QString &path)
{
	const QByteArray ipl=LoadTemplate("ipl.bin");
	const QByteArray vbr=LoadTemplate("vbr.bin");
	if(ipl.size()!=0x600 || vbr.size()!=0x800)
	{
		return false;
	}

	if(true!=cpputil::CreateSparseBinaryFile(path.toStdString(),kImageBytes))
	{
		return false;
	}

	// Sparse file is already zero-filled: only write IPL, VBR, and FAT media signatures.
	QFile out(path);
	if(true!=out.open(QIODevice::ReadWrite))
	{
		return false;
	}
	if(true!=WriteAll(out,0,ipl.constData(),ipl.size()) ||
	   true!=WriteAll(out,kPartitionHostOffset,vbr.constData(),vbr.size()) ||
	   true!=WriteAll(out,kFat1Offset,kFatMediaSig,4) ||
	   true!=WriteAll(out,kFat2Offset,kFatMediaSig,4))
	{
		return false;
	}
	return true;
}
