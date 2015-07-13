#pragma once
#include <Windows.h>
#include <IPExport.h>
#include <list>
#include "rw_lock.h"

using namespace std;

struct PingStatus {
	IP_STATUS	Status;

	PingStatus( IP_STATUS Status ) {
		PingStatus::Status = Status;
	}

	PingStatus(){};
};

class PingStatistics {
public:
								PingStatistics( const int PingsNum );
								~PingStatistics( void );
	static const int			MAX_STRIKE_NUMBER;
	void						Update( IP_STATUS S );
	void						Reset( void );
	int							GetPerc();
	int							GetOfflineStrike();
	int							GetOnlineStrike();
	bool						IsFull();
private:
	void						ResetStats( void );

	rw_lock						_list_rw_lock;
	list<PingStatus>			_pings;
	unsigned int				_offlineStrike, _onlineStrike;
	unsigned int				_bad, _percs;
	unsigned int				_pingsNum;
};

