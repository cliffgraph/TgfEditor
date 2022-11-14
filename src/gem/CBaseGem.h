#include "pch.h"

class CD2d;

class CBaseGem
{
public:
	struct Location
	{
		int x, y;
		int w, h;
		Location() : x(0), y(0), w(0), h(0){}
	};

protected:
	Location m_Location;

public:
	CBaseGem()
	{
		return;
	}

	virtual ~CBaseGem()
	{
		return;
	}

public:
	virtual void SetLocate(const int sx, const int sy, const int w, const int h)
	{
		m_Location.x = sx, m_Location.y = sy;
		m_Location.w = w, m_Location.h = h;
		return;
	}

	const Location &GetLocate()
	{
		return m_Location;
	}

public:
	virtual bool LeftButtonDown(const int mx, const int my, bool *pbRefreshDraw)
	{
		return false;
	}

	virtual bool LeftButtonUp(const int mx, const int my, bool *pbRefreshDraw)
	{
		return false;
	}

	virtual void Draw(CD2d &d)
	{
		return;
	}
};



