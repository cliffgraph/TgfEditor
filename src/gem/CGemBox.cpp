#include "pch.h"
#include "CBaseGem.h"
#include "CGemBox.h"
#include "CD2d.h"


/** コンストラクタ
*/
CGemBox::CGemBox()
{
	m_pMouseCatchedGem = nullptr;
	return;
}

/** デストラクタ
*/
CGemBox::~CGemBox()
{
	for (auto t : m_List)
		NDELETE(t);
	m_List.clear();
	return;
}

int CGemBox::GetNum() const
{
	return static_cast<int>(m_List.size());
}

void CGemBox::Set(CBaseGem *pGem)
{
	m_List.push_back(pGem);
	return;
};

bool CGemBox::WndMessageHandler_LBUTTONDOWN(
	HWND hWnd, WPARAM wParam, LPARAM lParam, bool *pbRefreshDraw)
{
	const int mx = static_cast<int>(LOWORD(lParam));
	const int my = static_cast<int>(HIWORD(lParam));
	bool bRetc = false;
	for (auto pGem : m_List){
		auto &loc = pGem->GetLocate();
		if (loc.x <= mx && mx <= (loc.x+loc.w-1) && loc.y <= my && my <= (loc.y+loc.h-1)){
			int lx = mx - loc.x, ly = my - loc.y;
			bRetc = pGem->LeftButtonDown(lx, ly, pbRefreshDraw);
			if( bRetc ){
				m_pMouseCatchedGem = pGem;
				break;
			}
		}
	}
	return bRetc;
}

bool CGemBox::WndMessageHandler_LBUTTONUP(
	HWND hWnd, WPARAM wParam, LPARAM lParam, bool *pbRefreshDraw)
{
	if (m_pMouseCatchedGem != nullptr){
		const int mx = static_cast<int>(LOWORD(lParam));
		const int my = static_cast<int>(HIWORD(lParam));
		auto &loc = m_pMouseCatchedGem->GetLocate();
		int lx = mx - loc.x, ly = my - loc.y;
		m_pMouseCatchedGem->LeftButtonUp(lx, ly, pbRefreshDraw);
		m_pMouseCatchedGem = nullptr;
		return true;
	}
	return false;
}

void CGemBox::Draw(CD2d &d)
{
	for (auto pGem : m_List) {
		pGem->Draw(d);
	}
	return;
}



