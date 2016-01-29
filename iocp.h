#ifndef IOCP_H
#define IOCP_H

#include <process.h>
#include "user.h"
#include <list>
#include <string>

namespace lancelot {

#define MSGHEADLEN 28


	class IOCP {
	public:
		typedef void (CALLBACK* NOTIFYPROC)(LPVOID, DWORD, UINT nCode);
		typedef std::list<_NetIO*>      IOlistType;

	public:
		IOCP();
		virtual ~IOCP();
		bool Initialize(NOTIFYPROC pNotifyProc, int nPort);
		bool PostSend(_NetIO* pContext, LPBYTE lpData, UINT nSize);
		bool PostSend(SOCKADDR_IN * sAddr, LPBYTE lpData, UINT nSize);
		bool PostRecv();

		bool IsRunning();
		void Shutdown();

	public:
		LONG                             _currentThreads;
		LONG                             _busyThreads;
		UINT                             _sendKbps;
		UINT                             _recvKpbs;
		NOTIFYPROC                       _notify;
		IOlistType                       _listFree;
		IOlistType                       _listIO;

	private:
		volatile LONG			g_fResourceInUse;
		bool				   	_bInit;
		SOCKET					_sockListen;
		HANDLE					_hCompletionPort;
		bool			   		_bTimeToKill;

		static unsigned __stdcall 	WorkerThreadFunc(LPVOID WorkContext);

		void 						MoveToFreePool(_NetIO *pContext);
		_NetIO*             	    AllocateIO();
		void 						CloseCompletionPort();
		void 						Stop();
		bool 						OnRecv(_NetIO* pContext, DWORD dwIoSize = 0);
	};


}

#endif // IOCP_H
