#include "pch.h"
#include "CGemsButton.h"
#include "CD2d.h"

CGemsButton::CGemsButton(const HWND hWnd, const tstring &cap, const int btnId, const bool bEnb) :
	m_hWnd(hWnd), m_Caption(cap), m_Id(btnId), bPushing(false), m_bEnable(bEnb)
{
	return;
}

CGemsButton::~CGemsButton()
{
	return;
}

bool CGemsButton::LeftButtonDown(const int mx, const int my, bool *pbRefreshDraw)
{
	if(!m_bEnable)
		return false;
	bPushing = true;
	*pbRefreshDraw = true;
	PostMessage(m_hWnd, WMA_GEMBUTTON, (WPARAM)(m_Id), (LPARAM)(bPushing));
	return true;
}

bool CGemsButton::LeftButtonUp(const int mx, const int my, bool *pbRefreshDraw)
{
	if(!m_bEnable)
		return false;
	bPushing = false;
	*pbRefreshDraw = true;
	PostMessage(m_hWnd, WMA_GEMBUTTON, (WPARAM)(m_Id), (LPARAM)(bPushing));
	return true;
}

void CGemsButton::Draw(CD2d &d)
{
	const float sx = static_cast<float>(m_Location.x);
	const float sy = static_cast<float>(m_Location.y);
	const float w = static_cast<float>(m_Location.w);
	const float h = static_cast<float>(m_Location.h);
	const float trim = 2.f;
	RGBREF fcol, pcol;
	if(m_bEnable){
		fcol = (bPushing) ? RGBREF(0xd0d0d0) : RGBREF(0x808080);
		pcol = (bPushing) ? RGBREF(0x606060) : RGBREF(0xffffff);
	}
	else{
		fcol = RGBREF(0x808080), pcol = RGBREF(0x707070);
	}
	d.RectPaint(sx, sy, w, h, trim, pcol);
	d.TextInRect(sx, sy, w, h, CD2d::FONT::ARIAL_16, m_Caption.c_str(), fcol);
	d.Rect(sx, sy, w, h, 1.0f, trim, fcol);
	return;
}




