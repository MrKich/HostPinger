#include <Windows.h>
#include <winsock.h>
#include <iphlpapi.h>

#define PIO_APC_ROUTINE_DEFINED
#define WM_ADDNEW WM_USER + 1

typedef LONG NTSTATUS;

typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID    Pointer;
  };
  ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef
VOID
(WINAPI *PIO_APC_ROUTINE) (
    IN PVOID ApcContext,
    IN PIO_STATUS_BLOCK IoStatusBlock,
    IN ULONG Reserved
    );

#include <icmpapi.h>
#include <CommCtrl.h>
#include <list>

#include "resource.h"
#include "splash.h"
#include "PingStatistics.h"

using namespace std;

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Comctl32.lib")

#define ARRAY_SIZE( x ) ( sizeof( x ) / sizeof( x[0] ) )

const char			DATA_BUFFER[]			= "Ping? Pong!";
const char			SHOW_IP_URL[]			= "https://billing.line-r.ru/LA/ip/ip.php?seg=";
const char			PING_IP_PARAMS[]		= "/K ping -t ";
const int			PING_ONLINE_SLEEP_MAX	= 6000;
const int			PING_WAIT_ONLINE		= 2000;
const int			PING_WAIT_OFFLINE		= 5000;
const int			TIMER_INTERVAL			= 1000;
const int			PING_INTERVAL			= 10;
const int			IP_FIELD_WIDTH			= 100;
const int			IP_FIELD_HEIGHT			= 20;
const int			IPS_PER_ROW				= 4;
const int			PING_COUNTS				= 50;
const int			DETECT_UNSTABLE_PERC	= 50;
const int			FAILS_SKIP_PERC			= 20;
const int			DEFAULT_OFFLINE_SENS	= 2;
const int			IP_GROUP_COUNT			= 50;
const int			UNSTABLE_SLEEP			= 2000;
const int			TOP_BUTTONS_OFFSET		= 24;
const int			TOP_BUTTONS_PADDING		= 8;
const int			TOP_BUTTONS_COUNT		= 8;
const int			TOP_PINGIP_WIDTH		= 128;
const COLORREF		LIST_COLOR_ON			= RGB( 200, 255, 200 );
const COLORREF		LIST_COLOR_OFF			= RGB( 255, 200, 200 );
const COLORREF		LIST_COLOR_UNS			= RGB( 255, 255, 200 );
const COLORREF		LIST_COLOR_UNK			= RGB( 150, 150, 150 );

#pragma pack(push)
#pragma pack(1)
struct Buffer {
	ICMP_ECHO_REPLY Head;
	BYTE			Data[sizeof( DATA_BUFFER )];
	IO_STATUS_BLOCK	Status;
};
#pragma pack(pop)

struct TimeStruct {
	short Hours;
	short Minutes;
	short Seconds;
};

class DownTime {
public:
	static const int BUF_SIZE = 9;
	DownTime() {
		Reset();
	};
	void ParseString() {
		Time.Hours = atoi( TimeStr );
		Time.Minutes = atoi( &TimeStr[3] );
		Time.Seconds = atoi( &TimeStr[6] );
	}
	void IncreaseTimeBy( short Seconds ) {
		Time.Seconds += Seconds;
		if( Time.Seconds > 59 ) {
			Time.Seconds = 60 - Time.Seconds;
			Time.Minutes += 1;
		}
		if( Time.Minutes > 59 ) {
			Time.Minutes = 0;
			Time.Hours += 1;
		}
	}
	void DecreaseTimeBy( short Seconds ) {
		Time.Seconds -= Seconds;
		if( Time.Seconds < 0 ) {
			Time.Seconds = 60 + Time.Seconds;
			Time.Minutes -= 1;
		}
		if( Time.Minutes < 0 ) {
			Time.Minutes = 59;
			Time.Hours -= 1;
		}
	}
	char* GetBuf() {
		return TimeStr;
	}
	void Reset() {
		Time.Hours = 0;
		Time.Minutes = 0;
		Time.Seconds = 0;
	}
	char* MakeString() {
		if( Time.Hours < 10 ) {
			TimeStr[0] = '0';
			_itoa( Time.Hours, &TimeStr[1], 10 );
		} else {
			_itoa( Time.Hours, TimeStr, 10 );
		}
		TimeStr[2] = ':';
		if( Time.Minutes < 10 ) { 
			TimeStr[3] = '0';
			_itoa( Time.Minutes, &TimeStr[4], 10 );
		} else {
			_itoa( Time.Minutes, &TimeStr[3], 10 );
		}
		TimeStr[5] = ':';
		if( Time.Seconds < 10 ) {
			TimeStr[6] = '0';
			_itoa( Time.Seconds, &TimeStr[7], 10 );
		} else {
			_itoa( Time.Seconds, &TimeStr[6], 10 );
		}
		return TimeStr;
	}
	bool IsZero( void ) {
		return Time.Hours == 0 && Time.Minutes == 0 && Time.Seconds == 0;
	}
private:
	TimeStruct			Time;
	char				TimeStr[BUF_SIZE];
};

struct IpStatus {
	GLOBAL_STATUS		Status;
	char				Seg[4];
	char				Hid[4];
	char				Addr[64];
	int					Sens;
	DownTime			DT;
	PingStatistics		*PS;
	IpStatus() {
		Status = ONLINE;
		PS = new PingStatistics( PING_COUNTS );
		Sens = 6;//DEFAULT_OFFLINE_SENS;
		Seg[0] = '\0';
		Hid[0] = '\0';
		Addr[0] = '\0';
	}
};

const char		CLASS_NAME[]					= "Pinger";
const int		LIST_SIZE[]						= { 20, 100, 52, 70, 175, 36 };
const int		LIST_COLUMN_NUM					= sizeof( LIST_SIZE ) / sizeof( LIST_SIZE[0] );
const char		LIST_TITLE[LIST_COLUMN_NUM][5]	= { "U", "Ip", "Seg", "Down", "Addr", "Sens" };
const LONG		LIST_FMT[LIST_COLUMN_NUM]		= { LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_CENTER, LVCFMT_CENTER,
													LVCFMT_LEFT, LVCFMT_CENTER };

enum		LIST_ENUM {
	COL_CHECK = 0,
	COL_IP,
	COL_SEG,
	COL_DOWN,
	COL_ADDR,
	COL_SENS,
	COL_COMM
};

volatile bool Stopped = false;

int			IpNumber		= 0;
char		(*IpText)[IP_SIZE];
HANDLE		hIcmpFile;
ULONG		*Ip;
Buffer		*Data;
IpStatus	*Status;
//HWND		*hIps;
HWND		hWndMain, hLV, hTopPingIp, hTopButtons[TOP_BUTTONS_COUNT];
LIST_ENUM	SortField = COL_DOWN;
bool		SortInverted = false;

int CALLBACK SortItemsInColumn( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort ) {
	int i1 = 0, i2 = 0;
	int iSeg1, iSeg2;

	if( Status[lParam1].Status == UNSTABLE ) {
		if( Status[lParam2].Status != UNSTABLE ) {
			return 1;
		}
	} else {
		if( Status[lParam2].Status == UNSTABLE ) {
			return -1;
		}
	}

	if( Status[lParam1].Status == ONLINE ) {
		if( Status[lParam2].Status != ONLINE ) {
			return -1;
		}
	} else {
		if( Status[lParam2].Status == ONLINE ) {
			return 1;
		}
	}

	if( Status[lParam1].Status == SMALL_LAG ) {
		if( Status[lParam2].Status != SMALL_LAG ) {
			return -1;
		}
	} else {
		if( Status[lParam2].Status == SMALL_LAG ) {
			return 1;
		}
	}

	if( SortInverted ) {	// Флаг отвечает за перевернутую сортировку (просто меняем переменные местами)
		lParam1 ^= lParam2;
		lParam2 = lParam1 ^ lParam2;
		lParam1 ^= lParam2;
	}

	//switch( (LIST_ENUM)lParamSort ) {
	switch( SortField ) {
	case COL_IP:
		for( int i = 0; i < 4; i++ ) {
			iSeg1 = atoi( &IpText[lParam1][i1] );
			iSeg2 = atoi( &IpText[lParam2][i2] );

			if( iSeg1 > iSeg2 ) {
				return 1;
			} else if ( iSeg1 < iSeg2 ) {
				return -1;
			}

			while( IpText[lParam1][i1++] != '.' && IpText[lParam1][i1] != '\0' );
			while( IpText[lParam2][i2++] != '.' && IpText[lParam2][i2] != '\0' );
		}
		break;
	case COL_SEG:
		return strcmp( Status[lParam1].Seg, Status[lParam2].Seg );
		break;
	case COL_DOWN:
		return strcmp( Status[lParam1].DT.MakeString(), Status[lParam2].DT.MakeString() );
		break;
	case COL_ADDR:
		return strcmp( Status[lParam1].Addr, Status[lParam2].Addr );
		break;
	}
	return 0;
}

int FindItemById( int Id ) {
	LV_FINDINFO lvfi;
	lvfi.flags = LVFI_PARAM;
	lvfi.lParam = Id;
	return ListView_FindItem( hLV, -1, &lvfi );
}

void AddItemToList( int IpId, bool checked = false ) {
	int iNum;

	if( ( iNum = FindItemById( IpId ) ) == -1 ) {
		LV_ITEM lvi;
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		lvi.iSubItem = 0;
		lvi.pszText = "";
		lvi.lParam = IpId;
		lvi.iItem = IpNumber;
		iNum = ListView_InsertItem( hLV, &lvi );
		ListView_SetItemText( hLV, iNum, COL_IP, IpText[IpId] );

		ListView_SetItemText( hLV, iNum, COL_SEG, Status[IpId].Seg );

		ListView_SetItemText( hLV, iNum, COL_ADDR, Status[IpId].Addr );

		char sens[4];
		itoa( Status[IpId].Sens, sens, 10 );
		ListView_SetItemText(hLV, iNum, COL_SENS, sens );
	}

	Status[IpId].DT.Reset();
	ListView_SetItemText( hLV, iNum, COL_DOWN, Status[IpId].DT.MakeString() );
	if( checked ) {
		ListView_SetCheckState( hLV, iNum, 2 );
	}
}

void DeleteItem( int Id ) {
	int id = FindItemById(Id);
	if( id != -1 )
		ListView_DeleteItem( hLV, id );
}

void UpdatePercentage( int id ) {
	char perc[4];
	perc[0] = '\0';
	LV_FINDINFO lvf;
	lvf.flags = LVFI_PARAM;
	lvf.lParam = id;
	int f = ListView_FindItem( hLV, 0, &lvf );
	_itoa( Status[id].PS->GetPerc(), perc, 10 );
	ListView_SetItemText( hLV, f, COL_DOWN, perc );
}

void UpdateView( int id, GLOBAL_STATUS GStatus ) {
	int lId = FindItemById( id );
	switch( GStatus ) {
	case ONLINE:
		Status[id].DT.Reset();
		Status[id].DT.IncreaseTimeBy( 60 );
		//DeleteItem( id );
		break;
	case OFFLINE:
		AddItemToList( id );
		break;
	case UNSTABLE:
		AddItemToList( id, true );
		break;
	case SMALL_LAG:
		AddItemToList( id, false );
	}
	ListView_SortItems( hLV, SortItemsInColumn, NULL );
	//ListView_RedrawItems( hLV, lId, lId );
}

DWORD WINAPI CheckOnlineThread( LPVOID lParam ) {
	static bool first_cycle = true;
	int start_ip_num = (int)(lParam);
	int end_ip_num = (start_ip_num + IP_GROUP_COUNT <= IpNumber ? start_ip_num + IP_GROUP_COUNT : IpNumber);
	int sleep_time = PING_ONLINE_SLEEP_MAX;

	for( int i = start_ip_num; i < end_ip_num; i++ ) {
		if( Status[i].Status == ONLINE ) {
			IcmpSendEcho2( hIcmpFile, NULL, NULL, NULL, Ip[i], (LPVOID)DATA_BUFFER,
						   sizeof( DATA_BUFFER ), NULL, &Data[i], sizeof( Buffer ), PING_WAIT_ONLINE );
			Status[i].PS->Update( Data[i].Head.Status );

			if( Data[i].Head.Status != IP_SUCCESS ) {
				sleep_time -= PING_WAIT_ONLINE;
				if( Status[i].PS->GetOfflineStrike() >= DEFAULT_OFFLINE_SENS ) {
					Status[i].Status = SMALL_LAG;
					SendMessage( hWndMain, WM_ADDNEW, i, true );	
				}
			}
		}
		if( Stopped ) break;
		if( i == end_ip_num - 1 ) {
			if( sleep_time > 0 )
				Sleep( sleep_time );
			sleep_time = PING_ONLINE_SLEEP_MAX;
			i = start_ip_num - 1;
			
			if( first_cycle ) first_cycle = false;
		}
	}
	return 0;
}

DWORD WINAPI CheckSmallLagThread( LPVOID lParam ) {
	int i = (int)lParam;
	bool need_reporting = false;

	while( Status[i].Status != ONLINE ) {
		IcmpSendEcho2( hIcmpFile, NULL, NULL, NULL, Ip[i], (LPVOID)DATA_BUFFER,
			sizeof(DATA_BUFFER), NULL, &Data[i], sizeof(Buffer), PING_WAIT_ONLINE );
		Status[i].PS->Update( Data[i].Head.Status );
		switch( Status[i].Status ) {
		case UNSTABLE:
			break;
		case SMALL_LAG:
			if( Status[i].PS->GetOfflineStrike() >= Status[i].Sens ) {
				Status[i].Status = OFFLINE;
				SendMessage( hWndMain, WM_ADDNEW, i, need_reporting ? false : true );
				need_reporting = true;
				break;
			}
			/*if( Status[i].PS->GetPerc() > DETECT_UNSTABLE_PERC && Status[i].PS->IsFull() ) {
				Status[i].Status = UNSTABLE;
				SendMessage( hWndMain, WM_ADDNEW, i, true );
			}*/
		case OFFLINE:
			if( Status[i].PS->GetOnlineStrike() >= Status[i].Sens ) {
				if( !need_reporting ) {
					DeleteItem( i );
					Status[i].Status = ONLINE;
					break;
				} else {
					Status[i].Status = ONLINE;
					SendMessage( hWndMain, WM_ADDNEW, i, true );
					break;
				}
			}
			if( Data[i].Head.Status == IP_SUCCESS && Status[i].Status != SMALL_LAG ) {
				Status[i].Status = SMALL_LAG;
				UpdateView( i, SMALL_LAG );
			}
		}

		if( Data[i].Head.Status != IP_REQ_TIMED_OUT ) {
			Sleep( UNSTABLE_SLEEP );
		}
	}
	string dbg_str = "Destroyed thread for ";
	dbg_str += IpText[i];
	dbg_str += "\n";
	OutputDebugString( dbg_str.c_str() );
	return 0;
}

LRESULT CALLBACK ListViewProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
							   UINT_PTR uIdSubclass, DWORD_PTR dwRefData ) {
	char sens[4];
	LV_ITEM lvi;
	lvi.mask = LVIF_PARAM;

	switch( uMsg ) {
	case WM_CHAR:
		if( wParam > '0' && wParam <= '9' ) {
			int iPos = ListView_GetNextItem( hLV, -1, LVNI_SELECTED );
			while( iPos != -1 ) {
				lvi.iItem = iPos;
				lvi.iSubItem = 0;
				ListView_GetItem( hLV, &lvi );
				
				Status[(int)(lvi.lParam)].Sens = wParam - '0';
				itoa( Status[(int)(lvi.lParam)].Sens, sens, 10 );
				ListView_SetItemText( hLV, iPos, COL_SENS, sens );
				iPos = ListView_GetNextItem( hLV, iPos, LVNI_SELECTED );
			}
			return 0;
		}
	break;
	}
	return DefSubclassProc( hWnd, uMsg, wParam, lParam );
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) {
	LV_ITEM lvi;
	static int tmr_count;
	LPNMLISTVIEW pnmv;
	LPNMLVCUSTOMDRAW lplvcd;
	FILE *fSave;
	string dbg_str = "Created thread for ";

    switch( message ) {
	case WM_ADDNEW:
		UpdateView( (int)wParam, Status[(int)wParam].Status );
		if ((bool)lParam && Status[(int)wParam].Status != SMALL_LAG )
			CreateSplash( IpText[(int)wParam], Status[(int)wParam].Status );
		switch( Status[(int)wParam].Status ) {
		case ONLINE:
			PlaySound( (LPCTSTR)SND_ALIAS_SYSTEMDEFAULT, NULL, SND_ASYNC | SND_ALIAS_ID );
			break;
		case OFFLINE:
			if( (bool)lParam ) {
				PlaySound( (LPCTSTR)SND_ALIAS_SYSTEMHAND, NULL, SND_ASYNC | SND_ALIAS_ID );
			}
			//CreateThread( NULL, 0, CheckOfflineThread, (LPVOID)wParam, 0, NULL );
			break;
		case UNSTABLE:
			PlaySound( (LPCTSTR)SND_ALIAS_SYSTEMHAND, NULL, SND_ASYNC | SND_ALIAS_ID );
			//CreateThread( NULL, 0, CheckUnstableThread, (LPVOID)wParam, 0, NULL );
			break;
		case SMALL_LAG:
			dbg_str += IpText[(int)wParam];
			dbg_str += "\n";
			OutputDebugString( dbg_str.c_str() );
			CreateThread(NULL, 0, CheckSmallLagThread, (LPVOID)wParam, 0, NULL );
			break;
		}
		break;
	case WM_TIMER:
		if( wParam == -1 ) { 
			tmr_count = 0;
		}

		lvi.iSubItem = 0;
		for( int i = 0; i < ListView_GetItemCount( hLV ); i++ ) {
			lvi.mask = LVIF_PARAM;
			lvi.iItem = i;
			ListView_GetItem( hLV, &lvi );
			if( Status[lvi.lParam].Status == ONLINE ) {
				if( Status[lvi.lParam].DT.IsZero() ) {		// Удаляем как только таймер стукнул в ноль
					DeleteItem( lvi.lParam );
				} else {
					Status[lvi.lParam].DT.DecreaseTimeBy( TIMER_INTERVAL / 1000 );
					ListView_SetItemText( hLV, i, COL_DOWN, Status[lvi.lParam].DT.MakeString() );
				}
			} else if ( Status[lvi.lParam].Status != UNSTABLE ) {
				Status[lvi.lParam].DT.IncreaseTimeBy( TIMER_INTERVAL / 1000 );
				ListView_SetItemText( hLV, i, COL_DOWN, Status[lvi.lParam].DT.MakeString() );
			}
		}
		break;
	case WM_NOTIFY:
		switch ( ( (LPNMHDR) lParam )->code ) {
		case LVN_COLUMNCLICK:
			pnmv = (LPNMLISTVIEW) lParam;
			if( SortField == (LIST_ENUM)pnmv->iSubItem ) {
				SortInverted = !SortInverted;
			} else {
				SortInverted = false;
				SortField = (LIST_ENUM)pnmv->iSubItem;
			}
			ListView_SortItems( hLV, SortItemsInColumn, NULL );
			break;
		case LVN_ITEMCHANGING:
			pnmv = (LPNMLISTVIEW) lParam;
			if( ( pnmv->uNewState & LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( 2 ) ) {
				Status[pnmv->lParam].Status = UNSTABLE;
				UpdatePercentage( pnmv->lParam );
				ListView_SortItems( hLV, SortItemsInColumn, NULL );
				//CreateThread( NULL, 0, CheckSmallLagThread, (LPVOID)pnmv->lParam, 0, NULL );
			} else if ( ( pnmv->uNewState & LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( 1 )
						  && ( pnmv->uOldState & LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( 2 ) ) {
				Status[pnmv->lParam].Status = OFFLINE;
				Status[pnmv->lParam].PS->Reset();
				ListView_SetItemText( hLV, pnmv->iItem, COL_DOWN, Status[pnmv->lParam].DT.MakeString() )
				if( pnmv->uOldState != 0 ) ListView_SortItems( hLV, SortItemsInColumn, NULL );	// FIX: Вроде как это костыль.
				//CreateThread( NULL, 0, CheckSmallLagThread, (LPVOID)( pnmv->lParam ), 0, NULL );
			}
			break;

		case NM_CUSTOMDRAW:
			lplvcd = (LPNMLVCUSTOMDRAW)lParam;
			switch( lplvcd->nmcd.dwDrawStage )  {
			case CDDS_PREPAINT:
				return CDRF_NOTIFYITEMDRAW;

			case CDDS_ITEMPREPAINT:
				lplvcd->clrText   = RGB( 0, 0, 0 );
				if( Status[lplvcd->nmcd.lItemlParam].Status == UNSTABLE ) {
						/*lplvcd->clrTextBk = RGB( 
							255 - 100 * ( Status[lplvcd->nmcd.lItemlParam].PS->GetPerc( 0 ) / 100.0f ), 
							255 - 100 * ( Status[lplvcd->nmcd.lItemlParam].PS->GetPerc( 0 ) / 100.0f ),
							255 - 100 * ( Status[lplvcd->nmcd.lItemlParam].PS->GetPerc( 0 ) / 100.0f ) );*/
					lplvcd->clrTextBk = LIST_COLOR_UNS;
				} else if ( Status[lplvcd->nmcd.lItemlParam].Status == OFFLINE ) {
					//lplvcd->clrTextBk = RGB( 255, 255, 255 );
					lplvcd->clrTextBk = LIST_COLOR_OFF;
				} else if ( Status[lplvcd->nmcd.lItemlParam].Status == ONLINE )  {
					lplvcd->clrTextBk = LIST_COLOR_ON;
				} else {
					lplvcd->clrTextBk = LIST_COLOR_UNK;
				}

				return CDRF_NEWFONT;
				break;
			}
			return CDRF_DODEFAULT;
			break;

		case NM_DBLCLK:
			LPNMITEMACTIVATE lpnmitem  = ( LPNMITEMACTIVATE) lParam;
			char params[IP_SIZE + sizeof(PING_IP_PARAMS) - 1];	// В IP_SIZE уже включен последний символ конца строки
			char url[256];
			if( lpnmitem->iItem == -1 ) break;
			switch( lpnmitem->iSubItem ) {
			case COL_IP:
				strcpy( params, PING_IP_PARAMS );
				ListView_GetItemText( hLV, lpnmitem->iItem, lpnmitem->iSubItem, &params[11], sizeof( params ) );
				ShellExecute( NULL, NULL, "cmd.exe", params, NULL, SW_SHOWNORMAL );
				break;
			case COL_SEG:
				strcpy( url, SHOW_IP_URL );
				ListView_GetItemText(hLV, lpnmitem->iItem, lpnmitem->iSubItem, &url[sizeof(SHOW_IP_URL)-1], 256);
				ShellExecute( NULL, "open", url, NULL, NULL, SW_SHOWNORMAL );
			case COL_DOWN:
				lvi.mask = LVIF_PARAM;
				lvi.iItem = lpnmitem->iItem;
				lvi.iSubItem = 0;
				ListView_GetItem( hLV, &lvi );
				Status[lvi.lParam].PS->Reset();
			}
			break;
		}
		break;
    case WM_DESTROY:
		Stopped = true;
#ifndef _DEBUG
		fSave = fopen( "save.txt", "w" );		
		for( int i = 0; i < IpNumber; i++ ) {
			fprintf( fSave, "%s %d %d\n", IpText[i], Status[i].Status == UNSTABLE ? 1 : 0, Status[i].Sens );
		}
		fclose( fSave );
#endif
        PostQuitMessage( 0 );
        break;
    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
        break;
    }

    return 0;
}

int CALLBACK WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = NULL;//CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASS_NAME;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

	if( !RegisterClassEx( &wcex ) ) {
        MessageBox( NULL, "Call to RegisterClassEx failed!", "Error", NULL );
        return -1;
    }

	RECT desk_rect;
	SystemParametersInfo( SPI_GETWORKAREA, 0, &desk_rect, 0 );

	int window_width = 30 + ( IP_FIELD_WIDTH + 10 ) * IPS_PER_ROW;

	hWndMain = CreateWindow(
    CLASS_NAME,
    CLASS_NAME,
	WS_OVERLAPPEDWINDOW &~(WS_THICKFRAME) &~(WS_MAXIMIZEBOX) /*| WS_VSCROLL*/,
    0, 0,
	window_width, desk_rect.bottom,
    NULL,
    NULL,
    hInstance,
    NULL );
	if ( !hWndMain ) {
		MessageBox( NULL, "Call to CreateWindow failed!", "Error", NULL );
		return -1;
	}

	/*for( int i = 0; i < TOP_BUTTONS_COUNT - 1; i++ ) {
		hTopButtons[i] = CreateWindow( WC_BUTTON, "", WS_VISIBLE | WS_CHILD,
									   i * ( TOP_BUTTONS_OFFSET + TOP_BUTTONS_PADDING ) + TOP_BUTTONS_PADDING, 5,
									   TOP_BUTTONS_OFFSET, TOP_BUTTONS_OFFSET, hWndMain, NULL, hInstance, NULL );
	}
	
	hTopButtons[TOP_BUTTONS_COUNT - 1] = CreateWindow( WC_BUTTON, "", WS_VISIBLE | WS_CHILD, 
													   window_width - 10 - TOP_BUTTONS_OFFSET - TOP_PINGIP_WIDTH - TOP_BUTTONS_PADDING,
													   5, TOP_BUTTONS_OFFSET, TOP_BUTTONS_OFFSET,
													   hWndMain, NULL, hInstance, NULL );
	hTopPingIp = CreateWindow( WC_IPADDRESS, "", WS_VISIBLE | WS_CHILD, window_width - 5 - TOP_BUTTONS_PADDING - TOP_PINGIP_WIDTH,
							   5, TOP_PINGIP_WIDTH, TOP_BUTTONS_OFFSET + 1, hWndMain, NULL, hInstance, NULL );*/

	INITCOMMONCONTROLSEX icex;           // Structure for control initialization.
	icex.dwSize = sizeof( icex );
	icex.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex);
	hLV = CreateWindow( WC_LISTVIEW, "", WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_EDITLABELS,
		0, 0 /*TOP_BUTTONS_OFFSET + 10*/, window_width - 5, desk_rect.bottom - 28 /*- TOP_BUTTONS_OFFSET - 10*/,
		hWndMain, NULL, hInstance, NULL );
	ListView_SetExtendedListViewStyle( hLV,
		LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES );

	LV_COLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_TEXT;
	SetWindowSubclass( hLV, ListViewProc, 0, 0 );

	for( int i = 0; i < LIST_COLUMN_NUM; i++ ) {
		lvc.iSubItem = i;
		lvc.pszText = (LPSTR)LIST_TITLE[i];
		lvc.cx = LIST_SIZE[i];
		lvc.fmt = LIST_FMT[i];
		ListView_InsertColumn( hLV, i, &lvc);
	}

	hIcmpFile = IcmpCreateFile();
	if( hIcmpFile == INVALID_HANDLE_VALUE ) return -1;
	
	char ip_buf[IP_SIZE];
	FILE *host_file = fopen( "hosts.txt", "r" );
	while( fgets( ip_buf, IP_SIZE, host_file ) ) {
		IpNumber++;
	}
	fseek( host_file, 0, SEEK_SET );

	Ip			= new ULONG[IpNumber];
	Data		= new Buffer[IpNumber];
	Status		= new IpStatus[IpNumber];
	IpText		= new char[IpNumber][IP_SIZE];
	
	for( int i = 0; i < IpNumber; i++ ) {
		fgets( IpText[i], IP_SIZE, host_file );
		sscanf( IpText[i], "%s", IpText[i] );	// FIX: Костыль! Надо избавиться от одной из команд!
		Ip[i] = inet_addr( IpText[i] );
		if( Ip[i] == INADDR_NONE ) {
			MessageBox( 0, "Invalid Address", IpText[i], 0 );
			return -1;
		}
		//Events[i] = CreateEvent( NULL, false, false, NULL );
		//RegisterWaitForSingleObject( &RegEvents[i], Events[i], PingReceiver, (PVOID)i, INFINITE, WT_EXECUTEDEFAULT );
	}
	fclose( host_file );

	// Считываем второй файл addr.txt и заполняем поля в IpStatus
	FILE* fAddr = fopen( "addr.txt", "r" );
	char buf[256];
	while( fgets( buf, sizeof( buf ), fAddr ) != nullptr ) {
		sscanf( buf, "%s", ip_buf );
		for( int i = 0; i < IpNumber; i++ ) {
			if( strcmp( IpText[i], ip_buf ) == 0 ) {
				sscanf( buf, "%s %s %s %[^\t\n]", ip_buf,  Status[i].Hid, Status[i].Seg, Status[i].Addr );
			}
		}
	}
	fclose( fAddr );

	int unstable, sens;
	fAddr = fopen( "save.txt", "r" );
	if( fAddr != NULL ) {
		while( fgets( buf, sizeof( buf ), fAddr ) ) {
			sscanf( buf, "%s %d %d", ip_buf, &unstable, &sens );
			for( int i = 0; i < IpNumber; i++ ) {
				if( strcmp( IpText[i], ip_buf ) == 0 ) {
					Status[i].Sens = sens;
					if( unstable == 1 ) {
						Status[i].Status = UNSTABLE;
						UpdateView( i, Status[i].Status );
					}
				}
			}
		}
		fclose( fAddr );
	}

	int dot_counter;
	int iIp, iSeg;

	for( int i = 0; i < IpNumber; i++ ) {		// FIX: Нахуя столько циклов?! Ужать в один. (в идеале)
		if( Status[i].Seg[0] == '\0' ) {
			dot_counter = 0; iIp = 0; iSeg = 0;
			while( dot_counter != 2 ) {
				if( IpText[i][iIp++] == '.' ) dot_counter++;
			}
			while( IpText[i][iIp] != '.' ) {
				Status[i].Seg[iSeg++] = IpText[i][iIp++];
			}
			Status[i].Seg[iSeg] = '\0';
		}
	}

	SetTimer( hWndMain, 0, TIMER_INTERVAL, NULL );

	HICON icon = LoadIcon( hInstance, MAKEINTRESOURCE( IDI_ICON ) );
	SendMessage( hWndMain, WM_SETICON, ICON_SMALL, (LPARAM) icon );
	SendMessage( hWndMain, WM_SETICON, ICON_BIG, (LPARAM) icon );

	ShowWindow(hWndMain, nCmdShow);
	UpdateWindow(hWndMain);

	for( int i = 0; i < IpNumber; i += IP_GROUP_COUNT ) {
		CreateThread( NULL, 0, CheckOnlineThread, (LPVOID)(i), 0, NULL );
	}

	SendMessage( hWndMain, WM_TIMER, (WPARAM) -1, NULL );

	InitSplash( hWndMain, hInstance );	// Прогрузка окна уведомлений

	ListView_SortItems( hLV, SortItemsInColumn, NULL );

	MSG msg;
    while( GetMessage( &msg, NULL, 0, 0) ) {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    return (int) msg.wParam;
}