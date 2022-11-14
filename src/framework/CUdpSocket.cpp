#include "pch.h"
#include "CUdpSocket.h"

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

// #define SOCKDEB

const int CUdpSocket::SIZE_RECVBUFF = 1*1024*1024;

// -----------------------------------------------------------------------------------------------
/** コンストラクタ
*/
CUdpSocket::CUdpSocket()
{
	m_bEffective = false;	
	m_myPortNo = 0;
	m_aRecvBuff = NNEW uint8_t[SIZE_RECVBUFF];
	m_sizeRecv = 0;
	m_errorNo = 0;

#ifdef __linux
	/* ファイルディスクリプタリストクリア */
	FD_ZERO(&m_readFds);
	FD_ZERO(&m_writeFds);
#endif

#ifdef _WIN32
	// Winsock の初期化
	WSADATA	wsaData;
	WSAStartup(0x0101, &wsaData);	// Winsock Ver1.01
#endif
	return;
}

/** デストラクタ
*/
CUdpSocket::~CUdpSocket()
{
	NDELETEARRAY(m_aRecvBuff);
	return;
}

/** 受信データが格納されている領域へのポインタと、受信データサイズ、IPアドレスを返す
* @return false=受信データなし
*/
bool CUdpSocket::GetReceiveDataPtr(uint8_t **ppBuff, size_t *pSize, uint32_t *pSrcIp, uint16_t *pSrcPort)
{
	if( readData(m_aRecvBuff, SIZE_RECVBUFF, &m_sizeRecv, pSrcIp, pSrcPort) )
	{
		if( 0 < m_sizeRecv )
			*ppBuff = m_aRecvBuff;
		*pSize = m_sizeRecv;
		return (*pSize == 0) ? false : true;
	}
	return false;
}

/** 受信データをクリアする
*/
void CUdpSocket::ClearReceiveData()
{
	m_sizeRecv = 0;	// サイズを0にするだけ

}

/** ポートをオープン（バインド）する Member of IUdpPacketCarrier
* @param myPortNo ポート番号
* @param bEnable TRUE=ブロードキャストを許可するソケットを開く
*/
bool CUdpSocket::Open(const int myPortNo, const bool bEnable)
{
	memset( m_aRecvBuff, '\0', SIZE_RECVBUFF);

#ifdef	__linux
	m_socketID = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in	bindinfo;
	memset(&bindinfo, 0, sizeof bindinfo);
	bindinfo.sin_family        = AF_INET;
	bindinfo.sin_addr.s_addr   = htonl(INADDR_ANY);
	bindinfo.sin_port          = (unsigned short)htons( (unsigned short)myPortNo );
	if( bind(m_socketID, reinterpret_cast<sockaddr *>(&bindinfo), sizeof(bindinfo)) < 0)
	{
		close(m_socketID);
		m_socketID = 0;
		#ifdef	SOCKDEB
			wprintf(_T("UDP bind() error. PortNo.%d\n"), myPortNo);
		#endif
		return false;
	}
	else
	{
		#ifdef	SOCKDEB
			const int rbuff = static_cast<unsigned int>(GetSizeOfReceiveBuffer());
			const int sbuff = static_cast<unsigned int>(GetSizeOfSendBuffer());
			wprintf(_T("Open the UDP-PortNo.%d : Success.  RecvBuffSz:%u, SendBuffSz:%u\n"), myPortNo, rbuff, sbuff);
		#endif
	}
	FD_SET(m_socketID, &m_readFds);
	FD_SET(m_socketID, &m_writeFds);
	if( bEnable )
	{
		int val = 1;
		setsockopt(m_socketID, SOL_SOCKET, SO_BROADCAST, &val, sizeof val);
	}
#endif

#ifdef _WIN32
	m_socketID = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in	bindinfo;
	memset(&bindinfo, 0, sizeof bindinfo);
	bindinfo.sin_family        = AF_INET;
	bindinfo.sin_addr.s_addr   = htonl(INADDR_ANY);
	bindinfo.sin_port          = htons(myPortNo);
	u_long arg = 1L;
	ioctlsocket(m_socketID, FIONBIO, &arg );			// 非ブロッキング化
	if( bind(m_socketID, reinterpret_cast<sockaddr *>(&bindinfo), sizeof(bindinfo)) < 0)
	{
		closesocket(m_socketID);
		m_socketID = 0;
		#ifdef	SOCKDEB
			_tprintf(_T("UDP bind() error. PortNo.%d\n"), myPortNo);
		#endif
		return false;
	}
	else
	{
		#ifdef	SOCKDEB
			_tprintf(_T("Open the UDP-PortNo.%d : Success.\n"), myPortNo);
		#endif
	}
	if( bEnable )
	{
		int val = 1;
		setsockopt(m_socketID, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&val), sizeof val);
	}
#endif

	m_myPortNo = myPortNo;
	m_bEffective = true;
	return true;
}

/** ポートをクローズする Member of IUdpPacketCarrier
* @return true 成功
* @return false 失敗
*/
bool CUdpSocket::Close()
{
#ifdef	__linux
	FD_ZERO(&m_readFds);
	FD_ZERO(&m_writeFds);
	close(m_socketID);
	m_socketID = 0;
#endif
#ifdef _WIN32
	closesocket(m_socketID);
	m_socketID = 0;
#endif
	m_bEffective = false;
	return true;
}

/** 送信先アドレスをセットする
* @param ipadder 送信先IPアドレス文字列
* @param portNo 送信先ポート番号
* @return none
*/
void CUdpSocket::SetDestinationAddress(const tstring &ipaddr, const int portNo)
{
	m_destPortNo = static_cast<unsigned short>(portNo);
	m_destIpAddr = IpStrings2SockaddrBin(ipaddr.c_str());
	return;
}
void CUdpSocket::SetDestinationAddress(const uint32_t ipaddr, const int portNo)
{
	m_destPortNo = static_cast<unsigned short>(portNo);
	m_destIpAddr = ipaddr;
	return;
}

/** 送信する
* @param pData データへのポインタ
* @param size データバイトサイズ
* @return none
*/
bool CUdpSocket::SendBinary(const uint8_t *pData, const size_t size)
{
	if(!m_bEffective)
		return false;
	return writeData(pData, size);
}


/** ソケットの受信バッファサイズを変更する
 @param s サイズ(64kbyte以下であること）
*/
bool CUdpSocket::SetSizeOfReceiveBuffer(const size_t size)
{
	assert( m_socketID != 0 );
	assert(size <= size_t(1024*1024*1));
	if(!m_bEffective)
		return false;
#ifdef _WIN32
	setsockopt(
		m_socketID, SOL_SOCKET, SO_RCVBUF,
		reinterpret_cast<const char*>(&size), sizeof size);
#endif
#ifdef __linux
	const socklen_t ssize = static_cast<socklen_t>(sizeof size);
	setsockopt(
		(int)m_socketID, SOL_SOCKET, SO_RCVBUF, &size, ssize);
#endif
	return true;
}

/** ソケットの送信バッファサイズを変更する
 @param s サイズ(64kbyte以下であること）
*/
bool CUdpSocket::SetSizeOfSendBuffer(const size_t size)
{
	assert(m_socketID != 0);
	assert(size <= size_t(1024*1024*1));
	if(!m_bEffective)
		return false;
#ifdef _WIN32
	setsockopt(m_socketID, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&size), sizeof size);
#endif
#ifdef __linux
	const socklen_t ssize = static_cast<socklen_t>(sizeof size);
	setsockopt((int)m_socketID, SOL_SOCKET, SO_SNDBUF, &size, ssize);
#endif
	return true;
}

/** ソケットの受信バッファサイズを返す
 @return サイズ(byte)
*/
size_t CUdpSocket::GetSizeOfReceiveBuffer()
{
	if(!m_bEffective)
		return 0;
#ifdef _WIN32
	int sizelen, size;
	sizelen = sizeof(size);
	getsockopt(m_socketID, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&size), &sizelen);
	return(size);
#endif
#ifdef __linux
	socklen_t ssizelen, ssize;
	ssizelen = sizeof(ssize);
	getsockopt((int)m_socketID, SOL_SOCKET, SO_RCVBUF, &ssize, &ssizelen);
	return( static_cast<size_t>(ssize));
#endif
}

/** ソケットの送信バッファサイズを返す
 @return サイズ(byte)
*/
size_t CUdpSocket::GetSizeOfSendBuffer()
{
	if(!m_bEffective)
		return 0;
#ifdef _WIN32
	int sizelen, size;
	sizelen = sizeof(size);
	getsockopt(m_socketID, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&size), &sizelen);
	return(size);
#endif
#ifdef __linux
	socklen_t ssizelen, ssize;
	ssizelen = sizeof(ssize);
	getsockopt((int)m_socketID, SOL_SOCKET, SO_SNDBUF, &ssize, &ssizelen);
	return( static_cast<size_t>(ssize));
#endif
}

/** ソケットへ送信データを書き込む（つまり送信するということ）
* @param pData データへのポインタ
* @param size データバイトサイズ
* @return none
*/
bool CUdpSocket::writeData(const uint8_t *pData, const size_t size)
{
	assert(m_socketID != 0);
	assert(pData != NULL);
	assert(size <= size_t(SIZE_RECVBUFF));

#ifdef	__linux
	if( !checkSendSignalGreen() )
		return false;
	sockaddr_in	info;
	memset(&info, 0, sizeof info);
	info.sin_family        = AF_INET;
	info.sin_addr.s_addr   = static_cast<in_addr_t>(htonl(m_destIpAddr));
	info.sin_port          = static_cast<unsigned short>(htons(m_destPortNo));
	ssize_t result = sendto(m_socketID, pData, size, 0/*flags*/, reinterpret_cast<sockaddr *>(&info), sizeof info);
	m_errorNo = errno;
	if( result == -1 && m_errorNo != EAGAIN )
	{
		#ifdef	SOCKDEB
			tostringstream oss;
			oss << "sendto() error. ErrorNo." << m_errorNo << " in My PortNo. " << m_myPortNo << "\n";
			wprintf(_T("%ls\n"), oss.str().c_str());
		#endif
		return false;
	}
#endif
#ifdef _WIN32
	sockaddr_in	info;
	memset(&info, 0, sizeof info);
	info.sin_family        = AF_INET;
	info.sin_addr.s_addr   = static_cast<u_long>(htonl(m_destIpAddr));
	info.sin_port          = htons(m_destPortNo);
	if( sendto(m_socketID, reinterpret_cast<const char*>(pData), static_cast<int>(size), 0, reinterpret_cast<sockaddr *>(&info), sizeof info) == SOCKET_ERROR )
	{
		m_errorNo = WSAGetLastError();
		#ifdef	SOCKDEB
			tostringstream oss;
			oss << "sendto() error. ErrorNo." << m_errorNo << " in My PortNo." << m_myPortNo << "\n";
			_tprintf(oss.str().c_str());
		#endif
		return false;
	}
#endif
	return true;
}

/** ソケットから受信データを読み込む
* @param pData 格納データへのポインタ
* @param size 格納データ領域のバイトサイズ
* @return false=受信データなし
*/
bool CUdpSocket::readData(
	uint8_t *pReadData, const size_t buffSize, size_t *pReadSize,
	uint32_t *pSrcIp, uint16_t *pSrcPort)
{
	assert(m_socketID != 0);
	assert(pReadData != NULL);
	if(!m_bEffective)
		return false;
#ifdef	__linux
	if( !checkRecvedSignalGreen() )
		return false;
	sockaddr_in	info;
	socklen_t length = sizeof(info);
	int rsize = recvfrom(m_socketID, reinterpret_cast<char*>(pReadData), buffSize, 0, reinterpret_cast<sockaddr *>(&info), &length);
	m_errorNo = errno;
	if( rsize <= 0 && m_errorNo != EAGAIN && m_errorNo != EINTR )
	{
		#ifdef	SOCKDEB
			wprintf(_T("Sock Read Error No.%d\n"), m_errorNo);
		#endif
		return false;
	}
	ConvSockAddrToIpAddr(*(reinterpret_cast<sockaddr*>(&info)), &m_srcIpAddr, &m_srcPortNo);
	*pReadSize = (rsize <= 0 ) ? 0 : (size_t)rsize;
	return (*pReadSize == 0)?false:true;
#endif
#ifdef	_WIN32
	sockaddr_in	info;
	int length = sizeof(info);
	size_t rsize = recvfrom(m_socketID, reinterpret_cast<char*>(pReadData), static_cast<int>(buffSize), 0, reinterpret_cast<sockaddr *>(&info), &length);
	m_errorNo = WSAGetLastError();
	if( m_errorNo == WSAEWOULDBLOCK )
		return false;
	if( rsize <= 0 && m_errorNo != EAGAIN && m_errorNo != EINTR )
	{
		#ifdef	SOCKDEB
			_tprintf(_T("Sock Read Error No.%d\n"), m_errorNo);
		#endif
		return false;
	}
	ConvSockAddrToIpAddr(*(reinterpret_cast<sockaddr*>(&info)), &m_srcIpAddr, &m_srcPortNo);
	*pReadSize = (rsize <= 0 ) ? 0 : rsize;
	return (*pReadSize == 0)?false:true;
#endif
}

/**  データ送信準備できているか確認
*/
bool CUdpSocket::checkSendSignalGreen()
{
	if(!m_bEffective)
		return false;
#ifdef	__linux
	fd_set	testfds = m_writeFds;
	TIMEVAL	timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	int result = select(m_socketID+1, NULL, &testfds, NULL, &timeout );
	if( result < 0 && errno != EINTR )
		return false;
	if( 1 <= result )
		return true;
	return false;
#endif
#ifdef	_WIN32
	return true;
#endif
}

/**  データ受信準備できているか確認
*/
bool CUdpSocket::checkRecvedSignalGreen()
{
	if(!m_bEffective)
		return false;
#ifdef	__linux
	fd_set	testfds = m_readFds;
	TIMEVAL	timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	int result = select(m_socketID+1, &testfds, NULL, NULL, &timeout );
	if( result < 0 && errno != EINTR )
		return false;
	if( 1 <= result )
		return true;
	return false;
#endif
#ifdef	_WIN32
	return true;
#endif
}

uint32_t CUdpSocket::IpStrings2SockaddrBin(const wchar_t *pBuff)
{
	int	b1 = 0, b2 = 0, b3 = 0, b4 = 0;

#ifdef _WIN32
	if( _stscanf_s( pBuff, _T("%d.%d.%d.%d"), &b1, &b2, &b3, &b4 ) != 4 )
		return(false);
#endif
#ifdef __linux
#ifdef _UNICODE
	if( swscanf( pBuff, _T("%d.%d.%d.%d"), &b1, &b2, &b3, &b4 ) != 4 )
		return(false);
#else
	if( scanf( pBuff, _T("%d.%d.%d.%d"), &b1, &b2, &b3, &b4 ) != 4 )
		return(false);
#endif
#endif
	uint32_t addr = (uint32_t)(((uint8_t)b1 << 24) |
					((uint8_t)b2 << 16) |
					((uint8_t)b3 << 8) |
					((uint8_t)b4 << 0));
	return(addr);
}

void CUdpSocket::ConvSockAddrToIpAddr(const sockaddr &addr, uint32_t *pIp, uint16_t *pPort)
{
	switch(addr.sa_family)
	{
		case AF_INET:
		{
			const sockaddr_in *pAddr = reinterpret_cast<const sockaddr_in *>(&addr);
			const uint8_t *pIpAddr = reinterpret_cast<const uint8_t *>(&(pAddr->sin_addr));
			*pIp =	(uint32_t)(((uint8_t)pIpAddr[0] << 24) |
					((uint8_t)pIpAddr[1] << 16) |
					((uint8_t)pIpAddr[2] << 8) |
					((uint8_t)pIpAddr[3] << 0));
			*pPort = (uint16_t)htons(pAddr->sin_port);
			break;
		}
		default:
			*pIp = 0;
			*pPort = 0;
			break;
	}
	return;
}


