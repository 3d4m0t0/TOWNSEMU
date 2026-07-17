/* TownsUI — menu bar + embedded FM TOWNS screen (derivative of Tsugaru / TOWNSEMU) */
#ifndef MENUBAR_CONNECTION_IS_INCLUDED
#define MENUBAR_CONNECTION_IS_INCLUDED

#include "fssimplewindow_connection.h"

class MenuBarFsSimpleWindowConnection : public FsSimpleWindowConnection
{
public:
	static constexpr unsigned MENU_BAR_HEIGHT=28;

	class MenuBarWindowConnection : public FsSimpleWindowConnection::WindowConnection
	{
	public:
		void Start(void) override;
		void Render(bool swapBuffers) override;
	};

	Outside_World::WindowInterface *CreateWindowInterface(void) const override;

	void DevicePolling(class FMTownsCommon &towns) override;

private:
	static void DrawMenuBar(unsigned winWid,unsigned menuBarHei);
	static std::string MenuCommandForClick(int mx,int my,unsigned menuBarHei);
};

#endif
