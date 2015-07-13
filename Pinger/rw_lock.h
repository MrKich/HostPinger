#pragma once
#include <Windows.h>

class rw_lock {
public:
	rw_lock( void );
	~rw_lock( void );
	void w_lock( void );
	void w_unlock( void );
private:
	HANDLE _mutex;
};

