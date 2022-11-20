// Winps2vkbd.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "pch.h"
#include "framework.h"
#include "TgfEditor.h"
#include <Dbt.h>    	// for DBT_DEVNODES_CHANGED
#include <winnls32.h>	// for WINNLSEnableIME()
#include "tdebug.h"
#include "CD2d.h"
#include "tools.h"
#include "tgf.h"
#include "tgp.h"
#include "sndregs.h"
#include "CUdpSocket.h"
#include <set>
#include <commdlg.h>	// GetOpenFileName
#include <vector>
#include <list>
#include <thread>
#include <timeapi.h>	// for timeBeginPeriod
#include "CGemBox.h"
#include "CGemsButton.h"

#pragma comment(lib,"Winmm.lib")	// for timeBeginPeriod()


#define MAX_LOADSTRING 100

static const float TL_X = 0;			// TL欄の左上座標
static const float TL_Y = 80;
static const float TL_MargineL = 10.f;	// TL欄内の左右マージン
static const float TL_MargineR = 10.f;
static const float TL_Height = 100.f;

static const int ID_BTN_PLAY = 1;
static const int ID_BTN_STOP = 2;
static const int ID_BTN_GOTO_TOP = 3;


// グローバル変数:
static HINSTANCE g_hInst;                                // 現在のインターフェイス
static WCHAR g_szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
static WCHAR g_szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名
static HWND	g_hWnd = 0;
static bool	g_bFocus = false;
static CD2d *g_pD2d = nullptr;
static int g_TopIndex = 0;
static int g_CursorIndex = 0;
static int g_SelectStartCursorIndex = -1;
static int g_SelectEndCursorIndex = -1;
static bool g_bPlaingMusic;

enum class DRAG_STATUS
{
	NONE,
	VISUALIZER,
	SCROLL_KNOB,
};

static DRAG_STATUS g_MouseDraggingStatus = DRAG_STATUS::NONE;
static int g_BeginScrollIndex;
static int g_BeginScrollMX;

struct OPTS {
	tstring DefaultFilePath;
	tstring TgfpIpAddr;
	uint16_t TgfpPort;
	OPTS() : TgfpPort(50000) {}
};
static OPTS g_Opts;

static std::thread g_ThreadTgp;
static std::atomic_bool g_CtrlThreadPlayMusic;
static std::atomic_bool g_CtrlThreadStopMusic;
static std::atomic_bool g_CtrlThread;
static std::atomic_int	g_ThCursurIndex;
static std::atomic_int	g_ThPlaingIndex;
static CGemBox g_GemBox;


// 初期ウィンドウサイズ
static const int WINDOW_W = 1094;
static const int WINDOW_H = 420;

struct Volume {
	uint8_t		v8;
	float		vf;
	bool		mod;
};

struct TgfRecord {
	tgf::timecode_t			tc;
	std::vector<tgf::ATOM>	*pAtoms;
	SOUND_REGISTERS			regs;
	SOUND_REGISTERS_MOD		regs_mod;
	Volume					volume_opll;
	Volume					volume_psg;
	Volume					volume_scc;
	Volume					volume_sccplus;
	bool					bSccAccess;
	bool					bSccPlusAccess;
	bool					bDeleteMaker;
	TgfRecord() : tc(0), bSccAccess(false), bSccPlusAccess(false), bDeleteMaker(false)
	{
		pAtoms = NNEW std::vector<tgf::ATOM>();
	}
	~TgfRecord()
	{
		NDELETE(pAtoms);
	}
};

struct TGFDATA {
	tstring FilePath;
	tstring FileName;
	std::vector<uint8_t> *pRawData;
	std::vector<TgfRecord*> tracks;
	TGFDATA() : pRawData(nullptr) {}
	~TGFDATA()
	{
		NDELETE(pRawData);
		ClearTracks();
	}
	void ClearTracks()
	{
		for( auto p : tracks )
			NDELETE(p);
		tracks.clear();
		return;
	}
};
static TGFDATA g_Tgf;

/**
 * 再描画の要求
*/
inline void REDRAW(const HWND hWnd)
{
	InvalidateRect(hWnd, nullptr, false);
}

static void getPosScrollKnob(
	float *pX, float *pY, float *pW, float *pH, float tlWidth, int topIndex, int numTracks)
{
	const float rwf = tlWidth / numTracks;
	*pX = TL_X + TL_MargineL + topIndex * rwf;
	*pY = TL_Y + TL_Height + 3;
	*pW = tlWidth * rwf;
	*pH = 14;
	return;
}


static void arrangeTrack(TGFDATA *pTgf)
{
	auto *pData = reinterpret_cast<const tgf::ATOM *>(pTgf->pRawData->data());
	const int numAtom = static_cast<int>(pTgf->pRawData->size()/sizeof(tgf::ATOM));

	tgf::timecode_t tc = 0, start_tc = 0, old_tc = 0;
	TgfRecord *pTr = NNEW TgfRecord();
	bool bInit = true;
	SOUND_REGISTERS regs;
	SOUND_REGISTERS_MOD regs_mod;
	regs.clear();
	regs_mod.clear();
	uint8_t oldPsg8 = 0, oldOpll8 = 0, oldScc8 = 0, oldSccPlus8 = 0;
	bool bScc = true;
	bool bSccAccess = false;
	bool bSccPlus = false;
	bool bSccPlusAccess = false;
	uint16_t sccModeReg = 0x00;
	for (int t = 0; t < numAtom; ++t) {
		const tgf::ATOM &dt = pData[t];
		switch(dt.mark){
			case tgf::MARK::TC:
			{
				pTr->regs = regs;
				pTr->regs_mod = regs_mod;
				pTr->bSccAccess = bSccAccess;
				pTr->bSccPlusAccess = bSccPlusAccess;
				pTr->volume_psg.v8 =
					(regs.psg[0x08]&0xf) + (regs.psg[0x09]&0xf) + (regs.psg[0x0a]&0xf);
				pTr->volume_psg.vf =
					pTr->volume_psg.v8 / (0x0f*3.f);
				pTr->volume_opll.v8 = 
					(0x0f*9) -
					((regs.opll[0x30]&0xf) +(regs.opll[0x31]&0xf) +(regs.opll[0x32]&0xf) +(regs.opll[0x33]&0xf)
					+(regs.opll[0x34]&0xf) +(regs.opll[0x35]&0xf) +(regs.opll[0x36]&0xf) +(regs.opll[0x37]&0xf)
					+(regs.opll[0x38]&0xf));
				pTr->volume_opll.vf =
					pTr->volume_opll.v8 / (0x0f * 9.f);

				int of;
				of = 0x8A;
				pTr->volume_scc.v8 =
					regs.scc[0x0+of] + regs.scc[0x1+of] + regs.scc[0x2+of] + regs.scc[0x3+of] + regs.scc[0x4+of];
				pTr->volume_scc.vf =
					pTr->volume_scc.v8 / (0x0f * 5.f);
				// SCC+
				of = 0xAA;
				pTr->volume_sccplus.v8 =
					regs.sccplus[0x0+of] + regs.sccplus[0x1+of] + regs.sccplus[0x2+of] + regs.sccplus[0x3+of] + regs.sccplus[0x4+of];
				pTr->volume_sccplus.vf =
					pTr->volume_sccplus.v8 / (0x0f * 5.f);

				pTr->volume_psg.mod		= (oldPsg8 != pTr->volume_psg.v8);
				pTr->volume_opll.mod	= (oldOpll8 != pTr->volume_opll.v8);
				pTr->volume_scc.mod		= (oldScc8 != pTr->volume_scc.v8);
				pTr->volume_sccplus.mod	= (oldSccPlus8 != pTr->volume_sccplus.v8);
				oldPsg8		= pTr->volume_psg.v8;
				oldOpll8	= pTr->volume_opll.v8;
				oldScc8		= pTr->volume_scc.v8;
				oldSccPlus8	= pTr->volume_sccplus.v8;

				pTgf->tracks.push_back(pTr);
				pTr = NNEW TgfRecord();
				bSccAccess = false;
				bSccPlusAccess = false;
				regs_mod.clear();
				// 
				pTr->tc = (static_cast<uint32_t>(dt.data1) << 16) | static_cast<uint32_t>(dt.data2);
				if (bInit) {
					start_tc = (0 < pTr->tc) ? (pTr->tc - 1) : 0;	// 最初のTCは1以上になるように、start_tcは決定されている
					bInit = false;
				}
				// もしTCが先祖返りしたらこれまでのデータは破棄してやり直す
				//		先祖が入りはログ開始後に本体の電源を入れるなどで発生する(ON時にリセットがかかり0に戻るから）
				if( pTr->tc < old_tc ){
					for(auto  *p : pTgf->tracks )
						NDELETE(p); 
					pTgf->tracks.clear();
					start_tc = (0 < pTr->tc) ? (pTr->tc - 1) : 0;
				}
				old_tc = pTr->tc;
				pTr->tc -= start_tc;
				break;
			}
			case tgf::MARK::OPLL:
			{
				pTr->pAtoms->push_back(dt);
				regs.opll[dt.data1] = static_cast<uint8_t>(dt.data2);
				regs_mod.opll[dt.data1] = true;
				break;
			}
			case tgf::MARK::PSG:
			{
				pTr->pAtoms->push_back(dt);
				regs.psg[dt.data1] = static_cast<uint8_t>(dt.data2);
				regs_mod.psg[dt.data1] = true;
				break;
			}
			case tgf::MARK::SCC:
			{
				pTr->pAtoms->push_back(dt);
				if (0x9000 <= dt.data1 && dt.data1 <= 0x97FF) {
					const bool temp = bScc;
					bScc = (((sccModeReg&0x20)==0x00) && (dt.data2 == 0x3F));
					if (!temp && bScc) {
						bSccPlus = false;
					}
				}
				else if (0xB000 <= dt.data1 && dt.data1 <= 0xB7FF) {
					const bool temp = bSccPlus;
					bSccPlus = (((sccModeReg & 0x30) == 0x20) && (dt.data2 == 0x80));
					if (!temp && bSccPlus) {
						bScc = false;
					}
				}
				else if (0xBFFE <= dt.data1 && dt.data1 <= 0xBFFF) {
					sccModeReg = dt.data2;
				}
				else if (bScc && (0x9800 <= dt.data1 && dt.data1 <= 0x98FF)) {
					int ad = dt.data1 - 0x9800;
					regs.scc[ad] = static_cast<uint8_t>(dt.data2);
					regs_mod.scc[ad] = true;
					bSccAccess = true;
				}
				else if (bSccPlus && (0xB800 <= dt.data1 && dt.data1 <= 0xB8DF)) {
					int ad = dt.data1 - 0xB800;
					regs.sccplus[ad] = static_cast<uint8_t>(dt.data2);
					regs_mod.sccplus[ad] = true;
					bSccPlusAccess = true;
				}
				break;
			}
			case tgf::MARK::NOP:
			case tgf::MARK::SYSINFO:
			case tgf::MARK::WAIT:
			default:
				// do nothing
				break;
		}
	}
	if( pTr !=  nullptr ){
		if( !pTr->pAtoms->empty() ){
			pTgf->tracks.push_back(pTr);
		}
		else{
			NDELETE(pTr);
		}
	}
	return;
}

static void convertToTgf(const TGFDATA &tgf, std::vector<uint8_t> *p)
{
	std::unique_ptr<std::vector<tgf::ATOM>> pAtoms(NNEW std::vector<tgf::ATOM>());
	tgf::timecode_t first_tc = 0;
	if( !tgf.tracks.empty() ){
		first_tc = tgf.tracks[0]->tc;
	}

	tgf::timecode_t tc = 0;
	tgf::timecode_t beginDelTc = 0;
	bool bDeleting = false;


	for( auto *pTr : tgf.tracks ){
		const TgfRecord &d = *pTr;
		if( d.bDeleteMaker ){
			if( !bDeleting ){
				bDeleting = true;
				beginDelTc = d.tc;
			}
			continue;
		}
		if( tc != d.tc ){
			tc = d.tc;
			if( bDeleting ){
				bDeleting = false;
				first_tc += d.tc - beginDelTc;
			}
			tgf::timecode_t tempTc = d.tc - first_tc;
			tgf::ATOM a(tgf::MARK::TC);
			a.data1 = (tempTc>>16)&0xFFFF;
			a.data2 = (tempTc>> 0)&0xFFFF;
			pAtoms->push_back(a);
		}
		for (auto& a : *d.pAtoms) {
			pAtoms->push_back(a);
		}
	}
	size_t bsz = pAtoms->size() * sizeof(tgf::ATOM);
	const uint8_t *pStr = reinterpret_cast<const uint8_t*>(pAtoms->data());
	for( size_t t = 0; t < bsz; ++t, ++pStr)
		p->push_back(*pStr);
	return;
}


static void stepTopIndex(int *pIndex, const int step, const int minv, const int maxv)
{
	*pIndex += step;
	if( *pIndex < minv )
		*pIndex = minv;
	else if( maxv < *pIndex )
		*pIndex = maxv;
	return;
}

static void thread_CtrlTgp(HWND hWnd)
{
	std::unique_ptr<CUdpSocket> pUdp(NNEW CUdpSocket());
	pUdp->Open(0, false);	// 0を指定してポート番号の割り当てはシステムに任せるものとする
	pUdp->SetDestinationAddress(g_Opts.TgfpIpAddr, g_Opts.TgfpPort);	// IP addiress of RaMsxMuse

	uint8_t *pRecvBuff;
	size_t recvSize;
	uint32_t srcIp;
	uint16_t srcPort;

	tgpk::PACKET temp;
	size_t hdsize = static_cast<size_t>(reinterpret_cast<uint8_t*>(&temp.atoms[0]) - reinterpret_cast<uint8_t*>(&temp.cmd));

	for(;;){
		if (g_CtrlThread.load(std::memory_order_acquire)) {
			break;
		}
		if (g_CtrlThreadPlayMusic.load(std::memory_order_acquire)) {
			g_CtrlThreadPlayMusic.store(false, std::memory_order_release);

			const int maxTraks = static_cast<int>(g_Tgf.tracks.size());
			int curIndex = g_ThCursurIndex.load(std::memory_order_acquire);

			// 再生要求を送信する
			temp.cmd = tgpk::CMD::REQUEST_PLAY;
			temp.index = curIndex;
			temp.maxIndex = maxTraks;
			pUdp->SendBinary(reinterpret_cast<const uint8_t*>(&temp), sizeof(temp));
		}
		if (g_CtrlThreadStopMusic.load(std::memory_order_acquire)) {
			g_CtrlThreadStopMusic.store(false, std::memory_order_release);
			// 再生停止を送信する
			temp.cmd = tgpk::CMD::REQUEST_STOP;
			pUdp->SendBinary(reinterpret_cast<const uint8_t*>(&temp), sizeof(temp));
		}

		if( !pUdp->GetReceiveDataPtr(&pRecvBuff, &recvSize, &srcIp, &srcPort) )
			continue;
		if( sizeof(tgpk::PACKET) <= recvSize){
			auto *p = reinterpret_cast<const tgpk::PACKET*>(pRecvBuff);
			switch(p->cmd)
			{
				case tgpk::CMD::REQUEST_ATOMS:
				{
					const int maxTraks = static_cast<int>(g_Tgf.tracks.size());
					const int requestIndex = p->index;
					if( requestIndex < 0 || maxTraks <= requestIndex )
						break;
					const TgfRecord *pRec = g_Tgf.tracks[requestIndex];
					const int numAtoms = static_cast<int>(pRec->pAtoms->size());
					const size_t sizePack = hdsize+sizeof(tgf::ATOM)*(numAtoms+1);
					std::unique_ptr<uint8_t[]> pSendDt(NNEW uint8_t[sizePack]);
					tgpk::PACKET *pPack = reinterpret_cast<tgpk::PACKET*>(pSendDt.get());
					pPack->cmd = tgpk::CMD::TG_ATOMS;
					pPack->index = requestIndex;
					pPack->maxIndex = maxTraks-1;
					pPack->num = numAtoms +1;
					//
					tgf::ATOM &atomTc = pPack->atoms[0];
					atomTc.mark = tgf::MARK::TC;
					atomTc.data1 = (pRec->tc >> 16) & 0xffff;
					atomTc.data2 = (pRec->tc >> 0) & 0xffff;
					//
					for(int t = 0; t < numAtoms; ++t )
						pPack->atoms[t+1] = pRec->pAtoms->at(t);
					pUdp->SendBinary(pSendDt.get(), sizePack);
					g_ThPlaingIndex.store(requestIndex, std::memory_order_release);
					PostMessage(hWnd, WMA_REDRAW, 0, 0);

				}
				case tgpk::CMD::NONE:				
				case tgpk::CMD::REQUEST_PLAY:		// TGFE -> TGFP, 再生を要求する	index=開始インデックス
				case tgpk::CMD::REQUEST_STOP:		// TGFE -> TGFP, 再生停止を要求する
				case tgpk::CMD::TG_ATOMS:			// TGFE -> TGFP, 再生データ、inde=データのインデックス
				default:
					break;
			}
		}
		pUdp->ClearReceiveData();
	}
	pUdp->Close();
	return;
}

static void readFile(const tstring fpath)
{
	g_Tgf.FilePath = fpath;
	std::unique_ptr<TCHAR[]> pTemp(NNEW TCHAR[g_Tgf.FilePath.size() + 1]);
	_tcscpy_s(pTemp.get(), g_Tgf.FilePath.size() + 1, g_Tgf.FilePath.c_str());
	PathStripPath(pTemp.get());
	g_Tgf.FileName = pTemp.get();
	//
	NDELETE(g_Tgf.pRawData);
	g_Tgf.ClearTracks();
	t_ReadFile(g_Tgf.FilePath, &g_Tgf.pRawData);
	arrangeTrack(&g_Tgf);
	return;
}


static void startMusic()
{
	g_ThCursurIndex.store(g_CursorIndex, std::memory_order_release);
	g_CtrlThreadPlayMusic.store(true, std::memory_order_release);
	g_bPlaingMusic = true;
	return;
}

static void stopMusic()
{
	g_CtrlThreadStopMusic.store(true, std::memory_order_release);
	g_CursorIndex = g_ThPlaingIndex.load(std::memory_order_acquire);
	g_bPlaingMusic = false;
	return;
}

static void gotoTop()
{
	g_TopIndex = g_CursorIndex = 0;
	g_SelectStartCursorIndex = g_SelectEndCursorIndex = -1;
	return;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_TGFEDITOR);
    wcex.lpszClassName = g_szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON1));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(
	const HINSTANCE hInstance, const int nCmdShow, const std::vector<tstring> &args)
{
	g_hInst = hInstance;
	 
	HWND hWnd = CreateWindowW(
        g_szWindowClass, g_szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
		WINDOW_W, WINDOW_H,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

	g_ThCursurIndex.store(g_CursorIndex, std::memory_order_release);
	g_ThPlaingIndex.store(0, std::memory_order_release);
	g_CtrlThreadPlayMusic.store(false, std::memory_order_release);
	g_CtrlThreadStopMusic.store(false, std::memory_order_release);
	g_CtrlThread.store(false, std::memory_order_release);
	g_bPlaingMusic = false;
    return TRUE;
}

static LRESULT handler_WM_CREATE(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	g_hWnd = hWnd;
	g_ThreadTgp = std::thread(&thread_CtrlTgp, hWnd);

	// -fで指定されたファイルの読み込み
	if( !g_Opts.DefaultFilePath.empty() && t_FileExist(g_Opts.DefaultFilePath) ) 
		readFile(g_Opts.DefaultFilePath);

	// IME Disable（IMEの文字入力ウィンドウが出てしまうのを抑止する）
	WINNLSEnableIME(hWnd, false);

	// Direct2D
	g_pD2d = NNEW CD2d();
	g_pD2d->SetRenderTargetWindow(hWnd, 200, 100);

	// Buttons
	const bool b = !g_Opts.TgfpIpAddr.empty();
	auto *pBtnTop = NNEW CGemsButton(hWnd, _T("TOP"), ID_BTN_GOTO_TOP);
	pBtnTop->SetLocate(10, 205, 80, 30);
	auto *pBtnPlay = NNEW CGemsButton(hWnd, _T("PLAY"), ID_BTN_PLAY, b);
	pBtnPlay->SetLocate(130, 205, 80, 30);
	auto *pBtnStop = NNEW CGemsButton(hWnd, _T("STOP"), ID_BTN_STOP, b);
	pBtnStop->SetLocate(220, 205, 80, 30);
	g_GemBox.Set(pBtnPlay);
	g_GemBox.Set(pBtnStop);
	g_GemBox.Set(pBtnTop);
	return 0;
}

static LRESULT handler_WM_CLOSE(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	g_CtrlThread.store(true, std::memory_order_release);	// スレッドの停止を要求
	g_ThreadTgp.join();
	DestroyWindow(hWnd);
	return 0;
}

static LRESULT handler_WM_COMMAND(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId = LOWORD(wParam);
	// 選択されたメニューの解析:
	switch (wmId)
	{
		case IDM_ABOUT:
		{
			INT_PTR CALLBACK DlgHdrAbout(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, DlgHdrAbout);
			break;
		}
		case IDM_OPEN_FILE:
		{
			// ファイルパスを指定してそれを読み込む
			TCHAR fpath[MAX_PATH+1] = { '\0' };
			OPENFILENAME ofn;
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hWnd;
			ofn.lpstrInitialDir = _T("");
			ofn.lpstrFile = fpath;
			ofn.nMaxFile = sizeof(fpath);
			ofn.lpstrFilter = _T("mdatファイル(*.mdat)\0*.mdat\0tgfdatファイル(*.tgfdat)\0*.tgfdat\0");
			ofn.lpstrDefExt = _T("mdat");
			ofn.lpstrTitle = _T("TGFファイルを指定");
			ofn.nFilterIndex = 1;
			if (GetOpenFileName(&ofn) != 0) {
				if( !t_FileExist(fpath) ){
					const tstring mess = tstring(_T("Not found : ")) + fpath;
					MessageBox(hWnd, mess.c_str(), _T("TgfEditor error"), MB_OK);
					g_Tgf.FilePath.clear();
				}
				else{
					stopMusic();
					readFile(fpath);
					g_TopIndex = g_CursorIndex = 0;
					g_SelectStartCursorIndex = g_SelectEndCursorIndex = -1;
					g_ThPlaingIndex.store(0, std::memory_order_release);
					REDRAW(hWnd);
				}
			}
			break;
		}
		case IDM_SAVE_AS:
		{
			// ファイルの書き込み
			TCHAR fpath[MAX_PATH + 1] = { '\0' };
			OPENFILENAME ofn;
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hWnd;
			ofn.lpstrInitialDir = _T("");
			ofn.lpstrFile = fpath;
			ofn.nMaxFile = sizeof(fpath);
			ofn.lpstrFilter = _T("mdatファイル(*.mdat)\0*.mdat\0tgfdatファイル(*.tgfdat)\0*.tgfdat\0");
			ofn.lpstrDefExt = _T("mdat");
			ofn.lpstrTitle = _T("TGFファイルを指定");
			ofn.nFilterIndex = 1;
			if (GetSaveFileName(&ofn) != 0) {
				auto p = NNEW std::vector<uint8_t>();
				convertToTgf(g_Tgf, p);
				tstring savePath(fpath);
				t_WriteFile(savePath, *p);
			}
			break;
		}
		case IDM_EXIT:
		{
			SendMessage( hWnd, WM_CLOSE, 0, 0 );
			break;
		}
		default:
		{
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	return 0;
}

static void getWindowSize(const HWND hWnd, int *pW, int *pH)
{
	// サイズ
	RECT wcrect;
	GetClientRect(hWnd, &wcrect);
	*pW = wcrect.right - wcrect.left;
	*pH = wcrect.bottom - wcrect.top;
	return;
} 

// Tgfpの情報描画
static void paintTgfpInfo(CD2d &d, const int w, const int h, const OPTS &opts)
{
	tostringstream oss;
	oss << _T("tgfp ");
	if( opts.TgfpIpAddr.empty() ){
		oss << _T("(no use)");
	}
	else{
		oss << opts.TgfpIpAddr << _T(" : ") << opts.TgfpPort;
	}
	d.Text(10, 4, CD2d::FONT::ARIAL_12, oss.str().c_str(), RGBREF(0x808080));
	return;
}

// ファイル名の描画
static void paintFileName(CD2d &d, const int w, const int h, const TGFDATA &tgf)
{
	if (tgf.pRawData == nullptr)
		return;
	tostringstream oss;
	oss << tgf.FileName << _T(" (") << static_cast<int>(tgf.pRawData->size()) << _T(" bytes)");
	SIZE textSize;;
	d.MeasureText(CD2d::FONT::ARIAL_12, oss.str().c_str(), &textSize);
	d.Rect(10, 22, textSize.cx + 6.f, textSize.cy + 6.f, 1.0f, 2.0f, RGBREF(0x4040FF));
	d.Text(12, 24, CD2d::FONT::ARIAL_12, oss.str().c_str(), RGBREF(0x4040FF));
	return;
}

// 表示位置情報の描画
static void paintCursorInfo(
	CD2d &d, const int w, const int h, const int cursor, const int numTracks)
{
	tostringstream oss;
	oss << cursor << _T(" / ") << ((0<numTracks)?(numTracks-1):0);
	d.Text(10.f, 44, CD2d::FONT::ARIAL_12, oss.str().c_str(), RGBREF(0x808080));
	return;
}

// ビジュアライザーの描画
static void paintVisualizer(
	CD2d &d, const int w, const int h,
	const TGFDATA &tgf, const int numTracks, const int plaingIndex)
{
	const float tlWidth = w-TL_MargineL-TL_MargineR;	// Windowsサイズに従う

	// TL欄の外周枠線	
 	d.Line(TL_X+TL_MargineL, TL_Y, TL_X+TL_MargineL+tlWidth, TL_Y, 1.f, RGBREF(0x808080), 1.0f);
 	d.Line(TL_X+TL_MargineL, TL_Y+TL_Height, TL_X+TL_MargineL+tlWidth, TL_Y+TL_Height, 1.f, RGBREF(0x808080), 1.0f);
	// 左端の外周枠線
	if( g_TopIndex == 0 )
 		d.Line(TL_X+TL_MargineL, TL_Y, TL_X+TL_MargineL, TL_Y+TL_Height, 1.f, RGBREF(0x808080), 1.0f);

	// センターライン
 	d.Line(TL_X+TL_MargineL, TL_Y+TL_Height/2, TL_X+TL_MargineL+tlWidth, TL_Y+TL_Height/2, 1.f, RGBREF(0x808080), 1.0f);

	// 選択範囲
	int st = std::min(g_SelectStartCursorIndex, g_SelectEndCursorIndex);
	int ed = std::max(g_SelectStartCursorIndex, g_SelectEndCursorIndex);

	// 波形（音量）
	for( int t = 0; t < tlWidth; ++t ){
		const int index = g_TopIndex + t;
		if(numTracks <= index )
			break;
		const TgfRecord &dt = *(g_Tgf.tracks[index]);
		const float x = TL_X + TL_MargineL + t;
		RGBREF opllCol = (dt.volume_opll.mod) ? RGBREF(0xff8080) : RGBREF(0x806060);
		RGBREF psgCol = (dt.volume_psg.mod) ? RGBREF(0x80ff80) : RGBREF(0x608060);
		RGBREF sccCol = (dt.volume_scc.mod) ? RGBREF(0x8080ff) : RGBREF(0x606080);
		RGBREF sccpCol = (dt.volume_sccplus.mod) ? RGBREF(0x8080ff) : RGBREF(0x606080);

		// OPLL
		const float opllSY = TL_Y+TL_Height*(1.f- dt.volume_opll.vf)/2;
		const float opllDY = opllSY+TL_Height* dt.volume_opll.vf;
		d.Line(x, opllSY, x, opllDY, 1.f, opllCol, 0.7f);
		// PSG
		const float psgSY = TL_Y+TL_Height*(1.f- dt.volume_psg.vf)/2;
		const float psgDY = psgSY+TL_Height* dt.volume_psg.vf;
		d.Line(x, psgSY, x, psgDY, 1.f, psgCol, 0.7f);
		// SCC
		const float sccSY = TL_Y+TL_Height*(1.f- dt.volume_scc.vf)/2;
		const float sccDY = sccSY+TL_Height* dt.volume_scc.vf;
		d.Line(x, sccSY, x, sccDY, 1.f, sccCol, 0.7f);
		// SCC+
		const float sccpSY = TL_Y+TL_Height*(1.f- dt.volume_sccplus.vf)/2;
		const float sccpDY = sccpSY+TL_Height* dt.volume_sccplus.vf;
		d.Line(x, sccpSY, x, sccpDY, 1.f, sccpCol, 0.7f);
		// // SCC+ flag
		// if (dt.bSccPlusAccess) {
		// 	const float flagSY = TL_Y+TL_Height/2;
		// 	d.Line(x, flagSY - 2.f, x, flagSY + 4.f, 1.f, RGBREF(0xffffff), 0.7f);
		// }
		// // SCC flag
		// if (dt.bSccAccess) {
		// 	const float flagSY = TL_Y+TL_Height/2;
		// 	d.Line(x, flagSY - 2.f, x, flagSY + 4.f, 1.f, RGBREF(0xffffA0), 0.7f);
		// }

		// Delete
		if(dt.bDeleteMaker ){
			d.Line(x, TL_Y, x, TL_Y+TL_Height, 1.f, RGBREF(0x404040), 0.7f);
		}	
		if( st <= index && index <= ed && 0 <= st && 0 <= ed){
			d.Line(x, TL_Y, x, TL_Y+TL_Height, 1.f, RGBREF(0x202080), 0.5f);
		}	
	}

	// スクロールバー
	float scX, scY, scW, scH;
	getPosScrollKnob(&scX, &scY, &scW, &scH, tlWidth, g_TopIndex, numTracks);
	d.RectPaint(scX, scY, scW, scH, 1.f, RGBREF(0x808080), 0.7f);

	// カーソル
 	float curPosX = static_cast<float>(g_CursorIndex - g_TopIndex);
 	float curX = curPosX + TL_MargineL;
 	float curSY = TL_Y;
 	float curDY = TL_Y + TL_Height;
 	if( 0 <= curPosX && curPosX <= tlWidth )
 		d.Line(curX, curSY, curX, curDY, 1.f, RGBREF(0x000000), 1.0f);

	// 再生中の位置
 	float playPosX = static_cast<float>(plaingIndex - g_TopIndex);
 	float playX = playPosX + TL_MargineL;
 	float playSY = TL_Y;
 	float playDY = TL_Y + TL_Height;
	tostringstream oss;
	oss << _T("♪ ") << plaingIndex;
 	if( 0 <= playPosX && playPosX <= tlWidth ){
 		d.Line(playX, playSY, playX, playDY, 1.f, RGBREF(0xff2020), 1.0f);
		d.Text(playX, playSY-20, CD2d::FONT::ARIAL_16, oss.str().c_str(), RGBREF(0xff2020));
	}
	return;
}

// レジスター値の描画
static void paintDumRegister(
	CD2d &d, const int w, const int h, const TGFDATA &tgf,
	const int tergetIndex, const bool bPlayMusic)
{
	SIZE textSize;
	const TgfRecord &curDt = *(tgf.tracks[tergetIndex]);
	const float curIndexY = 250.f;
	tostringstream oss;
	oss << ((bPlayMusic)?_T("♪ "):_T("⇩ ")) << tergetIndex;
	d.MeasureText(CD2d::FONT::ARIAL_16, oss.str().c_str(), &textSize);
	d.Rect(10.f-3.f, curIndexY-3.f, textSize.cx + 10.f, textSize.cy + 6.f, 1.0f, 3.0f, RGBREF(0x808080));
	d.Text(10.f, curIndexY, CD2d::FONT::ARIAL_16, oss.str().c_str(), RGBREF(0x808080));

	const float dumpY = curIndexY+30;
	const float dumpLH = 18;
	d.Text(10.f, dumpY+ dumpLH*0, CD2d::FONT::ARIAL_12, _T("OPLL"), RGBREF(0xFF8080));
	d.Text(10.f, dumpY+ dumpLH*1, CD2d::FONT::ARIAL_12, _T("PSG "), RGBREF(0x80A080));
	d.Text(10.f, dumpY+ dumpLH*2, CD2d::FONT::ARIAL_12, _T("SCC "), RGBREF(0x8080FF));
	d.Text(10.f, dumpY+ dumpLH*3, CD2d::FONT::ARIAL_12, _T("SCC+"), RGBREF(0x8080FF));

	for( int t = 0; t < NUM_REG_OPLL; ++t ){
		RGBREF col =
			(curDt.regs_mod.opll[t])
			? RGBREF(0xFF8080)
			: ((curDt.regs.opll[t]==0)?RGBREF(0xC0C0C0):RGBREF(0x808080));
		TCHAR temp[2 + 1];
		::_stprintf_s(temp, sizeof(temp)/sizeof(TCHAR), _T("%02X"), curDt.regs.opll[t]);
		d.Text( (float)(18*t+50), dumpY+ dumpLH*0, CD2d::FONT::ARIAL_12, temp, col);
	}
	for( int t = 0; t < NUM_REG_PSG; ++t ){
		RGBREF col =
			(curDt.regs_mod.psg[t])
			? RGBREF(0xFF8080)
			: ((curDt.regs.psg[t]==0)?RGBREF(0xC0C0C0):RGBREF(0x808080));
		TCHAR temp[2+1];
		::_stprintf_s(temp, sizeof(temp) / sizeof(TCHAR), _T("%02X"), curDt.regs.psg[t]);
		d.Text( (float)(18*t+50), dumpY+ dumpLH*1, CD2d::FONT::ARIAL_12, temp, col);
	}
	// SCC
	for (int t = 0; t < (10+5+1); ++t) {
		const int offset = 0x80;
		const bool bMod = curDt.regs_mod.scc[t+offset];
		const uint8_t vol = curDt.regs.scc[t+offset];
		RGBREF col = (bMod) ? RGBREF(0xFF8080) : ((vol==0)?RGBREF(0xC0C0C0):RGBREF(0x808080));
		TCHAR temp[2 + 1];
		::_stprintf_s(temp, sizeof(temp) / sizeof(TCHAR), _T("%02X"), vol);
		d.Text((float)(18 * t + 50), dumpY+ dumpLH*2, CD2d::FONT::ARIAL_12, temp, col);
	}
	// SCC+
	for (int t = 0; t < (10+5+1); ++t) {
		const int offset = 0xA0;
		const bool bMod = curDt.regs_mod.sccplus[t+offset];
		const uint8_t vol = curDt.regs.sccplus[t+offset];
		RGBREF col = (bMod) ? RGBREF(0xFF8080) : ((vol==0)?RGBREF(0xC0C0C0):RGBREF(0x808080));
		TCHAR temp[2 + 1];
		::_stprintf_s(temp, sizeof(temp) / sizeof(TCHAR), _T("%02X"), vol);
		d.Text((float)(18 * t + 50), dumpY+ dumpLH*3, CD2d::FONT::ARIAL_12, temp, col);
	}
	return;
}

static LRESULT handler_WM_PAINT(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// GDI
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hWnd, &ps);
	EndPaint(hWnd, &ps);

	// サイズ
	int w, h;
	getWindowSize(hWnd, &w, &h);

	// Direct2D
	g_pD2d->BeginDraw();
	g_pD2d->Clear(RGBREF(0xFFFFFF));	// 背景色

	const int numTracks = static_cast<int>(g_Tgf.tracks.size());
	const int plaingIndex = g_ThPlaingIndex.load(std::memory_order_acquire);

	paintTgfpInfo(*g_pD2d, w, h, g_Opts);

	paintFileName(*g_pD2d, w, h, g_Tgf);

	paintCursorInfo(*g_pD2d, w, h, g_TopIndex, numTracks);

	paintVisualizer(*g_pD2d, w, h, g_Tgf, numTracks, plaingIndex);

	if( 0 < numTracks ){
		const int tergetIndex = (g_bPlaingMusic) ? plaingIndex : g_CursorIndex;
		paintDumRegister(*g_pD2d, w, h, g_Tgf, tergetIndex, g_bPlaingMusic);
	}

	g_GemBox.Draw(*g_pD2d);

	g_pD2d->EndDraw();
	return 0;
}

static LRESULT handler_WM_DESTROY(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (g_pD2d != nullptr)
		g_pD2d->Release();
	PostQuitMessage(0);
	return 0;
}

static LRESULT handler_WM_SIZE(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (g_pD2d != nullptr){
		RECT wcrect;
		GetClientRect(hWnd, &wcrect);
		g_pD2d->SetViewSize(wcrect.right - wcrect.left, wcrect.bottom - wcrect.top);
		REDRAW(hWnd);
	}
	return 0;
}

static LRESULT handler_WM_LBUTTONDOWN(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	const int mx = static_cast<int>(LOWORD(lParam));
	const int my = static_cast<int>(HIWORD(lParam));

	int w, h;
	getWindowSize(hWnd, &w, &h);
	const int tlWidth = static_cast<int>(w - TL_MargineL - TL_MargineR);
	const int maxTrack = static_cast<int>(g_Tgf.tracks.size());

	// ビジュアライザー
	if (TL_MargineL <= mx && mx <= TL_MargineL + tlWidth && TL_Y <= my && my <= TL_Y + TL_Height) {

		const int vmx = mx - (int)TL_X - (int)TL_MargineL;
		const int vmy = my - (int)TL_Y;

		int index = g_TopIndex + vmx;
		if (index < maxTrack) {
			stepTopIndex(&index, 0, 0, maxTrack);
			const int oldCursor = g_CursorIndex;
			const int oldStartCursor = g_SelectStartCursorIndex;
			const int oldEndCursor = g_SelectEndCursorIndex;
			if (GetAsyncKeyState(VK_SHIFT)) {
				if (0 <= g_SelectStartCursorIndex) {
					g_SelectEndCursorIndex = index;
					g_CursorIndex = index;
					g_MouseDraggingStatus = DRAG_STATUS::VISUALIZER;
				}
			}
			else {
				g_CursorIndex = index;
				g_SelectStartCursorIndex = g_CursorIndex;
				g_SelectEndCursorIndex = -1;
				g_MouseDraggingStatus = DRAG_STATUS::VISUALIZER;
			}

			if (oldCursor != g_CursorIndex ||
				oldStartCursor != g_SelectStartCursorIndex ||
				oldEndCursor != g_SelectEndCursorIndex) {
				SetCapture(hWnd);
				REDRAW(hWnd);
			}

		}
	}
	// スクロールバー
	float scX, scY, scW, scH;
	getPosScrollKnob(&scX, &scY, &scW, &scH, (float)tlWidth, g_TopIndex, maxTrack);
	if( (int)scX <= mx && mx <= (int)(scX+scW) && (int)scY <= my && my <= (int)(scY+scH) ){
		g_BeginScrollIndex = g_TopIndex;
		g_BeginScrollMX = mx;
		g_MouseDraggingStatus = DRAG_STATUS::SCROLL_KNOB;
		SetCapture(hWnd);
	}

	return 0;
}

static LRESULT handler_WM_LBUTTONUP(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	g_MouseDraggingStatus = DRAG_STATUS::NONE;
	ReleaseCapture();
	return 0;
}

static LRESULT handler_WM_MOUSEMOVE(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	const int mx = static_cast<int>(LOWORD(lParam));
	const int my = static_cast<int>(HIWORD(lParam));
	const int oldCursor = g_CursorIndex;
	const int oldEndCursor = g_SelectEndCursorIndex;
	const int oldTopIndex = g_TopIndex;
	int w, h;
	getWindowSize(hWnd, &w, &h);
	const int tlWidth = static_cast<int>(w - TL_MargineL - TL_MargineR);
	const int maxTrack = static_cast<int>(g_Tgf.tracks.size());

	switch(g_MouseDraggingStatus)
	{
		case DRAG_STATUS::VISUALIZER:
		{
			const int vmx = mx - (int)TL_X - (int)TL_MargineL;
			const int vmy = my - (int)TL_Y;
			if( 0 <= vmx && vmx <= tlWidth && 0 <= vmy && vmy <= TL_Height ){
				int index = g_TopIndex + vmx;
				stepTopIndex(&index, 0, 0, maxTrack -1);
				if( GetAsyncKeyState(VK_SHIFT) ){
					g_SelectEndCursorIndex = index;
					g_CursorIndex = index;
				}
				else{
					g_CursorIndex = index;
					g_SelectStartCursorIndex = index;
					g_SelectEndCursorIndex = -1;
				}
			}
			break;
		}
		case DRAG_STATUS::SCROLL_KNOB:
		{
			if( 0 <= mx && mx < w ){
				if (maxTrack < tlWidth) {
					g_TopIndex = 0;
				}
				else {
					const float rwf = (float)maxTrack / tlWidth;
					g_TopIndex = std::max(0, static_cast<int>(g_BeginScrollIndex + (mx - g_BeginScrollMX) * rwf));
					int maxv = std::max(0, maxTrack - tlWidth);
					if (0 < maxv) {
						stepTopIndex(&g_TopIndex, 0, 0, maxv - 1);
					}
				}
			}
			break;
		}
		default:
			break;
	}

	if( oldCursor != g_CursorIndex ||
		oldEndCursor != g_SelectEndCursorIndex ||
		oldTopIndex != g_TopIndex )
	{
		REDRAW(hWnd);
	}
	return 0;
}

static LRESULT handler_WM_KEYDOWN(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int w, h;
	getWindowSize(hWnd, &w, &h);
	const int tlWidth = static_cast<int>(w - TL_MargineL - TL_MargineR);
	const int maxTrack = static_cast<int>(g_Tgf.tracks.size());
	int maxv = maxTrack - tlWidth;
	if( maxv < 0 )
		maxv = 0;

	const int oldIndex = g_TopIndex;
	const int oldCursor = g_CursorIndex;
	const int oldStartIndex = g_SelectStartCursorIndex;
	const int oldEndIndex = g_SelectEndCursorIndex;
	switch(wParam)
	{
		case VK_F1:	// PLAY MUSIC
		{
			startMusic();
			break;
		}
		case VK_F2:	// STOP MUSIC
		{
			stopMusic();
			REDRAW(hWnd);
			break;
		}
		case VK_F3:
		{
			gotoTop();
			REDRAW(hWnd);
			break;
		}
		case VK_ESCAPE:
		{
			if( 0 < g_SelectStartCursorIndex && 0 < g_SelectEndCursorIndex ){
				g_SelectStartCursorIndex = g_SelectEndCursorIndex = -1;
				REDRAW(hWnd);
			}
			break;
		}
		case VK_PRIOR:	// Page Up
		{
			stepTopIndex(&g_TopIndex, -(h/2), 0, maxv);
			break;
		}
		case VK_NEXT:	// Page Down
		{
			stepTopIndex(&g_TopIndex, +(h/2), 0, maxv);
			break;
		}
		case VK_LEFT:
		{
			int step = GetAsyncKeyState(VK_SHIFT)==0 ? -1 : -30;
			stepTopIndex(&g_CursorIndex, step, 0, maxTrack-1);
			g_SelectStartCursorIndex = g_CursorIndex;
			g_SelectEndCursorIndex = -1;
			break;
		}
		case VK_RIGHT:
		{
			int step = GetAsyncKeyState(VK_SHIFT)==0 ? +1 : +30;
			stepTopIndex(&g_CursorIndex, step, 0, maxTrack-1);
			g_SelectStartCursorIndex = g_CursorIndex;
			g_SelectEndCursorIndex = -1;
			break;
		}
		case VK_DELETE:
		{
			if( g_SelectStartCursorIndex < 0 || g_SelectEndCursorIndex < 0 )
				break;
			int st = std::min(g_SelectStartCursorIndex, g_SelectEndCursorIndex);
			int ed = std::max(g_SelectStartCursorIndex, g_SelectEndCursorIndex);
			bool bDel = false;
			for( int t = st; t < ed; ++t){
				if( g_Tgf.tracks[t]->bDeleteMaker ){
					bDel = true;
					break;
				}
			}
			bDel = !bDel;
			for( int t = st; t < ed; ++t){
				g_Tgf.tracks[t]->bDeleteMaker = bDel;
			}
			g_SelectStartCursorIndex = g_SelectEndCursorIndex = -1;
			REDRAW(hWnd);
			break;
		}
		default:
			break;
	}
	if( oldIndex != g_TopIndex || oldCursor != g_CursorIndex || 
		oldStartIndex != g_SelectStartCursorIndex || oldEndIndex != g_SelectEndCursorIndex )
	{
		REDRAW(hWnd);
	}
	return 0;
}

static LRESULT handler_WM_MOUSEWHEEL(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	int w, h;
	getWindowSize(hWnd, &w, &h);

	const int tlWidth = static_cast<int>(w - TL_MargineL - TL_MargineR);
	const int maxTrack = static_cast<int>(g_Tgf.tracks.size());
	int maxv = maxTrack - tlWidth;
	if( maxv < 0 )
		maxv = 0;
	const int oldIndex = g_TopIndex;
	const int step = GET_WHEEL_DELTA_WPARAM(wParam) / 3;
	stepTopIndex(&g_TopIndex, -step, 0, maxv);
	if( oldIndex != g_TopIndex )
		REDRAW(hWnd);

	return 0;
}

static LRESULT handler_WM_SETFOCUS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	g_bFocus = true;
	REDRAW(hWnd);
	return 0;
}

static LRESULT handler_WM_KILLFOCUS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	g_bFocus = false;
	REDRAW(hWnd);
	return 0;
}

static LRESULT handler_WMA_GEMBUTTON(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	const int id = static_cast<int>(wParam);
	const bool bPush = static_cast<int>(lParam);
	LRESULT result = 1;
	if (!bPush)
		return result;
	switch(id)
	{
		case ID_BTN_PLAY:
			startMusic();
			result = 0;
			break;
		case ID_BTN_STOP:
			stopMusic();
			REDRAW(hWnd);
			result = 0;
			break;
		case ID_BTN_GOTO_TOP:
			gotoTop();
			REDRAW(hWnd);
			result = 0;
			break;
		default:
			break;
	}
	return result;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 1;

	bool bReDraw = false;
    switch (message)
    {
		case WM_LBUTTONDOWN:
		{
			if( g_GemBox.WndMessageHandler_LBUTTONDOWN(hWnd, wParam, lParam, &bReDraw) ) {
				SetCapture(hWnd);
				result = 0;
			}
			break;
		}
		case WM_LBUTTONUP:
		{
			if( g_GemBox.WndMessageHandler_LBUTTONUP(hWnd, wParam, lParam, &bReDraw) ){
				ReleaseCapture();
				result = 0;
			}
			break;
		}
	}

	if( result != 0 ){
		switch (message)
		{
			case WM_CREATE:			result = handler_WM_CREATE(hWnd, message, wParam, lParam);		break;
			case WM_CLOSE:			result = handler_WM_CLOSE(hWnd, message, wParam, lParam);		break;
			case WM_COMMAND:		result = handler_WM_COMMAND(hWnd, message, wParam, lParam);		break;
			case WM_PAINT:			result = handler_WM_PAINT(hWnd, message, wParam, lParam);		break;
			case WM_DESTROY:		result = handler_WM_DESTROY(hWnd, message, wParam, lParam);		break;
			case WM_SIZE:			result = handler_WM_SIZE(hWnd, message, wParam, lParam);		break;
			case WM_LBUTTONDOWN:	result = handler_WM_LBUTTONDOWN(hWnd, message, wParam, lParam);	break;
			case WM_LBUTTONUP:		result = handler_WM_LBUTTONUP(hWnd, message, wParam, lParam);	break;
			case WM_MOUSEMOVE:		result = handler_WM_MOUSEMOVE(hWnd, message, wParam, lParam);	break;
			case WM_KEYDOWN:		result = handler_WM_KEYDOWN(hWnd, message, wParam, lParam);		break;
			case WM_MOUSEWHEEL:		result = handler_WM_MOUSEWHEEL(hWnd, message, wParam, lParam);	break;
			case WM_SETFOCUS:		result = handler_WM_SETFOCUS(hWnd, message, wParam, lParam);	break;
			case WM_KILLFOCUS:		result = handler_WM_KILLFOCUS(hWnd, message, wParam, lParam);	break;
			case WMA_GEMBUTTON: 	result = handler_WMA_GEMBUTTON(hWnd, message, wParam, lParam);	break;
			case WMA_REDRAW:		result = 0; REDRAW(hWnd);										break;
			default:				result = DefWindowProc(hWnd, message, wParam, lParam);			break;
		}
	}

	if( bReDraw ){
		REDRAW(hWnd);
	}
    return result;
}

// バージョン情報ボックスのメッセージ ハンドラーです。
INT_PTR CALLBACK DlgHdrAbout(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
	static HFONT hFont = NULL;
	INT_PTR retc = FALSE;
    switch (message)
    {
		case WM_INITDIALOG:
		{
			tstring tergetPath, nameStr, verNumberStr, copyrightStr;
			t_GetModuleFileName(&tergetPath);
			t_GetFileVersionInfo(tergetPath, &nameStr, &verNumberStr, &copyrightStr);

			HWND hWndStatic2 = GetDlgItem(hDlg, IDC_STATIC2);
			hFont = CreateFont(
				24,0,0,0,0,0,0,0,
				DEFAULT_CHARSET/*SHIFTJIS_CHARSET*/,OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FF_DONTCARE,
				_T("Arial Black"));
			SendMessage( hWndStatic2, WM_SETFONT, (WPARAM)hFont, 0);
			tostringstream oss;
			oss << nameStr << _T("\n") << verNumberStr;
			SetDlgItemText(hDlg, IDC_STATIC2, oss.str().c_str());
			SetDlgItemText(hDlg, IDC_STATIC3, copyrightStr.c_str());
			retc = TRUE;
			break;
		}

	    case WM_COMMAND:
		{
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
				EndDialog(hDlg, LOWORD(wParam));
				DeleteObject(hFont);
				retc = TRUE;
			}
			break;
		}
    }
	return retc;
}


static const std::vector<tstring> *getArgvs(const tstring &str)
{
	std::vector<tstring> *pList = NNEW std::vector<tstring>();

	const TCHAR sep1[] = { _T(" ;\n\r") };
	TCHAR *pToken = const_cast<TCHAR*>(str.c_str());
	TCHAR *pTokenLine, *pNextTolenLine = nullptr;
	pTokenLine = _tcstok_s(pToken, sep1, &pNextTolenLine);

	while (pTokenLine != nullptr)
	{
		pList->push_back(pTokenLine);
		pTokenLine = _tcstok_s(nullptr, sep1, &pNextTolenLine);
	}
	return pList;
}

static bool options(const std::vector<tstring> &strs, OPTS *pOpts)
{
	const int num = static_cast<int>(strs.size());
	bool bError = false;
	tstring errStr;
	for( int t = 0; t < num; ++t){
		const auto s = strs[t];
		if( s == _T("-f") ){
			if( (t+1) < num ){
				pOpts->DefaultFilePath = strs[t+1];
				++t;
			}
			else{
				bError = true;
				errStr = _T("No File-path specified in -c option");
				break;
			}
		}
		else if( s == _T("-ip") ){
			if( (t+1) < num ){
				pOpts->TgfpIpAddr = strs[t+1];
				++t;
			}
			else{
				bError = true;
				errStr = _T("No IP Address specified in -ip option");
				break;
			}
		}
		else if( s == _T("-port") ){
			if( (t+1) < num ){
				pOpts->TgfpPort = ::_tstoi(strs[t+1].c_str());
				++t;
			}
			else{
				bError = true;
				errStr = _T("No PortNo specified in -port option");
				break;
			}
		}
		else{
			errStr = _T("found unknown option : ") + s;
			bError = true;
		}
	}
	if( bError ){
		MessageBox(nullptr, errStr.c_str(), _T("TgfEditor error"), MB_OK);
	}
	return bError;
}

/**
 * エントリ
*/ 
int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

	timeBeginPeriod(1);

    // グローバル文字列を初期化する
    LoadStringW(hInstance, IDS_APP_TITLE, g_szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_TGFEDITOR, g_szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

	// コマンドラインオプションの解析
	std::unique_ptr<const std::vector<tstring>> pArgs(getArgvs(lpCmdLine));
	if( options(*pArgs, &g_Opts) )
        return FALSE;

    // アプリケーション初期化の実行:
    if (!InitInstance(hInstance, nCmdShow, *pArgs))
        return FALSE;

	// メイン メッセージ ループ
    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TGFEDITOR));
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

	timeEndPeriod(1);

    return (int)msg.wParam;
}