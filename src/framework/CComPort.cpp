#include "pch.h"
#include "CComPort.h"
#include "setupapi.h"
#include "tdebug.h"
#include <winioctl.h>
#include <process.h> 

#pragma comment(lib,"setupapi.lib")

void CComPort::GetListupComs(std::vector<COMM_NAMES> *pList)
{
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, 0, 0, DIGCF_PRESENT| DIGCF_DEVICEINTERFACE);
 
	for (int t = 0; true; ++t) {
	 	SP_DEVINFO_DATA DeviceInfoData;
		DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		if (!SetupDiEnumDeviceInfo(hDevInfo, t, &DeviceInfoData) )
			break;

		HKEY hKey = SetupDiOpenDevRegKey(hDevInfo, &DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV,KEY_QUERY_VALUE);
		if (hKey != INVALID_HANDLE_VALUE) {
			BYTE buf[256];
			DWORD type=0;
			DWORD len = sizeof(buf);
			// "COM#" 名称 -> sname
			RegQueryValueEx(hKey,_T("PortName"), nullptr, &type, (LPBYTE)buf, &len);
			const tstring sname(reinterpret_cast<TCHAR *>(buf));
			if (sname.find_first_of(_T("COM")) != tstring::npos) {
				// "説明(COM#)" -> fname
				if (SetupDiGetDeviceRegistryProperty(
						hDevInfo, &DeviceInfoData, SPDRP_FRIENDLYNAME, &type, buf, sizeof(buf), nullptr)) {
					const tstring fname(reinterpret_cast<TCHAR *>(buf));
					COMM_NAMES ns = {fname, sname};
					pList->push_back(ns);
				}
			}
		}
	}
	return;
}

CComPort::CComPort()
{
	m_PortName.clear();
	m_hComm = INVALID_HANDLE_VALUE;
	m_hWnd = 0;
	return;
}

CComPort::~CComPort()
{
	if (m_hComm != INVALID_HANDLE_VALUE)
		ClosePort();
	return;
}


void CComPort::launcherForRecvThread(void *pArgs)
{
	auto *pSts = reinterpret_cast<CComPort*>(pArgs);
	pSts->recvThread();
	return;
}

void CComPort::recvThread()
{
	m_bThreadAlive = true;

	while( m_bThreadAlive ) {
		DWORD commEvnt = 0;
		if( WaitCommEvent(m_hComm, &commEvnt, NULL) != 0 )
		{
			//if( GetLastError() != ERROR_IO_PENDING)
			//	continue;
		}
		DWORD waitResult = WaitForMultipleObjects(2, &m_hThreadEvents[0], FALSE, INFINITE );
		if( waitResult == WAIT_OBJECT_0 )
			break; /* Thread finish */
		if (waitResult == WAIT_OBJECT_0 + 1)
		{
			DWORD size;
			if (GetOverlappedResult(m_hComm, &m_Ovr, &size, FALSE) != 0) {
				if (size == 0)
					continue;
				if (m_hWnd != INVALID_HANDLE_VALUE)
					SendMessage(m_hWnd, WMA_COMMRECVED, 0, 0);
			}
			else {
				ResetEvent(m_Ovr.hEvent);
			}
		}
	}

	// 終了を親に通知
	SetEvent(m_hThreadEnd);

	// スレッドの終了
	_endthread();
	return;
}


bool CComPort::OpenPort(HWND hWnd, const tstring &name)
{
	_tcscpy_s(m_DCB.devName, COMM_DCB::LEN_NAME, name.c_str());
	TCHAR comName[255 +1];
	_stprintf_s(comName, sizeof(comName) / sizeof(comName[0]), _T("\\\\.\\%s"), name.c_str());

	HANDLE hCom = CreateFile(
		comName, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
		OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

	if (hCom == INVALID_HANDLE_VALUE) {
		// MessageBox(hWnd, comname, _TEXT("Open Error"), MB_OK);
		return false;
	}

	m_hWnd = hWnd;
	m_PortName = name;

	DWORD DErr;
	ClearCommError(hCom, &DErr, nullptr);
	PurgeComm(hCom, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

	//
	auto &dcb = m_DCB.config.dcb;
	GetCommState(hCom, &dcb);
	dcb.DCBlength = sizeof(DCB);
	dcb.BaudRate = CBR_9600;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fBinary = true;
	SetCommState( hCom, &dcb );

	// 
	COMMTIMEOUTS timeout;
	memset(&timeout, 0, sizeof timeout);
	timeout.ReadIntervalTimeout = MAXDWORD;
	timeout.ReadTotalTimeoutConstant = 0;
	timeout.ReadTotalTimeoutMultiplier = 0;
	timeout.WriteTotalTimeoutConstant = 0;
	timeout.WriteTotalTimeoutMultiplier = 0;
	SetCommTimeouts(hCom, &timeout);

	m_hThreadEvents[0]	= CreateEvent(nullptr, true, false, nullptr);		// スレッド終了要求イベント
	m_hThreadEvents[1]	= CreateEvent(nullptr, true, false, nullptr);		// 受信イベント
	m_hThreadEnd		= CreateEvent(nullptr, true, false, nullptr);		// スレッド終了しました通知イベント

	memset(&m_Ovr, 0, sizeof m_Ovr);
	m_Ovr.hEvent = m_hThreadEvents[1];

	m_hComm = hCom;

	// 受信のみを監視する
	SetCommMask(hCom, EV_RXCHAR);

	_beginthread(CComPort::launcherForRecvThread, 0, this);
	return true;
}

void CComPort::ClosePort()
{
	if (m_hComm != INVALID_HANDLE_VALUE)
	{
		// スレッドに終了を通知して、終了を待つ
		m_hWnd = (HWND)INVALID_HANDLE_VALUE;
		m_bThreadAlive = false;
		SetCommMask(m_hComm, 0);
		SetEvent(m_hThreadEvents[0]);
		WaitForSingleObject(m_hThreadEnd, INFINITE);
		//
		PurgeComm(m_hComm, PURGE_RXCLEAR);
		CloseHandle(m_hComm);
		//
		CloseHandle(m_hThreadEvents[0]);
		CloseHandle(m_hThreadEvents[1]);
		CloseHandle(m_hThreadEnd);
		m_hComm = m_hThreadEnd = INVALID_HANDLE_VALUE;
		m_hThreadEvents[0] = m_hThreadEvents[1] = INVALID_HANDLE_VALUE;
		m_PortName.clear();
	}
	return;
}

size_t CComPort::Send(const uint8_t data)
{
	assert(m_hComm != 0);
	DWORD sendSize;
	BOOL ret = ::WriteFile(m_hComm, &data, 1, &sendSize, &m_Ovr);
	if (ret == FALSE)
		sendSize = 0;
	return sendSize;
}

size_t CComPort::Send(const uint8_t *pData, const size_t sz)
{
	assert(m_hComm != 0);
	DWORD sendSize;
	BOOL ret = ::WriteFile(m_hComm, pData, static_cast<DWORD>(sz), &sendSize, &m_Ovr);
	if (ret == FALSE)
		sendSize = 0;
	return sendSize;
}

size_t CComPort::Receive(uint8_t *pData, const size_t areaSize)
{
	assert(m_hComm != 0);
	DWORD readSize;
	BOOL ret = ::ReadFile(m_hComm, pData, static_cast<DWORD>(areaSize), &readSize, &m_Ovr);
	if (ret == FALSE)
		readSize = 0;
	return readSize;
}

bool CComPort::IsOpend() const
{
	return (m_hComm != INVALID_HANDLE_VALUE);
}

const tstring &CComPort::GetPortName() const
{
	return m_PortName;
}

