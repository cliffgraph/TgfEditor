#include "pch.h"
#include "CBaseGem.h"
class CD2d;

class CGemsButton : public CBaseGem
{
private:
	HWND m_hWnd;
	tstring m_Caption;
	int m_Id;
	bool m_bEnable;
private:
	bool bPushing;

public:
	CGemsButton(const HWND hWnd, const tstring &cap, const int btnId, const bool bEnb = true);
	virtual ~CGemsButton();

public:
	bool LeftButtonDown(const int mx, const int my, bool *pbRefreshDraw);
	bool LeftButtonUp(const int mx, const int my, bool *pbRefreshDraw);
	void Draw(CD2d &d);
};



