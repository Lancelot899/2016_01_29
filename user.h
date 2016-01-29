#ifndef USER_H
#define USER_H

#include <winsock2.h>
#include <windows.h>
#include "sqlite3.h"

#pragma comment(lib, "ws2_32.lib")

namespace lancelot {
#define  BUFSIZE 512
	struct _NetIO {
		OVERLAPPED             _ol;
		WSABUF                 _dataBuf;
		unsigned long          _recvBytes;
		SOCKADDR_IN            _clientAddr;
		int                    _clientAddrLen;
		char                   _buf[BUFSIZE];


		_NetIO() {
			ZeroMemory(this, sizeof(_NetIO));
		}

	};

	enum IOTYPE {
		LINK    = 0,
		LOGIN   = 1,
		LOGOUT  = 2,
		EXIT    = 3,
		DEFAULT = 4
	};

	class User {
	public:
		User(char* ID);
		~User();

	public:
		

	private:
		_NetIO*    _io;
		sqlite3*   _db;
		char       _ID[19];
	};

}

#endif // USER_H
