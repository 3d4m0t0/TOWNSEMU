#include "menubar_connection.h"

#include <GL/gl.h>

#include "fssimplewindow.h"

namespace
{
struct MenuItem
{
	int x0,x1;
	const char *cmd;
};

const MenuItem MENU_ITEMS[]=
{
	{0,72,"RUN"},
	{72,144,"PAUSE"},
	{144,216,"POFF"},
	{216,300,"QUIT"},
};
}

void MenuBarFsSimpleWindowConnection::DrawMenuBar(unsigned winWid,unsigned menuBarHei)
{
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glColor3ub(42,42,48);
	glVertex2i(0,0);
	glVertex2i(winWid,0);
	glVertex2i(winWid,menuBarHei);
	glVertex2i(0,menuBarHei);
	glEnd();

	glColor3ub(70,70,78);
	for(const auto &item : MENU_ITEMS)
	{
		glBegin(GL_QUADS);
		glVertex2i(item.x0,2);
		glVertex2i(item.x1,2);
		glVertex2i(item.x1,menuBarHei-2);
		glVertex2i(item.x0,menuBarHei-2);
		glEnd();
	}

	glColor3ub(120,120,130);
	for(const auto &item : MENU_ITEMS)
	{
		glBegin(GL_LINES);
		glVertex2i(item.x1,0);
		glVertex2i(item.x1,menuBarHei);
		glEnd();
	}
	glColor3f(1,1,1);
	glEnable(GL_TEXTURE_2D);
}

std::string MenuBarFsSimpleWindowConnection::MenuCommandForClick(int mx,int my,unsigned menuBarHei)
{
	if(my<0 || (unsigned)my>=menuBarHei)
	{
		return "";
	}
	for(const auto &item : MENU_ITEMS)
	{
		if(item.x0<=mx && mx<item.x1)
		{
			return item.cmd;
		}
	}
	return "";
}

void MenuBarFsSimpleWindowConnection::MenuBarWindowConnection::Start(void)
{
	// Keep menuBarHei=0 so window layout matches Tsugaru_CUI (480px content + status bar).
	// The menu bar is drawn as a top overlay; do not shrink or offset the FM TOWNS framebuffer.
	WindowConnection::Start();
}

void MenuBarFsSimpleWindowConnection::MenuBarWindowConnection::Render(bool swapBuffers)
{
	WindowConnection::Render(false);

	int winWid=0,winHei=0;
	FsGetWindowSize(winWid,winHei);
	if(menuBarHei>0)
	{
		DrawMenuBar(winWid,menuBarHei);
	}

	if(true==swapBuffers)
	{
		FsSwapBuffers();
	}
}

Outside_World::WindowInterface *MenuBarFsSimpleWindowConnection::CreateWindowInterface(void) const
{
	return new MenuBarWindowConnection;
}

void MenuBarFsSimpleWindowConnection::DevicePolling(class FMTownsCommon &towns)
{
	std::vector <FsSimpleWindowConnection::MouseEvent> menuClicks;

	for(auto it=windowEvent.mouseEvents.begin(); it!=windowEvent.mouseEvents.end();)
	{
		const auto &mos=*it;
		if(MENU_BAR_HEIGHT>0 &&
		   FSMOUSEEVENT_LBUTTONDOWN==mos.evt &&
		   mos.my<(int)MENU_BAR_HEIGHT)
		{
			menuClicks.push_back(mos);
			it=windowEvent.mouseEvents.erase(it);
		}
		else
		{
			++it;
		}
	}

	for(const auto &mos : menuClicks)
	{
		const auto cmd=MenuCommandForClick(mos.mx,mos.my,MENU_BAR_HEIGHT);
		if(""!=cmd)
		{
			commandQueue.push(cmd);
		}
	}

	FsSimpleWindowConnection::DevicePolling(towns);
}
