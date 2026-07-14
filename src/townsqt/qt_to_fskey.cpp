#include "qt_to_fskey.h"

#include <Qt>

int QtKeyToFsKey(int qtKey,unsigned int qtModifiers)
{
	if(0!=(qtModifiers&Qt::KeypadModifier))
	{
		switch(qtKey)
		{
		case Qt::Key_0: return FSKEY_TEN0;
		case Qt::Key_1: return FSKEY_TEN1;
		case Qt::Key_2: return FSKEY_TEN2;
		case Qt::Key_3: return FSKEY_TEN3;
		case Qt::Key_4: return FSKEY_TEN4;
		case Qt::Key_5: return FSKEY_TEN5;
		case Qt::Key_6: return FSKEY_TEN6;
		case Qt::Key_7: return FSKEY_TEN7;
		case Qt::Key_8: return FSKEY_TEN8;
		case Qt::Key_9: return FSKEY_TEN9;
		case Qt::Key_Period: return FSKEY_TENDOT;
		case Qt::Key_Plus: return FSKEY_TENPLUS;
		case Qt::Key_Minus: return FSKEY_TENMINUS;
		case Qt::Key_Asterisk: return FSKEY_TENSTAR;
		case Qt::Key_Slash: return FSKEY_TENSLASH;
		case Qt::Key_Enter:
		case Qt::Key_Return: return FSKEY_TENENTER;
		default: break;
		}
	}

	switch(qtKey)
	{
	case Qt::Key_Escape: return FSKEY_ESC;
	case Qt::Key_Tab: return FSKEY_TAB;
	case Qt::Key_Backspace: return FSKEY_BS;
	case Qt::Key_Return:
	case Qt::Key_Enter: return FSKEY_ENTER;
	case Qt::Key_Space: return FSKEY_SPACE;
	case Qt::Key_0: return FSKEY_0;
	case Qt::Key_1: return FSKEY_1;
	case Qt::Key_2: return FSKEY_2;
	case Qt::Key_3: return FSKEY_3;
	case Qt::Key_4: return FSKEY_4;
	case Qt::Key_5: return FSKEY_5;
	case Qt::Key_6: return FSKEY_6;
	case Qt::Key_7: return FSKEY_7;
	case Qt::Key_8: return FSKEY_8;
	case Qt::Key_9: return FSKEY_9;
	case Qt::Key_A: return FSKEY_A;
	case Qt::Key_B: return FSKEY_B;
	case Qt::Key_C: return FSKEY_C;
	case Qt::Key_D: return FSKEY_D;
	case Qt::Key_E: return FSKEY_E;
	case Qt::Key_F: return FSKEY_F;
	case Qt::Key_G: return FSKEY_G;
	case Qt::Key_H: return FSKEY_H;
	case Qt::Key_I: return FSKEY_I;
	case Qt::Key_J: return FSKEY_J;
	case Qt::Key_K: return FSKEY_K;
	case Qt::Key_L: return FSKEY_L;
	case Qt::Key_M: return FSKEY_M;
	case Qt::Key_N: return FSKEY_N;
	case Qt::Key_O: return FSKEY_O;
	case Qt::Key_P: return FSKEY_P;
	case Qt::Key_Q: return FSKEY_Q;
	case Qt::Key_R: return FSKEY_R;
	case Qt::Key_S: return FSKEY_S;
	case Qt::Key_T: return FSKEY_T;
	case Qt::Key_U: return FSKEY_U;
	case Qt::Key_V: return FSKEY_V;
	case Qt::Key_W: return FSKEY_W;
	case Qt::Key_X: return FSKEY_X;
	case Qt::Key_Y: return FSKEY_Y;
	case Qt::Key_Z: return FSKEY_Z;
	case Qt::Key_F1: return FSKEY_F1;
	case Qt::Key_F2: return FSKEY_F2;
	case Qt::Key_F3: return FSKEY_F3;
	case Qt::Key_F4: return FSKEY_F4;
	case Qt::Key_F5: return FSKEY_F5;
	case Qt::Key_F6: return FSKEY_F6;
	case Qt::Key_F7: return FSKEY_F7;
	case Qt::Key_F8: return FSKEY_F8;
	case Qt::Key_F9: return FSKEY_F9;
	case Qt::Key_F10: return FSKEY_F10;
	case Qt::Key_F11: return FSKEY_F11;
	case Qt::Key_F12: return FSKEY_F12;
	case Qt::Key_Insert: return FSKEY_INS;
	case Qt::Key_Delete: return FSKEY_DEL;
	case Qt::Key_Home: return FSKEY_HOME;
	case Qt::Key_End: return FSKEY_END;
	case Qt::Key_PageUp: return FSKEY_PAGEUP;
	case Qt::Key_PageDown: return FSKEY_PAGEDOWN;
	case Qt::Key_Up: return FSKEY_UP;
	case Qt::Key_Down: return FSKEY_DOWN;
	case Qt::Key_Left: return FSKEY_LEFT;
	case Qt::Key_Right: return FSKEY_RIGHT;
	case Qt::Key_Shift: return FSKEY_SHIFT;
	case Qt::Key_Control: return FSKEY_CTRL;
	case Qt::Key_Alt: return FSKEY_ALT;
	case Qt::Key_CapsLock: return FSKEY_CAPSLOCK;
	case Qt::Key_NumLock: return FSKEY_NUMLOCK;
	case Qt::Key_ScrollLock: return FSKEY_SCROLLLOCK;
	case Qt::Key_Pause: return FSKEY_PAUSEBREAK;
	case Qt::Key_AsciiTilde:
	case Qt::Key_QuoteLeft: return FSKEY_TILDA;
	case Qt::Key_Minus: return FSKEY_MINUS;
	case Qt::Key_Equal:
	case Qt::Key_Plus: return FSKEY_PLUS;
	case Qt::Key_BracketLeft: return FSKEY_LBRACKET;
	case Qt::Key_BracketRight: return FSKEY_RBRACKET;
	case Qt::Key_Backslash: return FSKEY_BACKSLASH;
	case Qt::Key_Semicolon: return FSKEY_SEMICOLON;
	case Qt::Key_Apostrophe: return FSKEY_SINGLEQUOTE;
	case Qt::Key_Comma: return FSKEY_COMMA;
	case Qt::Key_Period: return FSKEY_DOT;
	case Qt::Key_Slash: return FSKEY_SLASH;
	case Qt::Key_Print: return FSKEY_PRINTSCRN;
	default: return FSKEY_NULL;
	}
}
