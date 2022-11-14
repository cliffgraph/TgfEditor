#pragma once

#include "pch.h"
#include <atlbase.h>
#include <vector>

interface ID2D1Bitmap;
interface ID2D1Factory;
interface ID2D1HwndRenderTarget;
interface ID2D1PathGeometry;
interface ID2D1RenderTarget;
interface ID2D1Brush;
interface ID2D1SolidColorBrush;
interface ID2D1LinearGradientBrush;
interface ID2D1StrokeStyle;
interface IDWriteFactory;
interface IDWriteTextFormat;
interface IWICImagingFactory;
interface ID2D1LinearGradientBrush;
class D2DColorBrushItem;


typedef DWORD RGBREF;	//!< R8G8B8型

namespace D2DRENDER
{
	enum class TEXT_ALIMENT_V
	{
		TOP = 0,
		CENTER = 1,
		BOTTOM = 2,
	};
	enum class TEXT_ALIMENT_H
	{
		LEFT = 0,
		CENTER = 1,
		RIGHT = 2,
	};
	//! グラデーションの種類
	enum class GRADMODE
	{
		Horizontal = 0,			//!< 水平方向
		Vertical,				//!< 垂直方向
		ForwardDiagonal,		//!< 左上から右下の方向
		BackwardDiagonal,		//!< 右上から左下の方向
	};

};

class CD2d
{
private:
	static int									m_Instances;	// CD2dRenderのインスタンス化カウンタ
	static std::vector<IDWriteTextFormat*>		m_Fonts;		// フォント格納管理
	static ID2D1Factory							*m_pD2DFactory;
	static IDWriteFactory						*m_pDWriteFactory;
	static IWICImagingFactory					*m_pWICImagingFactory;

private:
	struct PICTFILEINFO { tstring name; BYTE* p; size_t size; PICTFILEINFO() :p(nullptr), size(0) {} };
	struct WHSIZE { int W, H; WHSIZE() :W(0), H(0) {}; };

public:
	enum class FONT
	{
		ARIAL_10 = 0,
		ARIAL_12 = 1,
		ARIAL_16 = 2,
		NUM,
	};

private:
	HWND							m_TargetHWND;
	WHSIZE							m_TargetWind;
	bool							m_bUseCanvasSize;
	WHSIZE							m_Canvas;
	ID2D1HwndRenderTarget			*m_pD2DRenderTarget;
	bool							m_bDrawing;				//!< true=描画中
	HFONT							m_hOldFont;
	std::vector<ID2D1Bitmap*>		*m_pBitmapResource;
	std::vector<PICTFILEINFO>		*m_pBitmapFileImage;
	std::vector<D2DColorBrushItem*>	*m_pBrushStocker;
	float							m_LineScaling;

private:
	void init();
	void createDefaultStockFonts(IDWriteFactory *pFactory);
	void deleteAllFonts();
	void deleteAllStocksBrush();
	void measureText(
		IDWriteTextFormat *pFormat, const TCHAR *pText, SIZE *pSize);
	void measureText(
		IDWriteTextFormat *pFormat, const TCHAR *pText, const UINT len, SIZE *pSize);
	bool createBitmapFromFile(
		ID2D1RenderTarget *pRenderTarget, IWICImagingFactory *pIWICFactory,
		const tstring uri, ID2D1Bitmap **ppBitmap );
	bool createBitmapFromMemory(
		ID2D1RenderTarget *pRenderTarget, IWICImagingFactory *pIWICFactory,
		BYTE *pMemory, const size_t memorySize, ID2D1Bitmap **ppBitmap );
	void releaseResourceOfD2DRenderTarget();
	void restraintResourceOfD2DRenderTarget();

public:
	CD2d();
	virtual ~CD2d();

public:
	ID2D1Factory *GetFactory();
	void Release();
	bool SetRenderTargetWindow(const HWND hWnd, const int width, const int height);
	void SetPictureFileImage(const tstring &name, BYTE *pBin, const size_t size);

	float GetFontHeight(const FONT fc) const;

	bool GetAntiAlias();
	bool GetAntiAliasText();
	void AntiAlias(bool on);
	void AntiAliasText(bool on);
	void SetLineWidthScaling(const float scaling);
	void SetDpi(const float dpiX, const float dpiY);

	void SetViewSize(const int vw, const int vh);
	void GetViewSize(int *pVw, int *pVh) const;

	void BeginDraw();
	bool EndDraw();
	void Flush();

	D2DColorBrushItem *GetStockD2dBrush(
		const DWORD col1, const DWORD col2 = 0,
		const D2DRENDER::GRADMODE grad = D2DRENDER::GRADMODE::Vertical,
		const float sx = 0, const float sy = 0, const float lx = 0, const float ly = 0);

	ID2D1SolidColorBrush *CreateSolidColorBrush(
		const RGBREF col, const float alpha);

	ID2D1LinearGradientBrush *CreateLinearGradientBrush(
		const float sx, const float sy, const float dx, const float dy,
		const D2DRENDER::GRADMODE mode, const RGBREF aColor[], const float aAlpha[]);

	ID2D1LinearGradientBrush *CreateLinearGradientBrush(
		const float sx, const float sy, const float dx, const float dy,
		const D2DRENDER::GRADMODE mode, const RGBREF aColor[], const float aAlpha[],
		const float aPosition[], const int num, BOOL bSRGB = true);
	void ReleaseSolidColorBrush(ID2D1SolidColorBrush **ppBrush);

	void Clear(const RGBREF col = 0x000000, const float alpha = 1.0f);
 	void Line(
		const float sx, const float sy, const float dx, const float dy, const float width,
		const RGBREF col, const float alpha = 1.0f);
 	void Line(
		const float sx, const float sy, const float dx, const float dy, const float width,
		ID2D1Brush *pBrush );
 	void Rect(
		const float sx, const float sy, const float lx, const float ly, const float width,
		const RGBREF col, const float alpha = 1.0f);
	void Rect(
		const float sx, const float sy, const float lx, const float ly, const float width,
		ID2D1Brush *pBrush);
 	void Rect(
		const float sx, const float sy, const float lx, const float ly, const float width,
		const float trim, const RGBREF col, const float alpha = 1.0f);
 	void Rect(
		const float sx, const float sy, const float lx, const float ly, const float width,
		const float trim, ID2D1Brush *pBrush);
 	void Rect(
		const float sx, const float sy, const float lx, const float ly, const float width,
		ID2D1StrokeStyle *pStyle, const RGBREF col, const float alpha = 1.0f);
 	void Rect(
		const float sx, const float sy, const float lx, const float ly, const float width,
		ID2D1StrokeStyle *pStyle, ID2D1Brush *pBrush);
 	void Rect(
		const float sx, const float sy, const float lx, const float ly, const float width,
		ID2D1StrokeStyle *pStyle, const float trim, const RGBREF col, const float alpha = 1.0f);
 	void Rect(
		const float sx, const float sy, const float lx, const float ly, const float width,
		ID2D1StrokeStyle *pStyle, const float trim, ID2D1Brush *pBrush);
 	void RectPaint(
		const float sx, const float sy, const float lx, const float ly,
		const RGBREF col, const float alpha = 1.0f);
 	void RectPaint(
		const float sx, const float sy, const float lx, const float ly,
		ID2D1Brush *pBrush);
 	void RectPaint(
		const float sx, const float sy, const float lx, const float ly,
		const float trim, const RGBREF col, const float alpha = 1.0f);
	void RectPaint(
		const float sx, const float sy, const float lx, const float ly,
		const float trim, ID2D1Brush *pBrush);
 	void RectPaint(
		const float sx, const float sy, const float lx, const float ly,
		const RGBREF col[], const float alpha[], const D2DRENDER::GRADMODE mode);
 	void RectPaint(
		const float sx, const float sy, const float lx, const float ly, const float trim,
		const RGBREF col[], const float alpha[], const D2DRENDER::GRADMODE mode);
	void Ellipse(
		const float sx, const float sy, const float lx, const float ly, const float width,
		const RGBREF col, const float alpha = 1.0f);
	void Ellipse(
		const float sx, const float sy, const float lx, const float ly, const float width,
		ID2D1Brush *pBrush);
	void Ellipse(
		const float sx, const float sy, const float r, const float width, ID2D1Brush *pBrush);
	void Ellipse(
		const float sx, const float sy, const float r, const float width,
		const RGBREF col, const float alpha = 1.0f);
	void EllipsePaint(
		const float sx, const float sy, const float lx, const float ly,
		const RGBREF col, const float alpha = 1.0f);
	void EllipsePaint(
		const float sx, const float sy, const float lx, const float ly,	ID2D1Brush *pBrush);
	void EllipsePaint(
		const float sx, const float sy, const float r, const RGBREF col, const float alpha = 1.0f);
	void EllipsePaint(
		const float sx, const float sy, const float r, ID2D1Brush *pBrush);
	void EllipsePaint(
		const float sx, const float sy, const float r, const RGBREF col[], const float alpha[], const D2DRENDER::GRADMODE mode);
 	void Text(
		const float sx, const float sy, const FONT fc, const TCHAR *pText,
		const RGBREF color, const float alpha = 1.0f);
	void Text(
		const float sx, const float sy, const FONT fc, const TCHAR *pText, ID2D1Brush *pBrush);
	void TextInRect(
		const float sx, const float sy, const float boxWidth, const float boxHeight,
		const FONT fc, const TCHAR *pText, const RGBREF col, const float alpha = 1.0f,
		const D2DRENDER::TEXT_ALIMENT_V tav = D2DRENDER::TEXT_ALIMENT_V::CENTER,
		const D2DRENDER::TEXT_ALIMENT_H tah = D2DRENDER::TEXT_ALIMENT_H::CENTER);
	void TextInRect(
		const float sx, const float sy, const float boxWidth, const float boxHeight,
		const FONT fc, const TCHAR *pText, ID2D1Brush *pBrush,
		const D2DRENDER::TEXT_ALIMENT_V tav = D2DRENDER::TEXT_ALIMENT_V::CENTER,
		const D2DRENDER::TEXT_ALIMENT_H tah = D2DRENDER::TEXT_ALIMENT_H::CENTER);
	void MeasureText(
		const FONT fc, const TCHAR *pText, SIZE *pSize);
	void MeasureTextInRect(
		const float boxWidth, const float boxHeight,
		const FONT fc, const TCHAR *pText,
		const D2DRENDER::TEXT_ALIMENT_V tav, const D2DRENDER::TEXT_ALIMENT_H tah,
		float *pX, float *pY, SIZE *pSize);
	void MeasureCharactor(const FONT fc, const TCHAR ch, SIZE *pSize);
	void GeometryPaint(ID2D1PathGeometry *pGeo, const RGBREF col, const float alpha = 1.0f);
	void GeometryPaint(ID2D1PathGeometry *pGeo, ID2D1Brush *pBrush);
	void Geometry(ID2D1PathGeometry *pGeo, const float width, const RGBREF col, const float alpha = 1.0f);
	void Geometry(ID2D1PathGeometry *pGeo, const float width, ID2D1Brush *pBrush);

	// 画像に関する
	void GetPictureSize(const int pictNo, float *pWidth, float *pHeight);
	ID2D1Bitmap *GetPictureObj(const tstring &name);
	void DrawPicture(const float sx, const float sy, ID2D1Bitmap *pBitmap);
};


