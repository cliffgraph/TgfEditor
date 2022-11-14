#pragma once
#include "pch.h"
#include <vector>

class CBaseGem;

class CGemBox
{
private:
	std::vector<CBaseGem*> m_List;
	CBaseGem *m_pMouseCatchedGem;

public:
	CGemBox();
	virtual ~CGemBox();

public:
	int GetNum() const;
	void Set(CBaseGem *pGem);

public:
	bool WndMessageHandler_LBUTTONDOWN(
		HWND hWnd, WPARAM wParam, LPARAM lParam, bool *pbRefreshDraw);
	bool WndMessageHandler_LBUTTONUP(
		HWND hWnd, WPARAM wParam, LPARAM lParam, bool *pbRefreshDraw);
	void Draw(CD2d &d);
};
