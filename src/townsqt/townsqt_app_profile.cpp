#include "townsqt_app_profile.h"

#include "townsdef.h"

#include <QString>

namespace
{
const TownsQtAppProfileEntry kEntries[]={
    {
        TOWNS_APPSPECIFIC_NONE,
        "NONE",
        "なし",
        "タイトル別の補正を行いません",
    },
    {
        TOWNS_APPSPECIFIC_WINGCOMMANDER1,
        "WINGCOMMANDER1",
        "Wing Commander",
        "標準マウス BIOS ではなく独自ルーチンでマウスを読むため、マウス統合を有効化します。"
        "メニュー操作が可能になります。フライト操作には -FLIGHTMOUSE 等との併用も有効です。",
    },
    {
        TOWNS_APPSPECIFIC_WINGCOMMANDER2,
        "WINGCOMMANDER2",
        "Wing Commander II",
        "Wing Commander と同様のマウス統合に加え、イベントキュー連携で操作感を補正します。",
    },
    {
        TOWNS_APPSPECIFIC_STRIKECOMMANDER,
        "STRIKECOMMANDER",
        "Strike Commander",
        "マウス統合を有効化します。本タイトルはメモリ 8 MB 以上を推奨します（選択時に不足していれば 8 MB に引き上げます）。",
    },
    {
        TOWNS_APPSPECIFIC_SUPERDAISEN,
        "SUPERDAISEN",
        "スーパー大戦略",
        "マウス左ボタン押下中だけ CPU を 4 MHz に落とし、マップが押しっぱなしで高速スクロールする問題を抑えます。",
    },
    {
        TOWNS_APPSPECIFIC_LEMMINGS,
        "LEMMINGS",
        "レミングス",
        "マウス統合と座標補正を行います。VRAM オフセットは考慮しません。",
    },
    {
        TOWNS_APPSPECIFIC_LEMMINGS2,
        "LEMMINGS2",
        "レミングス2",
        "マウス統合・座標補正に加え、CPU を 20 MHz に固定し CD を 1 倍速読み取り、描画タイミングも調整します。",
    },
    {
        TOWNS_APPSPECIFIC_AMARANTH3,
        "AMARANTH3",
        "勇拝三期",
        "マウス統合を有効化します（VRAM オフセットは考慮しません）。",
    },
    {
        TOWNS_APPSPECIFIC_ULTIMAUNDERWORLD,
        "ULTIMAUNDERWORLD",
        "Ultima Underworld",
        "マウス統合を有効化し、Y 座標の変換を行います。",
    },
    {
        TOWNS_APPSPECIFIC_OPERATIONWOLF,
        "OPERATIONWOLF",
        "Operation Wolf",
        "マウス統合を有効化します。",
    },
    {
        TOWNS_APPSPECIFIC_BRANDISH,
        "BRANDISH",
        "ブランディッシュ",
        "80486 を 80386SX と誤認する問題への対策として PRETEND386DX を有効にします。",
    },
    {
        TOWNS_APPSPECIFIC_AIRWARRIOR_V2,
        "AIRWARRIORV2",
        "エアウォリアー V2.1",
        "ジョイスティック・スロットル・ラダー用のメモリ位置を追跡します。-FLIGHTTHR 等との併用を想定しています。",
    },
    {
        TOWNS_APPSPECIFIC_DUNGEONMASTER_JP,
        "DUNGEONMASTER_JP",
        "ダンジョンマスター (日本語)",
        "PF キーと数字キーの対応を補正し、キーボード操作を改善します。",
    },
    {
        TOWNS_APPSPECIFIC_DUNGEONMASTER_EN,
        "DUNGEONMASTER_EN",
        "Dungeon Master (英語)",
        "英語版向けの PF キー／数字キー対応補正です。",
    },
    {
        TOWNS_APPSPECIFIC_DAIKOUKAIJIDAI,
        "DAIKOUKAIJIDAI",
        "大航海時代",
        "マウス統合と VRAM オフセット考慮、各種ダイアログ座標の連携を行います。",
    },
    {
        TOWNS_APPSPECIFIC_DAIKOUKAIJIDAI2,
        "DAIKOUKAIJIDAI2",
        "大航海時代 II",
        "キー入力の引き継ぎ、マップ座標、スクリーンショット補正、マウスカーソル表示の改善などを行います。",
    },
    {
        TOWNS_APPSPECIFIC_RASHINBAN,
        "RASHINBAN",
        "羅針盤",
        "マウス統合を有効化し、ゲームポートのマウス割り当てを調整します。",
    },
    {
        TOWNS_APPSPECIFIC_AFTERBURNER2,
        "AFTERBURNER2",
        "After Burner II",
        "タイトル識別用の内部補正（AB2_Identify）を有効にします。",
    },
    {
        TOWNS_APPSPECIFIC_ORGEL,
        "ORGEL",
        "ORGEL",
        "CD-ROM の読み取りタイミングを調整します。",
    },
    {
        TOWNS_APPSPECIFIC_ROCKETRANGER,
        "ROCKETRANGER",
        "Rocket Ranger",
        "オープニング映像と音楽の同期のため CPU を低速化します。離陸関連の内部変数も追跡します。",
    },
    {
        TOWNS_APPSPECIFIC_ASUKA120,
        "ASUKA120",
        "飛鳥120％",
        "ゲームポートに割り当てられたマウスを解除します。",
    },
    {
        TOWNS_APPSPECIFIC_DRAKKEN,
        "DRAKKEN",
        "Drakken",
        "マウス Y 座標を補正します（VRAM オフセットは考慮しません）。",
    },
};
}

int TownsQtAppProfileCount()
{
	return static_cast<int>(sizeof(kEntries)/sizeof(kEntries[0]));
}

const TownsQtAppProfileEntry &TownsQtAppProfileAt(int index)
{
	static const TownsQtAppProfileEntry kFallback={
	    TOWNS_APPSPECIFIC_NONE,"NONE","なし","タイトル別の補正を行いません"};
	if(index<0 || index>=TownsQtAppProfileCount())
	{
		return kFallback;
	}
	return kEntries[index];
}

int TownsQtAppProfileDefaultIndex()
{
	return 0;
}

int TownsQtAppProfileIndexForApp(unsigned int app_value)
{
	for(int i=0; i<TownsQtAppProfileCount(); ++i)
	{
		if(kEntries[i].app_value==app_value)
		{
			return i;
		}
	}
	return TownsQtAppProfileDefaultIndex();
}

unsigned int TownsQtAppProfileApp(int index)
{
	return TownsQtAppProfileAt(index).app_value;
}

QString TownsQtAppProfileDescription(int index)
{
	const TownsQtAppProfileEntry &entry=TownsQtAppProfileAt(index);
	return QString::fromUtf8(entry.description).toHtmlEscaped();
}

QString TownsQtAppProfileId(int index)
{
	return QString::fromLatin1(TownsQtAppProfileAt(index).id);
}

int TownsQtAppProfileIndexForId(const QString &id)
{
	const QString normalized=id.trimmed().toUpper();
	for(int i=0; i<TownsQtAppProfileCount(); ++i)
	{
		if(normalized==QString::fromLatin1(kEntries[i].id))
		{
			return i;
		}
	}
	return TownsQtAppProfileDefaultIndex();
}
