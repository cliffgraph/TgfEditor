#pragma once
#include <vector>
#include <string>

struct COMM_DCB
{
    static const int LEN_NAME = 8;
    TCHAR       devName[LEN_NAME];
    COMMCONFIG  config;
};

struct COMM_NAMES
{
	tstring Name;
	tstring	ShortName;
};

class CComPort
{
private:
	tstring			m_PortName;
	COMM_DCB		m_DCB;
	HWND			m_hWnd;
	HANDLE			m_hComm;
//
	HANDLE			m_hThreadEnd;
	bool			m_bThreadAlive;
	OVERLAPPED		m_Ovr;
	HANDLE			m_hThreadEvents[2];

public:
	CComPort();
	virtual ~CComPort();

private:
	static void launcherForRecvThread(void *pArgs);
	void recvThread();

public:
	static void GetListupComs(std::vector<COMM_NAMES> *pList);

public:
	bool OpenPort(HWND hWnd, const tstring &name);
	void ClosePort();
	size_t Send(const uint8_t data);
	size_t Send(const uint8_t *pData, const size_t sz);
	size_t Receive(uint8_t *pData, const size_t areaSize);
	bool IsOpend() const;
	const tstring &GetPortName() const;

};

