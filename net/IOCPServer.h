// IOCPServer.h: interface for the CIOCPServer class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_IOCPSERVER_H__75B80E90_FD25_4FFB_B273_0090AA43BBDF__INCLUDED_)
#define AFX_IOCPSERVER_H__75B80E90_FD25_4FFB_B273_0090AA43BBDF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <winsock2.h>
#include <MSTcpIP.h>

#include <vector>
#include <string>

#include <common/Buffer.h>
//#include "CpuUsage.h"

#include <base/lock.h>
#include <net/net.h>

#include <process.h>

////////////////////////////////////////////////////////////////////
#define	NC_CLIENT_CONNECT		0x0001
#define	NC_CLIENT_DISCONNECT	0x0002
#define	NC_TRANSMIT				0x0003
#define	NC_RECEIVE				0x0004
#define NC_RECEIVE_COMPLETE		0x0005 // ��������

//#define BUFFER_COMPRESSED		1
#define ENABLE_HTTP				1

enum IOType 
{
	IOInitialize,
	IORead,
	IOWrite,
	IOIdle
};


class OVERLAPPEDPLUS 
{
public:
	OVERLAPPED			m_ol;
	IOType				m_ioType;

	OVERLAPPEDPLUS(IOType ioType) {
		ZeroMemory(this, sizeof(OVERLAPPEDPLUS));
		m_ioType = ioType;
	}
};


struct ClientContext
{
    SOCKET				m_Socket;
	// Store buffers
	CBuffer				m_WriteBuffer;
#if defined(BUFFER_COMPRESSED) || defined(ENABLE_HTTP)
	CBuffer				m_CompressionBuffer;	// ���յ���ѹ��������
#endif
	CBuffer				m_DeCompressionBuffer;	// ��ѹ�������
	CBuffer				m_ResendWriteBuffer;	// �ϴη��͵����ݰ�������ʧ��ʱ�ط�ʱ��

	int					m_Dialog[2]; // �ŶԻ����б��ã���һ��int�����ͣ��ڶ�����CDialog�ĵ�ַ
	int					m_nTransferProgress;
#ifdef ENABLE_HTTP
	bool				ispostheader;
	int					waittoread;
#endif
	// Input Elements for Winsock
	WSABUF				m_wsaInBuffer;
	BYTE				m_byInBuffer[8192];

	// Output elements for Winsock
	WSABUF				m_wsaOutBuffer;
	HANDLE				m_hWriteComplete;

	// Message counts... purely for example purposes
	LONG				m_nMsgIn;
	LONG				m_nMsgOut;	

	BOOL				m_bIsMainSocket; // �ǲ�����socket

	ClientContext*		m_pWriteContext;
	ClientContext*		m_pReadContext;
};

/*template<>
inline UINT AFXAPI HashKey(CString & strGuid)
{
  return HashKey( (LPCTSTR) strGuid);         
}*/

#include "Mapper.h"

typedef void (CALLBACK* NOTIFYPROC)(LPVOID, ClientContext*, UINT nCode);

typedef std::vector<ClientContext *> ContextList;

class CCpuUsage {
public:
	CCpuUsage() {};
	~CCpuUsage() {};

	void Init() {};
	int GetUsage() { return 0; };
};

class CIOCPServer
{
public:
	void DisconnectAll();
	CIOCPServer();
	virtual ~CIOCPServer();

	NOTIFYPROC					m_pNotifyProc;
	LPVOID						m_lParam;

	bool Initialize(NOTIFYPROC pNotifyProc, LPVOID lParam, int nMaxConnections, int nPort);

	static unsigned __stdcall ListenThreadProc(LPVOID lpVoid);
	static unsigned __stdcall ThreadPoolFunc(LPVOID WorkContext);
	static Lock	m_cs;

	void Send(ClientContext* pContext, LPBYTE lpData, UINT nSize);
	void PostRecv(ClientContext* pContext);

	bool IsRunning();
	void Shutdown();
	void ResetConnection(ClientContext* pContext);
	
	LONG					m_nCurrentThreads;
	LONG					m_nBusyThreads;

	
	UINT					m_nSendKbps; // ���ͼ�ʱ�ٶ�
	UINT					m_nRecvKbps; // ���ܼ�ʱ�ٶ�
	UINT					m_nMaxConnections; // ���������
protected:
	void InitializeClientRead(ClientContext* pContext);
	BOOL AssociateSocketWithCompletionPort(SOCKET device, HANDLE hCompletionPort, DWORD dwCompletionKey);
	void RemoveStaleClient(ClientContext* pContext, BOOL bGraceful);
	void MoveToFreePool(ClientContext *pContext);
	ClientContext*  AllocateContext();

	LONG				m_nWorkerCnt;

	bool				m_bInit;
	bool				m_bDisconnectAll;
#ifndef ENABLE_HTTP
	BYTE				m_bPacketFlag[5];
#endif
	void CloseCompletionPort();
	void OnAccept();
	bool InitializeIOCP(void);
	void Stop();

	ContextList				m_listContexts;
	ContextList				m_listFreePool;
	WSAEVENT				m_hEvent;
	SOCKET					m_socListen;    
    HANDLE					m_hKillEvent;
	HANDLE					m_hThread;
	HANDLE					m_hCompletionPort;
	bool					m_bTimeToKill;
	CCpuUsage				m_cpu;

	LONG					m_nKeepLiveTime; // ������ʱ

	// Thread Pool Tunables
	LONG					m_nThreadPoolMin;
	LONG					m_nThreadPoolMax;
	LONG					m_nCPULoThreshold;
	LONG					m_nCPUHiThreshold;


	std::string GetHostName(SOCKET socket);

	void CreateStream(ClientContext* pContext);

	BEGIN_IO_MSG_MAP()
		IO_MESSAGE_HANDLER(IORead, OnClientReading)
		IO_MESSAGE_HANDLER(IOWrite, OnClientWriting)
		IO_MESSAGE_HANDLER(IOInitialize, OnClientInitializing)
	END_IO_MSG_MAP()

	bool OnClientInitializing	(ClientContext* pContext, DWORD dwSize = 0);
	bool OnClientReading		(ClientContext* pContext, DWORD dwSize = 0);
	bool OnClientWriting		(ClientContext* pContext, DWORD dwSize = 0);

};

#endif // !defined(AFX_IOCPSERVER_H__75B80E90_FD25_4FFB_B273_0090AA43BBDF__INCLUDED_)
