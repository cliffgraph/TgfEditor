#pragma once

#include "pch.h"

class CUdpSocket
{
private:
	static const int SIZE_RECVBUFF;
	bool						m_bEffective;
	int							m_myPortNo;			// 自身のポート番号
	SOCKET						m_socketID;			// ソケット
	//
	uint8_t						*m_aRecvBuff;		// 受信データ格納領域
	size_t						m_sizeRecv;			// 受信データサイズ

	uint32_t					m_srcIpAddr;
	uint16_t					m_srcPortNo;
	uint32_t					m_destIpAddr;
	uint16_t					m_destPortNo;

	int							m_errorNo;			// errno の値

#ifdef	__linux
	fd_set						m_readFds, m_writeFds;	// ディスクリプタ
#endif

public:
	CUdpSocket();
	virtual ~CUdpSocket();

public:
	bool Open(const int myPortNo, const bool bEnable);
	bool Close();
	void SetDestinationAddress(const tstring &ipaddr, const int portNo);
	void SetDestinationAddress(const uint32_t ipaddr, const int portNo);
	bool SendBinary(const uint8_t *pData, const size_t size);
	bool GetReceiveDataPtr( uint8_t **ppBuff, size_t *pSize, uint32_t *pSrcIp, uint16_t *pSrcPort);
	void ClearReceiveData();
	bool SetSizeOfReceiveBuffer(const size_t s);
	bool SetSizeOfSendBuffer(const size_t s);
	size_t GetSizeOfReceiveBuffer();
	size_t GetSizeOfSendBuffer();

private:
	bool writeData(const uint8_t *pData, const size_t size);
	bool readData(
		uint8_t *pReadData, const size_t buffSize,
		size_t *pReadSize, uint32_t *pSrcIp, uint16_t *pSrcPort);
	bool checkSendSignalGreen();
	bool checkRecvedSignalGreen();

public:
	static uint32_t IpStrings2SockaddrBin(const wchar_t *pBuff);
	static void ConvSockAddrToIpAddr(const sockaddr &addr, uint32_t *pIp, uint16_t *pPort);
};


