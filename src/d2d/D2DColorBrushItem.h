#pragma once
#include "pch.h"
#include "d2d.h"

class D2DColorBrushItem
{
public:
	DWORD				Color1, Color2;			//!< ARGB
	D2DRENDER::GRADMODE	Gradation;
	float 				X, Y, Width, Height;
	ID2D1Brush			*pBrush;

public:
	D2DColorBrushItem()
	{
		Color1 = Color2 = 0;
		Gradation = D2DRENDER::GRADMODE::Vertical;
		X = Y = Width = Height = 0;
		pBrush = nullptr;
		return;
	}

	D2DColorBrushItem(
		const DWORD col1, const DWORD col2 = 0,
		const D2DRENDER::GRADMODE grad = D2DRENDER::GRADMODE::Vertical,
		const float sx = 0, const float sy = 0, const float lx = 0, const float ly = 0)
	{
		Color1 = col1;
		Color2 = col2;
		Gradation = grad;
		X = sx, Y = sy, Width = lx, Height = ly;
		pBrush = nullptr;
		return;
	}

	~D2DColorBrushItem()
	{
		D2DSafeRelease(&pBrush);
		return;
	}

	// パラメータの比較
	bool CompareTo(const D2DColorBrushItem &rhs) const
	{
		if( Color1 != rhs.Color1 )
			return false;
		if( Color2 != rhs.Color2 )
			return false;
		if( X != rhs.X )
			return false;
		if( Y != rhs.Y )
			return false;
		if( Width != rhs.Width )
			return false;
		if( Height != rhs.Height )
			return false;
		if( Gradation != rhs.Gradation )
			return false;
		return true;
	}
};
