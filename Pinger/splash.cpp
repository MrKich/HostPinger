#include "splash.h"

WNDCLASSEX	SplashClass;
HWND		SplashParent;
HINSTANCE	SplashInst;

void InitSplash( HWND hWndParent, HINSTANCE hInstance ) {
	SplashInst				   = hInstance;
	SplashParent			   = hWndParent;

	SplashClass.cbSize		   = sizeof(WNDCLASSEX);
    SplashClass.style          = CS_HREDRAW | CS_VREDRAW;
    SplashClass.lpfnWndProc    = SplashProc;
    SplashClass.cbClsExtra     = 0;
    SplashClass.cbWndExtra     = 0;
    SplashClass.hInstance      = hInstance;
    SplashClass.hIcon          = NULL;
    SplashClass.hCursor        = NULL;
	SplashClass.hbrBackground  = CreateSolidBrush( SPLASH_BK_COLOR_ON );
    SplashClass.lpszMenuName   = NULL;
    SplashClass.lpszClassName  = SPLASH_CLASS_ON;
    SplashClass.hIconSm        = NULL;

	RegisterClassEx( &SplashClass );
	SplashClass.lpszClassName  = SPLASH_CLASS_OFF;
	SplashClass.hbrBackground  = CreateSolidBrush( SPLASH_BK_COLOR_OFF );
	RegisterClassEx( &SplashClass );
	SplashClass.lpszClassName  = SPLASH_CLASS_UNS;
	SplashClass.hbrBackground  = CreateSolidBrush( SPLASH_BK_COLOR_UNS );
	RegisterClassEx( &SplashClass );
}

void CreateSplash( char *IpAndStatus, GLOBAL_STATUS Status ) {
	RECT rect;
	const char *detect_text, *class_text;
	char buf[ IP_SIZE + 2 ];

	strcpy( buf, IpAndStatus );
	switch( Status ) {
	case ONLINE:
		detect_text = SPLASH_STATUS_ONLINE;
		class_text = SPLASH_CLASS_ON;
		break;
	case OFFLINE:
		detect_text = SPLASH_STATUS_OFFLINE;
		class_text = SPLASH_CLASS_OFF;
		break;
	case UNSTABLE:
		detect_text = SPLASH_STATUS_UNSTABLE;
		class_text = SPLASH_CLASS_UNS;
		break;
	}

	strcat( buf, detect_text );

	SystemParametersInfo( SPI_GETWORKAREA, 0, &rect, 0 );
	HWND hWndSplash = CreateWindowEx( WS_EX_TOPMOST | WS_EX_LAYERED,
		class_text, buf, 
		WS_POPUP | WS_CHILD,
		rect.right - SPLASH_WIDTH, rect.bottom - SPLASH_HEIGHT, 
		SPLASH_WIDTH, SPLASH_HEIGHT,
		SplashParent, NULL, SplashInst, NULL );
	SetLayeredWindowAttributes( hWndSplash, 0, SPLASH_ALPHA, LWA_ALPHA);

	SetTimer( hWndSplash, 0, SPLASH_INTERVAL, NULL );

	ShowWindow( hWndSplash, SW_NORMAL );
	UpdateWindow( hWndSplash );
}

LRESULT WINAPI CALLBACK SplashProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) {
	HDC hdc;
	PAINTSTRUCT ps;
	HFONT font;
	LOGFONT font_data;
	RECT rect;
	char buf[IP_SIZE + 1];
	const char *statusText;

	switch( message ) {
	case WM_TIMER:
		DestroyWindow( hWnd );
		break;
	case WM_PAINT:
		GetWindowText( hWnd, buf, IP_SIZE + 1 );
		GetClientRect( hWnd, &rect );	// FIX: Maybe rect in wParam/lParam - Check MSDN
		
		font_data.lfCharSet			= RUSSIAN_CHARSET;
		font_data.lfClipPrecision	= CLIP_DEFAULT_PRECIS;
		font_data.lfEscapement		= 0;
		font_data.lfHeight			= SPLASH_FONT_SIZE;
		font_data.lfItalic			= 0;
		font_data.lfOrientation		= 0;
		font_data.lfOutPrecision	= OUT_DEFAULT_PRECIS;
		font_data.lfPitchAndFamily	= DEFAULT_PITCH | FF_ROMAN;
		font_data.lfQuality			= DEFAULT_QUALITY;
		font_data.lfStrikeOut		= FALSE;
		font_data.lfUnderline		= FALSE;
		font_data.lfWeight			= FW_BOLD;
		font_data.lfWidth			= 0;
		memcpy( font_data.lfFaceName, "Times New Roman", sizeof( "Times New Roman" ) );

		font = CreateFontIndirect( &font_data );
		hdc = BeginPaint( hWnd, &ps );
			SetTextColor( hdc, 0 );
			if( buf[strlen(buf) - 1] == SPLASH_STATUS_ONLINE[0] ) {
				statusText = SPLASH_ONLINE;
			} else if( buf[strlen(buf) - 1] == SPLASH_STATUS_OFFLINE[0]) {
				statusText = SPLASH_OFFLINE;
			} else {
				statusText = SPLASH_UNSTABLE;
			}
			buf[strlen(buf) - 1] = '\0';

			SetBkMode( hdc,TRANSPARENT );
			SelectObject( hdc, font );
			DrawText( hdc, buf, -1, &rect, DT_SINGLELINE | DT_NOCLIP | DT_CENTER ) ;
			DeleteObject( font );

			rect.top += SPLASH_HEIGHT / 2 + 5;
			font_data.lfHeight -= 8;
			font = CreateFontIndirect( &font_data );
			SelectObject( hdc, font );
			DrawText( hdc, statusText, -1, &rect, DT_SINGLELINE | DT_NOCLIP | DT_CENTER );
			DeleteObject( font );
		EndPaint( hWnd, &ps );
		break;
	default:
		return DefWindowProc( hWnd, message, wParam, lParam );
		break;
	}

	return 0;
}