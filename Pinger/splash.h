#pragma once
#include <Windows.h>

const char		SPLASH_CLASS_ON[]		= "Pinger_Splash_On";
const char		SPLASH_CLASS_OFF[]		= "Pinger_Splash_Off";
const char		SPLASH_CLASS_UNS[]		= "Pinger_Splash_Uns";
const char		SPLASH_ONLINE[]			= "Поднялся!";
const char		SPLASH_OFFLINE[]		= "Упал!";
const char		SPLASH_UNSTABLE[]		= "Нестабилен!";
const char		SPLASH_STATUS_UNSTABLE[]= "\x42";
const char		SPLASH_STATUS_ONLINE[]	= "\x41";
const char		SPLASH_STATUS_OFFLINE[]	= "\x40";
const int		SPLASH_WIDTH			= 200;
const int		SPLASH_HEIGHT			= 75;
const int		SPLASH_FONT_SIZE		= 34;
const int		SPLASH_INTERVAL			= 5000;
const int		SPLASH_ALPHA_PERCENT	= 85;
const int		SPLASH_ALPHA			= ( 255 * SPLASH_ALPHA_PERCENT ) / 100;
const COLORREF	SPLASH_BK_COLOR_ON		= RGB( 128, 255, 128 );
const COLORREF	SPLASH_BK_COLOR_UNS		= RGB( 255, 255, 128 );
const COLORREF	SPLASH_BK_COLOR_OFF		= RGB( 255, 128, 128 );
const int		IP_SIZE					= 3*4 + 3 + 1 + 1;
const COLORREF	COLOR_GREEN				= RGB( 32, 255, 32 );
const COLORREF	COLOR_RED				= RGB( 255, 32, 32 );
const COLORREF	COLOR_YELLOW			= RGB( 255, 255, 32 );

enum GLOBAL_STATUS {
	ONLINE,
	OFFLINE,
	UNSTABLE,
	SMALL_LAG
};

void InitSplash( HWND hWndParent, HINSTANCE hInstance );
void CreateSplash( char *IpStatus, GLOBAL_STATUS Status );
LRESULT WINAPI CALLBACK SplashProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
//void DestroySplash();