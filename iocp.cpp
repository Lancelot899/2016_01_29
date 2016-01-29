#define _CRT_SECURE_NO_WARNINGS
#include "iocp.h"
#include "Base64.h"

using namespace lancelot;



IOCP::IOCP() {
	WSADATA wsaData{ 0 };
A:  while (!WSAStartup(MAKEWORD(2, 2), &wsaData));
	if (2 != LOBYTE(wsaData.wVersion) || 2 != HIBYTE(wsaData.wVersion)) {
		WSACleanup();
		memset((void*)&wsaData, 0, sizeof(WSADATA));
		goto A;
	}
	_sockListen = NULL;
	_bTimeToKill = false;
	_hCompletionPort = NULL;
	_bInit = false;
	_currentThreads = 0;
	_busyThreads = 0;
	_sendKbps = 0;
	_recvKpbs = 0;
	g_fResourceInUse = false;

}

IOCP::~IOCP() {
	try {
		Shutdown();
		WSACleanup();
	}
	catch (...) {}
}


bool IOCP::Initialize(NOTIFYPROC pNotifyProc, int nPort) {
	_notify = pNotifyProc;
	DWORD flag = 0;
	while (INVALID_SOCKET == (_sockListen = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED))) {
		printf("initial socket error!\n");
		Sleep(1000);
	}
	while (SOCKET_ERROR == setsockopt(_sockListen, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag))) {
		printf("setsockopt error! Err: %d\n", WSAGetLastError());
		Sleep(1000);
	}
	SOCKADDR_IN serverAddr;
	serverAddr.sin_port = htons(nPort);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;

	while (SOCKET_ERROR == bind(_sockListen, (sockaddr*)&serverAddr, sizeof(sockaddr))) {
		printf("bind error! Err: %d\n", WSAGetLastError());
	}
	while (NULL == (_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0))) {
		printf("create completion port error!\n");
		Sleep(1000);
	}
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	UINT workerCnt = sysInfo.dwNumberOfProcessors * 2;
	HANDLE hWorker;
	for (UINT i = 0; i < workerCnt; i++) {
		UINT tmp = i + 1;
		while (NULL == (hWorker = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThreadFunc, this, 0, (LPDWORD)&tmp))) {
			printf("create work thread error\n");
			Sleep(1000);
		}
		PostRecv();
		++_currentThreads;
		CloseHandle(hWorker);
	}
	_bInit = true;
	return true;
}

unsigned IOCP::WorkerThreadFunc(LPVOID WorkContext) {
	IOCP* pThis = reinterpret_cast<IOCP*>(WorkContext);
	HANDLE hCompletionPort = pThis->_hCompletionPort;
	DWORD   dwIoSize;
	_NetIO* pNetIO;
	DWORD nsock;
	InterlockedIncrement(&pThis->_busyThreads);
	while (true) {
		try {
			BOOL IOret = GetQueuedCompletionStatus(hCompletionPort, &dwIoSize, (PULONG_PTR)& nsock, (LPOVERLAPPED*)&pNetIO, INFINITE);
			int nBusyThreads = InterlockedIncrement(&pThis->_busyThreads);
			DWORD dwIOErr = GetLastError();
			if (dwIoSize == 0 || nsock == 0 || 0 == pNetIO)
				break;
			if (dwIoSize == 0 || dwIOErr == SOCKET_ERROR || IOret == 0) {
				pThis->MoveToFreePool(pNetIO);
				continue;
			}
			pThis->OnRecv(pNetIO, dwIoSize);
		}
		catch (...) {}
	}
	InterlockedDecrement(&pThis->_currentThreads);
	InterlockedDecrement(&pThis->_busyThreads);
	return 0;
}

bool IOCP::PostRecv() {
	_NetIO *PerIoData = AllocateIO();
	if (PerIoData == NULL) {
		printf("GlobalAlloc() failed with error %d\n", GetLastError());
		return false;
	}

	DWORD RecvBytes = 0;
	DWORD Flags = 0;
	if (WSARecvFrom(_sockListen, &(PerIoData->_dataBuf), 1, &RecvBytes, &Flags,
		(sockaddr*)&(PerIoData->_clientAddr), &PerIoData->_clientAddrLen,
		&(PerIoData->_ol), NULL) == SOCKET_ERROR) {
		if (WSAGetLastError() != ERROR_IO_PENDING) {
			printf("WSARecvFrom() failed with error %d\n", WSAGetLastError());
			return false;
		}
	}
	return true;
}

bool IOCP::PostSend(_NetIO* pContext, LPBYTE lpData, UINT nSize) {
	if (pContext == NULL)
		return false;
	try {
		static DWORD nLastTick = GetTickCount();
		static DWORD nBytes = 0;
		nBytes += nSize;

		if (GetTickCount() - nLastTick >= 1000) {
			nLastTick = GetTickCount();
			InterlockedExchange((LPLONG)&(_sendKbps), nBytes);
			nBytes = 0;
		}
		sendto(_sockListen, (char *)lpData, nSize, 0, (sockaddr*)&(pContext->_clientAddr), sizeof(pContext->_clientAddr));
	}
	catch (...) {}
	return true;
}

bool IOCP::PostSend(SOCKADDR_IN *sAddr, LPBYTE lpData, UINT nSize) {
	if (sAddr == NULL)
		return false;
	try {
		static DWORD nLastTick = GetTickCount();
		static DWORD nBytes = 0;
		nBytes += nSize;
		if (GetTickCount() - nLastTick >= 1000) {
			nLastTick = GetTickCount();
			InterlockedExchange((LPLONG)&(_sendKbps), nBytes);
			nBytes = 0;
		}
		sendto(_sockListen, (char *)lpData, nSize, 0, (sockaddr*)sAddr, sizeof(SOCKADDR_IN));
	}
	catch (...) {}
	return true;
}
bool IOCP::OnRecv(_NetIO* pContext, DWORD dwIoSize) {
	try {
		if (dwIoSize == 0) {
			return false;
		}
		static DWORD nLastTick = GetTickCount();
		static DWORD nBytes = 0;
		nBytes += dwIoSize;

		if (GetTickCount() - nLastTick >= 1000) {
			nLastTick = GetTickCount();
			InterlockedExchange((LPLONG)&(_recvKpbs), nBytes);
			nBytes = 0;

			if (dwIoSize != MSGHEADLEN) {
				int OutByte = 0;
				char* tmp = pContext->_dataBuf.buf;
				tmp += MSGHEADLEN;
				std::string ClientMsg = Base64::Decode(tmp + 1, pContext->_dataBuf.len - MSGHEADLEN - 1, OutByte);
				if (*tmp == IOTYPE::LINK) {
					const char* msg = ClientMsg.c_str();
					if (!strcmp(msg, "hello server")) {
						unsigned char msg[] = "welcome to use IOCP built by lancelot, please login!";
						std::string Msg = Base64::Encode(msg, strlen((const char*)msg));
						PostSend(pContext, (LPBYTE)Msg.c_str(), Msg.size());
					}
				}

				if (*tmp == IOTYPE::LOGIN) {
					const char *msg = ClientMsg.c_str();
					char* password = (char*)msg;
					int len = 0;
					while (*password != 0) {
						++len;
						if (*password == '-') {
							++password;
							break;
						}
					}
					char* ID = (char*)malloc(len + 1);
					strncpy(ID, msg, len);
				}

				if (*tmp == IOTYPE::LOGOUT) {
					if (!strcmp(ClientMsg.c_str(), "bye-bye")) {
						
					}
				}

				if (*tmp == IOTYPE::DEFAULT) {
					if (!strncmp(ClientMsg.c_str(), "link", strlen("link"))) {
						
					}
				}

				if (_notify != NULL)
					_notify(pContext, dwIoSize, _busyThreads);
			}
		}
		MoveToFreePool(pContext);
		PostRecv();
	}

	catch (...) {}
	return true;
}

void IOCP::CloseCompletionPort() {
	while (_currentThreads) {
		PostQueuedCompletionStatus(_hCompletionPort, 0, (DWORD)NULL, NULL);
		Sleep(100);
	}
	CloseHandle(_hCompletionPort);

	_NetIO* pContext = NULL;
	for (auto it = _listIO.begin(); it != _listIO.end(); it++)
		delete (*it);
	_listIO.clear();
}

void IOCP::Shutdown() {
	if (_bInit == false)
		return;

	_bInit = false;
	_bTimeToKill = true;

	closesocket(_sockListen);

	CloseCompletionPort();
	while (!_listFree.empty())
		delete _listFree.front();
}

void IOCP::MoveToFreePool(_NetIO *pContext) {
	while (InterlockedExchange(&g_fResourceInUse, TRUE) == TRUE)Sleep(1);
	IOlistType::iterator iter;
	iter = find(_listIO.begin(), _listIO.end(), pContext);
	if (iter != _listIO.end()) {
		_listIO.remove(pContext);
		_listFree.push_back(pContext);
	}
	InterlockedExchange(&g_fResourceInUse, FALSE);
}

_NetIO* IOCP::AllocateIO() {
	_NetIO* pContext = NULL;

	while (InterlockedExchange(&g_fResourceInUse, TRUE) == TRUE)Sleep(1);

	if (!_listFree.empty()) {
		pContext = _listFree.front();
		_listFree.remove(pContext);
	}
	else {
		pContext = new _NetIO;
	}
	pContext->_dataBuf.len = BUFSIZE;
	pContext->_dataBuf.buf = pContext->_buf;
	pContext->_recvBytes = 24;
	pContext->_clientAddrLen = sizeof(pContext->_clientAddr);

	_listIO.push_back(pContext);

	InterlockedExchange(&g_fResourceInUse, FALSE);
	return pContext;
}

bool IOCP::IsRunning() {
	return _bInit;
}
