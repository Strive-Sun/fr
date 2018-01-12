
#include "stdafx.h"

#include "TANetBase.h"

#include <tcmalloc.h>

#include <base/string/stringprintf.h>
#include <common/Buffer.h>

#include <net/net.h>
#include <curlhelper.h>

//gzip
#ifndef _TA_NOGZIP
#include <zlibhelper.h>
#endif

//ms
#define DEFAULT_TIME_OUT	5000

#ifdef _SNOW
namespace snow {
	namespace config {
		extern int timeout;
	};
};
#undef DEFAULT_TIME_OUT
#define DEFAULT_TIME_OUT   (snow::config::timeout)
#endif

#define _TA_RETRY_TIMES		50
#define TA_BLOCK_LEN		4096

size_t CurlOnWriteData(void* buffer, size_t size, size_t nmemb, void* lpVoid)  
{  
  CBuffer* pbuf = (CBuffer *)lpVoid;  
  if(NULL == pbuf || NULL == buffer) {
      return -1;
  }
  
  pbuf->Write((unsigned char *)buffer, size * nmemb);

  return nmemb;
}

void HttpProcessContent(CBuffer& header, CBuffer& content)
{
	bool bIsChunked = false, bIsGzip = false;
	char* chheader = reinterpret_cast<char *>(header.GetBuffer());
	char* tstr;

	if (strstr(chheader, "chunked"))
	{
		bIsChunked = true;
	} else {
		bIsChunked = false;
	}

	//�ж��Ƿ�Ϊgzip��
	tstr = strstr(chheader, "Content-Encoding:");
	if (tstr)
	{
		tstr = strstr(tstr, "gzip");
		if (tstr) 
		{
			//buf = &cBuf;
			bIsGzip = true;
		}
	}

	//curl�ᴦ��chunked
	/*if (bIsChunked) {
		net::HttpChunkedDecoder cd;
		int len = cd.FilterBuf((char *)content.GetBuffer(), content.GetBufferLen());
		content.SetBufferLen(len);
	}*/

	if (bIsGzip) {
		CBuffer buf;
		ungzip((char *)content.GetBuffer(), content.GetBufferLen(), buf);

		content.SetBufferLink(buf.GetBuffer(), buf.GetBufferLen(), buf.GetMemSize());
		buf.SetDestoryFree(false);
	}
}

//CRecvDataMgr
CRecvDataMgr::CRecvDataMgr()
{
	Clear();
}

CRecvDataMgr::~CRecvDataMgr()
{
}

void CRecvDataMgr::Clear()
{
	m_bfirst = true;
	m_bchunked = false;
	m_next_size = 0;
	m_headers_offset = 0;
	m_bfinished = false;
	m_buffer.ClearBuffer();
}

char* CRecvDataMgr::FindHeaderEndFlags(unsigned char* buffer, int len)
{
	for (int i = 0; i < len - 4; i++)
	{
		if (memcmp(buffer + i, "\r\n\r\n", 4) == 0)
		{
			return reinterpret_cast<char *>(buffer + i);
			break;
		}
	}
	return NULL;
}

char* CRecvDataMgr::FindLineEndFlags(char* line, int len)
{
	for (int i = 0; i < len - 2; i++)
	{
		if (memcmp(line + i, "\r\n", 2) == 0)
		{
			return line + i;
			break;
		}
	}
	return NULL;
}

bool CRecvDataMgr::CheckFinish(unsigned char* buffer, int len)
{
	if (len > 0)
	{
		m_buffer.Write(buffer, len);

		if (m_bfirst)
		{
			char* pEnd = FindHeaderEndFlags(m_buffer.GetBuffer(), m_buffer.GetBufferLen());
			if (pEnd)
			{
				m_bfirst = false;

				pEnd[0] = 0;

				char* pBuffer = reinterpret_cast<char *>(m_buffer.GetBuffer());

				if (strstr(pBuffer, "chunked"))
				{
					m_bchunked = true;
				} else {
					m_bchunked = false;
				}

				pEnd[0] = '\r';

				m_next_size = pEnd + 4 - pBuffer;
				m_headers_offset = m_next_size;
			}
		}

		if (m_bchunked)
		{
			char* pBuffer = reinterpret_cast<char *>(m_buffer.GetBuffer() + m_next_size);
			int len = m_buffer.GetBufferLen() - m_next_size;
			int nChunkSize;
			char *nChunkData, *strLineEnd, *strLine = pBuffer;

			while (len > 0)
			{
				nChunkSize = strtoul(strLine, &strLineEnd, 16);
				if (nChunkSize == 0)
				{
					m_bfinished = true;
					break;
				}
				if (nChunkSize > len - (strLineEnd - strLine)) break;

				nChunkData = FindLineEndFlags(strLine, len - (strLineEnd - strLine));
				if (!nChunkData) break;
				nChunkData += 2;//'\r\n'

				strLine = nChunkData + nChunkSize + 2;	//'\r\n'
				len = m_buffer.GetBufferLen() - (strLine - reinterpret_cast<char *>(m_buffer.GetBuffer()));
				m_next_size = (strLine - reinterpret_cast<char *>(m_buffer.GetBuffer()));
			}
		}

		return m_bfinished;
	} else {
		return true;
	}
}

void SocketDefaultSet(SOCKET sock)
{
	int nTime = DEFAULT_TIME_OUT;

	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&nTime, sizeof(nTime)); 
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&nTime, sizeof(nTime));
}

bool AnalyzeRecvData(CBuffer *inbuf, CBuffer *outbuf, char **szRetHeader)
{
	if (!inbuf || !outbuf) return false;
	
	bool bIsChunked = false, bIsGzip = false;
	char *strLine = NULL;
	char *tstr = NULL;

	if (inbuf->GetBufferLen() > 0)
	{
		char* pBuffer = reinterpret_cast<char *>(inbuf->GetBuffer());
		int len = inbuf->GetBufferLen();

		if (strLine == NULL)
		{
			strLine = strstr(pBuffer, "\r\n\r\n");
			if (!strLine) return false;
			strLine[0] = 0;
			if (szRetHeader) *szRetHeader = _strdup(pBuffer);
			
			if (strstr(pBuffer, "chunked"))
			{
				bIsChunked = true;
			} else {
				bIsChunked = false;
			}

			//�ж��Ƿ�Ϊgzip��
			tstr = strstr(pBuffer, "Content-Encoding:");
			if (tstr)
			{
				tstr = strstr(tstr, "gzip");
				if (tstr) 
				{
					//buf = &cBuf;
					bIsGzip = true;
				}
			}

			strLine[0] = '\r';
			strLine += 4;	//"\r\n\r\n"
		}
		
		len -= (strLine - pBuffer);
		if (len > 0)
		{
			if (bIsChunked)
			{
				net::HttpChunkedDecoder cd;
				len = cd.FilterBuf(strLine, len);
				if (len > 0)
				{
					outbuf->Write(reinterpret_cast<BYTE *>(strLine), len);
				}
			} else {
				outbuf->Write(reinterpret_cast<BYTE *>(strLine), len);
			}
		}
	}

	if (outbuf->GetBufferLen() <= 0) 
		return false;
	
	//char *szRetScr;
#ifndef _TA_NOGZIP
	if (bIsGzip)
	{
		CBuffer uncompressbuf;
		ungzip((char *)outbuf->GetBuffer(), outbuf->GetBufferLen(), uncompressbuf);

		if (uncompressbuf.GetBufferLen() > 0) {
			outbuf->SetBufferLink(uncompressbuf.GetBuffer(), 
				uncompressbuf.GetBufferLen(),
				uncompressbuf.GetMemSize());
			uncompressbuf.SetDestoryFree(false);
			outbuf->WriteZeroByte();	//�ַ��ж�
		}
	}
#endif
	/*else{
		szRetScr = (char *)malloc(outbuf->GetBufferLen() + 1);
		memcpy(szRetScr, outbuf->GetBuffer(), outbuf->GetBufferLen());
		szRetScr[outbuf->GetBufferLen()] = 0;
	}*/
	return true;
}

bool RecvDataEx(SOCKET sock, CBuffer *buf, char **szHeader)
{
	if (!buf) return false;

	char buffer[TA_BLOCK_LEN];
	int nResult;

	CRecvDataMgr rdm;
	int i_again = 0;
	/*
	If   no   error   occurs,   recv   returns   the   number   of   bytes   received.
	If   the   connection   has   been   gracefully   closed,   the   return   value   is   zero.
	Otherwise,   a   value   of   SOCKET_ERROR   is   returned,
		and   a   specific   error   code   can   be   retrieved   by   calling   WSAGetLastError. 
	*/
	
	while (true)
	{
		nResult = recv(sock, buffer, TA_BLOCK_LEN, 0); //��ȡ����

		//if (ioctlsocket(sock, SIOCATMARK)
		
		if (nResult <= 0)
		{
			break;
		}/* else if (nResult < 0) {
			//5������
			if (i_again < 5)
			{
				i_again++;
				Sleep(200);
				continue;
			} else {
				break;
			}
		}*/
		
		//����Ƿ����
		if (rdm.CheckFinish(reinterpret_cast<unsigned char *>(buffer), nResult))
		{
			break;
		}
	}

	return AnalyzeRecvData(&rdm.m_buffer, buf, szHeader);
}

bool RecvDataEx(SOCKET sock, LPSTR *szScr)
{
	CBuffer buf;
	bool ret = RecvDataEx(sock, &buf);
	if (ret)
	{
		//��0��β���ַ���
		buf.WriteZeroByte();
		buf.SetDestoryFree(false); //����ʱ���������

		*szScr = reinterpret_cast<LPSTR>(buf.GetBuffer());
	}
	return ret;
}

int RecvData(SOCKET sock, LPCSTR request, LPSTR *szScr)
{
	//�������� 
	if (send(sock, request, lstrlenA(request), 0) == SOCKET_ERROR)
	{
		return -2;
	}

	if (!RecvDataEx(sock, szScr))
	{
		return -4;
	}

	return 0;
}

CTANetBase::CTANetBase(void)
	: m_use_proxy_(false)
	, m_proxy_type_(kHttp)
{
}


CTANetBase::~CTANetBase(void)
{
}

bool CTANetBase::SetProxy(LPCSTR lpszIP, DWORD nPort, SnowProxyType type)
{
	//�������������IP��ַ   
	hostent* pHostEnt = gethostbyname(lpszIP);
	if (pHostEnt == NULL) {
		return false;
	}

	//���� 
	memset((void *)&m_proxy_addr, 0, sizeof(m_proxy_addr));   
	m_proxy_addr.sin_family = AF_INET; 
	m_proxy_addr.sin_port = htons(nPort); 
	memcpy(&m_proxy_addr.sin_addr.S_un.S_addr, pHostEnt->h_addr, pHostEnt->h_length);  

	m_use_proxy_ = true;
	m_proxy_type_ = type;

	//ʹ��inet_ntoa���ܲ���ȫ TAMARK
	char *strip = inet_ntoa(m_proxy_addr.sin_addr);
	base::SStringPrintf(&m_proxy_str_address_, "%s:%u", strip, nPort);

	return true;
}

bool CTANetBase::InitHttpProxySocket(SOCKET *pSock)
{
	SOCKET r_sock;
	r_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);   
	if(r_sock == INVALID_SOCKET)
		return false;  
 
	//������ӳپ͸ߵ��
	SocketDefaultSet(r_sock);

	if( 0 != connect(r_sock, (struct sockaddr*)&m_proxy_addr, sizeof(m_proxy_addr)) ) 
		return false; 

	*pSock = r_sock;
	return true;
}

bool CTANetBase::InitSocks4ProxySocket(SOCKET *pSock, sockaddr_in *ptr_server_addr, char *http_domain)
{
//����μ� http://zh.wikipedia.org/wiki/SOCKS

	SOCKET r_sock;
	if (!InitHttpProxySocket(&r_sock))
	{
		return false;
	}

	//var
	int nRcvd=0,nCount=0;

// Step 1: �����ǿͻ�����SOCKS 4��������������͵�����������ĸ�ʽ�����ֽ�Ϊ��λ����
/*	+����+����+����+����+����+����+����+����+����+����+....+����+
	| VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
	+����+����+����+����+����+����+����+����+����+����+....+����+
	   1    1      2              4           variable       1		*/
//	VN��SOCK�汾��Ӧ����4��
//	CD��SOCK�������룬1��ʾCONNECT����2��ʾBIND����
//	DSTPORT��ʾĿ�������Ķ˿ڣ�
//	DSTIPָĿ��������IP��ַ��
	//char reqNego[9]={'\x04','\x01', 0};
	char reqNego[9]={0};
	char resNego[8]={0};

	reqNego[0] = 4;
	reqNego[1] = 1;
	*(unsigned short *)(&reqNego[2]) = ptr_server_addr->sin_port;
	*(unsigned long *)(&reqNego[4]) = ptr_server_addr->sin_addr.S_un.S_addr;

	int nRet = send(r_sock, reqNego, 9, 0);
	if (nRet == SOCKET_ERROR) goto error_l;


//	��������������ͻ�Ӧ�������ֽ�Ϊ��λ����
/*	+����+����+����+����+����+����+����+����+
	| VN | CD | DSTPORT |      DSTIP        |
	+����+����+����+����+����+����+����+����+
	   1    1      2              4				*/
//	VN�ǻ�Ӧ��İ汾��Ӧ����0��
//	CD�Ǵ���������𸴣��м��ֿ��ܣ�
//	90������õ�����
//	91�����󱻾ܾ���ʧ�ܣ�
//	92������SOCKS�������޷����ӵ��ͻ��˵�identd��һ����֤��ݵĽ��̣������󱻾ܾ���
//	93�����ڿͻ��˳�����identd������û���ݲ�ͬ�����ӱ��ܾ���
//	DSTPORT��DSTIP��������е�������ͬ���������ԡ�
//	������󱻾ܾ���SOCKS������������ͻ��˶Ͽ����ӣ����������������������ͳ䵱�ͻ�����Ŀ������֮�����˫�򴫵ݣ��Կͻ��˶��ԣ�����ֱͬ������Ŀ������������

	
	nRcvd=0;
	nCount=0;
	while(true)
	{
		//����sock[0]������������
		do{
			nRet = recv(r_sock, (char *)resNego + nRcvd, 8 - nRcvd,0);
			if(nRet==SOCKET_ERROR)
			{
				goto error_l;
			}
			nRcvd += nRet;
		}
		while((nRcvd < 8) && (++nCount < _TA_RETRY_TIMES));
		if(nRcvd >= 8) break;
		
		if(nCount++ >= _TA_RETRY_TIMES){
			goto error_l;
		}
	}

	if(resNego[0] != 0 || (resNego[1] != 90)){
		goto error_l;
	};

// ���ˣ������Ѿ��������ڴ��׽����Ͽɽ������ݵ��շ���

	*pSock = r_sock;
	return true;

error_l:
	closesocket(r_sock);
	return false;
}

bool CTANetBase::InitSocks5ProxySocket(SOCKET *pSock, sockaddr_in *ptr_server_addr, char *http_domain)
{
//����μ� http://www.ietf.org/rfc/rfc1928.txt
	
	SOCKET r_sock;
	if (!InitHttpProxySocket(&r_sock))
	{
		return false;
	}

	//var
	fd_set fdread;
	int nRcvd=0,nCount=0;
	timeval tout;
	char resSubNego1[5]={'\0'};
	char resNego[2]={0};
	//5�볬ʱ
	tout.tv_sec = (DEFAULT_TIME_OUT / 1000);
	tout.tv_usec = DEFAULT_TIME_OUT;

// Step 1: ���Ӵ���������ɹ������Ͽ�ʼ�ʹ���Э�̣�Э�̱�������,ѯ�ʷ��������汾5������Ҫ��֤(0x02)���ǲ���Ҫ��֤(0x00)
/*	+------+-------------------+------------+
����|VER   | Number of METHODS | METHODS    |
����+------+-------------------+------------+
����| 0x05 | 0x02 (����������) | 0x00 | 0x02|
����+------+-------------------+------------+	*/
	const char reqNego[4]={'\x05','\x02','\x00','\x02'};
	int nRet = send(r_sock, reqNego, 4, 0);
	if (nRet == SOCKET_ERROR) goto error_l;

// Setp 2: ��������������������ֽڵ�Э�̽��������Э�̽��
	FD_ZERO(&fdread);
	FD_SET(r_sock,&fdread);

// Last param set to NULL for blocking operation. (struct timeval*)
	if((nRet = select(0,&fdread,NULL,NULL,&tout)) == SOCKET_ERROR)
	{
		goto error_l;
	}

	nRcvd=0;
	nCount=0;
	while(true)
	{
		if(FD_ISSET(r_sock, &fdread))
		{
			//����sock[0]������������
			do{
				nRet = recv(r_sock, (char*)resNego + nRcvd, 2 - nRcvd,0);
				if(nRet==SOCKET_ERROR)
				{
					goto error_l;
				}
				nRcvd += nRet;
			}
			while((nRcvd < 2) && (++nCount < _TA_RETRY_TIMES));
			if(nRcvd >= 2) break;
		}
		if(nCount++ >= _TA_RETRY_TIMES){
			//return NC_E_PROXY_RECEIVE|WSAGetLastError();
			goto error_l;
		}
	}

	//��֤�Ƿ�ɹ�
	if(resNego[0] != 0x05 || (resNego[1] != 0x00 && resNego[1] != 0x02))
	{
		//return NC_E_PROXY_PROTOCOL_VERSION|WSAGetLastError();
		goto error_l;
	}

// Step 3: ����Э�̽���ж��Ƿ���Ҫ��֤�û��������0x02������Ҫ�ṩ��֤����֤���ֲο�RFC1929
	if(resNego[1] == 0x02)
	{
		// ��֧��
		/*
		// ��Ҫ������֤
		char reqAuth[513]={'\0'};
		BYTE byLenUser = (BYTE)strlen(m_szProxyUserName);
		BYTE byLenPswd = (BYTE)strlen(m_szProxyPassword);
		reqAuth[0]=0x01;
		reqAuth[1]=byLenUser;
		sprintf(&reqAuth[2],"%s",m_szProxyUserName);
		reqAuth[2+byLenUser]=byLenPswd;
		sprintf(&reqAuth[3+byLenUser],"%s",m_szProxyPassword);
		//Send authentication info
		int len = (int)byLenUser + (int)byLenPswd + 3;
		nRet=send(m_socTCP,(const char*)reqAuth,len,0);
		if (nRet==SOCKET_ERROR){return NC_E_PROXY_SEND|WSAGetLastError();}
		//Now : Response to the auth request
		char resAuth[2]={'\0'};
		int nRcvd=0,nCount=0;
		do{
		nRet = recv(m_socTCP,resAuth+nRcvd,2-nRcvd,0);
		if(nRet==SOCKET_ERROR){return NC_E_PROXY_RECEIVE|WSAGetLastError();}
		nRcvd += nRet;
		}
		while((nRcvd!=2)&&(++nCount<1000));
		if(nCount>=1000){return NC_E_PROXY_RECEIVE|WSAGetLastError();}
		if (resAuth[1]!=0) return NC_E_PROXY_AUTHORIZE;
		// ������֤ͨ����
		*/
		goto error_l;
	}

// Step 4: Э����ɣ���ʼ��������Զ�̷���������,�����ĸ�ʽ���£�
/*	+----+-----+-------+------+----------+----------+
����|VER | CMD |��RSV��| ATYP | DST.ADDR | DST.PORT |
����+----+-----+-------+------+----------+----------+
����| 1��| ��1 | 0x00  | ��1��| Variable |    2     |
����+----+-----+-------+------+----------+----------+	*/
// CMD==0x01 ��ʾ����, ATYP==0x01��ʾ����IPV4��ʽ��ַ��DST.ADDR��Զ�̷�������ַ��DST.PORT��Զ�̷������˿�
// �����Ҫ�����������ӣ�����Ҫ���������֮�󣬷���CMD==0x02�����󣬴���Ϊ�������һ���׽��ֽ����ⲿ����

	if (ptr_server_addr)
	{
		char reqSubNego[10]={'\x05','\x01','\x00','\x01','\x00','\x00','\x00','\x00','\x00','\x00'};

		*(unsigned long *)(&reqSubNego[4]) = ptr_server_addr->sin_addr.S_un.S_addr;
		*(unsigned short *)(&reqSubNego[8]) = ptr_server_addr->sin_port;

		nRet = send(r_sock,(const char *)reqSubNego, 10, 0);
		if (nRet == SOCKET_ERROR)
		{
			//return NC_E_PROXY_SEND|WSAGetLastError();
			goto error_l;
		}
	} else if (http_domain){
		int _name_size = lstrlenA(http_domain);
		if (_name_size > 255 || _name_size <= 0) goto error_l;

		int size = 4 + 1 + _name_size + 2;
		char *preqSubNego = new char[size];
		memcpy(preqSubNego, "\x05\x01\x00\x03", 4);
		((unsigned char *)preqSubNego)[4] = (unsigned char)_name_size;
		lstrcpyA(preqSubNego + 5, http_domain);
		*(unsigned short *)(preqSubNego + size - 2) = ntohs(80);

		nRet = send(r_sock, (const char *)preqSubNego, size, 0);
		if (nRet == SOCKET_ERROR)
		{
			delete[] preqSubNego;
			//return NC_E_PROXY_SEND|WSAGetLastError();
			goto error_l;
		}

		delete[] preqSubNego;
	} else {
		//��Ŀ������
		goto error_l;
	}

// Step 5: ���ն��������Ӧ����Ӧ����ʽ����
/*	+----+-----+-------+------+----------+----------+
����|VER | REP |��RSV��| ATYP | BND.ADDR | BND.PORT |
����+----+-----+-------+------+----------+----------+
����| 1��|��1��| 0x00  |��1   | Variable | ����2 �� |
����+----+-----+-------+------+----------+----------+	*/
// VER ������0x05, REP==0x00��ʾ�ɹ���ATYP==0x01��ʾ��ַ��IPV4��ַ��BND.ADDR �Ǵ���Ϊ����Զ�̷������󶨵ĵ�ַ��BND.PORT������׽��ֵĶ˿�
	
	//char resSubNego1[5]={'\0'};
	if(FD_ISSET(r_sock, &fdread))
	{
		nRcvd=0;
		nCount=0;

		do{
			nRet = recv(r_sock, resSubNego1 + nRcvd, 5 - nRcvd, 0);
			if(nRet == SOCKET_ERROR)
			{
				//return NC_E_PROXY_RECEIVE|WSAGetLastError();
				goto error_l;
			}
			nRcvd += nRet;
		}
		while((nRcvd < 5) && (++nCount < _TA_RETRY_TIMES));
		if(nCount >= _TA_RETRY_TIMES)
		{
			//return NC_E_PROXY_RECEIVE|WSAGetLastError();
			goto error_l;
		}
		if(resSubNego1[0] != 0x05 || resSubNego1[1] != 0x00)
		{
			//return NC_E_PROXY_PROTOCOL_VERSION_SUB|WSAGetLastError();
			goto error_l;
		}

		// type
		switch(resSubNego1[3])
		{
		case 0x01:
		{
			// IP v4 ǰ���Ѿ���ȡ��ip�ĵ�һ���ֽ�
			char resSubNego2[6] = {resSubNego1[4], 0};
			if(FD_ISSET(r_sock, &fdread))
			{
				nRcvd=0;
				nCount=0;

				do{
					int nRet = recv(r_sock, &resSubNego2[1] + nRcvd, 5 - nRcvd, 0);
					if(nRet==SOCKET_ERROR)
					{
						//return NC_E_PROXY_RECEIVE|WSAGetLastError();
						goto error_l;
					}
					nRcvd += nRet;
				}
				while((nRcvd < 5) && (++nCount < _TA_RETRY_TIMES));
				if(nCount >= _TA_RETRY_TIMES)
				{
					//return NC_E_PROXY_RECEIVE|WSAGetLastError();
					goto error_l;
				}
			}

			/*
			// �õ�����󶨵�ַ
			unsigned long  ulBINDAddr = *(unsigned long*)resSubNego2; // SOCKS BIND ADDR
			unsigned short usBINDPort = *(unsigned short*)&resSubNego2[4]; // SOCKS BIND PORT
			printf("%u.%u.%u.%u:%d\r\n", (unsigned char)((unsigned char *)&ulBINDAddr)[0],
				(unsigned char)((unsigned char *)&ulBINDAddr)[1],
				(unsigned char)((unsigned char *)&ulBINDAddr)[2],
				(unsigned char)((unsigned char *)&ulBINDAddr)[3], ntohs(usBINDPort));

			m_saiProxyBindTCP.sin_addr.S_un.S_addr=ulBINDAddr;
			m_saiProxyBindTCP.sin_port=usBINDPort;
			// �õ������󶨵�ַ
			int len = sizeof(m_saiHostTCP);
			getsockname(m_socTCP,(SOCKADDR*)&m_saiHostTCP,&len);

			printf("%u.%u.%u.%u:%d\r\n", (unsigned char)((unsigned char *)&m_saiHostTCP.sin_addr.S_un.S_addr)[0],
				(unsigned char)((unsigned char *)&m_saiHostTCP.sin_addr.S_un.S_addr)[1],
				(unsigned char)((unsigned char *)&m_saiHostTCP.sin_addr.S_un.S_addr)[2],
				(unsigned char)((unsigned char *)&m_saiHostTCP.sin_addr.S_un.S_addr)[3], ntohs(m_saiHostTCP.sin_port));
			*/
		}
			break;
		case 0x03:
		{
			// Domain name
			int nLen = resSubNego1[4] + 2;
			char* presSubNego2 = new char[nLen];
			if(FD_ISSET(r_sock, &fdread))
			{
				nRet= SOCKET_ERROR;
				nRcvd=0;
				nCount=0;

				do{
					nRet = recv(r_sock, presSubNego2 + nRcvd, nLen - nRcvd, 0);
					if(nRet == SOCKET_ERROR)
					{
						//return NC_E_PROXY_RECEIVE|WSAGetLastError();
						goto error_l;
					}
					nRcvd += nRet;
				}
				while((nRcvd < nLen)&&(++nCount < _TA_RETRY_TIMES));
				if(nCount >= _TA_RETRY_TIMES)
				{
					//return NC_E_PROXY_RECEIVE|WSAGetLastError();
					goto error_l;
				}
			}

			// �õ�����ʵ��������
			// �����Ǳ����
			// ��ʱ�õ�����Զ��������Domain Name

			//unsigned short usBINDPort = *(unsigned short*)(presSubNego2+nLen-2); // BIND PORT;

			delete[] presSubNego2;
			presSubNego2=NULL;
		}
			break;
		case 0x04:
		{
		// IP v6 ǰ���Ѿ���ȡ��ip�ĵ�һ���ֽ�
			char resSubNego2[18] = {resSubNego1[4], 0};
			if(FD_ISSET(r_sock, &fdread))
			{
				nRcvd=0;
				nCount=0;

				do{
					int nRet = recv(r_sock, &resSubNego2[1] + nRcvd, 17 - nRcvd, 0);
					if(nRet==SOCKET_ERROR)
					{
						//return NC_E_PROXY_RECEIVE|WSAGetLastError();
						goto error_l;
					}
					nRcvd += nRet;
				}
				while((nRcvd < 17) && (++nCount < _TA_RETRY_TIMES));
				if(nCount >= _TA_RETRY_TIMES)
				{
					//return NC_E_PROXY_RECEIVE|WSAGetLastError();
					goto error_l;
				}
			}

		//��ȡ�����������
		}
			break;
		default:
		
			//��ֵ�����
			goto error_l;
			break;
		}	// end of switch
	}
// ���ˣ������Ѿ��������ڴ��׽����Ͽɽ������ݵ��շ���

	*pSock = r_sock;
	return true;

error_l:
	closesocket(r_sock);
	return false;
}

bool CTANetBase::InitProxySocket(SOCKET *pSock, sockaddr_in *ptr_server_addr, char *http_domain)
{
	if (!m_use_proxy_) return false;
	if (!pSock) return false;

	switch (m_proxy_type_)
	{
	case kHttp:
		return InitHttpProxySocket(pSock);
		break;
	case kSocks4:
		if (ptr_server_addr)
			return InitSocks4ProxySocket(pSock, ptr_server_addr, NULL);
		else if (http_domain)
			return InitSocks4ProxySocket(pSock, NULL, http_domain);

		break;
	case kSocks5:
		if (ptr_server_addr)
			return InitSocks5ProxySocket(pSock, ptr_server_addr, NULL);
		else if (http_domain)
			return InitSocks5ProxySocket(pSock, NULL, http_domain);

		break;
	}

	return false;
}

void CTANetBase::SetCookie(LPCSTR lpszCookie)
{
	if (lpszCookie == NULL)
	{
		m_str_cookie_ = "";
		return;
	}

	if (strncmp(lpszCookie, "Cookie:", 7) != 0)
	{
		m_str_cookie_ = lpszCookie;
	} else {
		if (lpszCookie[7] == ' ')
			m_str_cookie_ = lpszCookie + 8;
		else
			m_str_cookie_ = lpszCookie + 7;
	}
}

LPCSTR CTANetBase::GetCookie()
{
	return m_str_cookie_.c_str();
}

const char* CTANetBase::GetUrlPrefix()
{
	return NULL;
}

const char* CTANetBase::GetUserAgent()
{
	return NULL;
}

//curl function api

curl_slist* CTANetBase::post_addheader(curl_slist *list)
{
	return addheader(list);
}

void CTANetBase::post_setopt(CURL *curl)
{
	setopt(curl);
}

curl_slist* CTANetBase::get_addheader(curl_slist *list)
{
	return addheader(list);
}

void CTANetBase::get_setopt(CURL *curl)
{
	setopt(curl);
}

curl_slist* CTANetBase::addheader(curl_slist *list)
{
	list = curl_slist_append(list, "Expect:");	// removing the Expect:100 content type
	return list;
}

void CTANetBase::setopt(CURL *curl)
{
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

	if (GetUserAgent()) {
		curl_easy_setopt(curl, CURLOPT_USERAGENT, GetUserAgent());
	}
	curl_easy_setopt(curl, CURLOPT_COOKIE, GetCookie());

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);

  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, DEFAULT_TIME_OUT);

	if (IsUseProxy()) {

		curl_easy_setopt(curl, CURLOPT_PROXY, m_proxy_str_address_.c_str());

		switch (m_proxy_type_) {
		case kHttp:
			//curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, true);
			curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
			break;
		case kSocks4:
			curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
			break;
		case kSocks5:
			curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
			break;
		}
		
	}
}

int CTANetBase::CurlRequestGet(LPCSTR url, CBuffer& response, CBuffer *header)
{
	CURL *curl;
	
	response.ClearBuffer();

	curl = curl_easy_init();

	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);

		return CurlRequestEx(false, curl, response, header);
	}

	return -1;
}

int CTANetBase::CurlRequestPost(LPCSTR url, LPCSTR postdata, CBuffer& response, CBuffer *header)
{
	CURL *curl;

	CBuffer bheader;

	response.ClearBuffer();

	curl = curl_easy_init();
		
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, url);

		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);

		//curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3);  
		//curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);

		return CurlRequestEx(true, curl, response, header);
	}

	return -1;
}

void CTANetBase::CurlSetWriteFunction(CURL *curl, CBuffer& response, CBuffer *header)
{
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlOnWriteData);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, CurlOnWriteData);

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)header);
}

int CTANetBase::CurlRequestEx(bool ispost, CURL *curl, CBuffer& response, CBuffer *header)
{
	int ret = -1;

	CURLcode retcode;
	CBuffer bheader;
	struct curl_slist *headerlist = NULL;

	if (curl)
	{
		if (ispost) {
			headerlist = post_addheader(headerlist);
		} else {
			headerlist = get_addheader(headerlist);
		}

    CurlSetWriteFunction(curl, response, &bheader);

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
		//curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");

		if (ispost) {
			post_setopt(curl);
		} else {
			get_setopt(curl);
		}

		//curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3);  
		//curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);

		retcode = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		curl_slist_free_all(headerlist);

		if (retcode != CURLE_OK) {
			ret = retcode;
			return ret;
		}
	}

	bheader.WriteZeroByte();
	HttpProcessContent(bheader, response);

	if (header) {
		header->SetBufferLink(bheader.GetBuffer(), bheader.GetBufferLen(), bheader.GetMemSize());
		bheader.SetDestoryFree(false);
	}

	ret = TA_NET_OK;

	return ret;
}
