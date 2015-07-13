#include "PingStatistics.h"

const int PingStatistics::MAX_STRIKE_NUMBER = 10000;

PingStatistics::PingStatistics( const int PingsNum ) {
	_pingsNum = PingsNum;
	_percs = 0;
	_bad = 0;
	_offlineStrike = 0;
	_onlineStrike = 0;
}

PingStatistics::~PingStatistics( void ) {
}

void PingStatistics::Update( IP_STATUS S ) {
	_list_rw_lock.w_lock();
	_pings.push_front( PingStatus( S ) );
	if( _pings.size() > _pingsNum ) {
		_pings.pop_back();
	}

	if( S == IP_SUCCESS ) {
		if( _onlineStrike < MAX_STRIKE_NUMBER )
			_onlineStrike++;
		_offlineStrike = 0;
	} else {
		if( _offlineStrike < MAX_STRIKE_NUMBER )
			_offlineStrike++;
		_onlineStrike = 0;
	}

	ResetStats();

	for( auto it = _pings.begin(); it != _pings.end(); it++ ) {
		if( it->Status != IP_SUCCESS )
			_bad++;
	}

	_percs = ( (float) _bad / _pings.size() ) * 100;

	_list_rw_lock.w_unlock();
}

void PingStatistics::Reset( void ) {
	_list_rw_lock.w_lock();
	ResetStats();
	_pings.resize( 0 );
	_list_rw_lock.w_unlock();
}

void PingStatistics::ResetStats( void ) {
	_bad = 0;
	_percs = 0;
}

int PingStatistics::GetPerc() {
	return _percs;
}

int PingStatistics::GetOfflineStrike() {
	return _offlineStrike;
}

int PingStatistics::GetOnlineStrike() {
	return _onlineStrike;
}

bool PingStatistics::IsFull() {
	return _pings.size() >= _pingsNum;
}