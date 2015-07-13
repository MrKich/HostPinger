#include "rw_lock.h"

rw_lock::rw_lock( void ) {
	_mutex = CreateMutex( NULL, false, NULL );
}

rw_lock::~rw_lock( void ) {
	CloseHandle( _mutex );
}

void rw_lock::w_lock( void ) {
	WaitForSingleObject( _mutex, INFINITE );
}

void rw_lock::w_unlock( void ) {
	ReleaseMutex( _mutex );
}