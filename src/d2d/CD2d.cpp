#include "pch.h"

// for CComPtr
#include <atlbase.h>

// for Direct2D(*1)
#include "d2d.h"
#pragma comment( lib, "d2d1.lib")

// for DirectWrite(*2)
#include <Dwrite.h>
#pragma comment( lib, "Dwrite.lib")
// (*1) Refer to "https://msdn.microsoft.com/ja-jp/library/windows/desktop/dd317121(v=vs.85).aspx"
// (*2) Refer to https://msdn.microsoft.com/ja-jp/library/windows/desktop/dd371554(v=vs.85).aspx

// for WIC
#include <wincodec.h>		// IWICBitmapDecoder, etc...
#pragma comment( lib, "windowscodecs.lib")

#include "CD2d.h"
#include "d2d.h"
#include "D2DColorBrushItem.h"

int									CD2d::m_Instances = 0;
std::vector<IDWriteTextFormat*>		CD2d::m_Fonts;
ID2D1Factory						*CD2d::m_pD2DFactory = nullptr;
IDWriteFactory						*CD2d::m_pDWriteFactory = nullptr;
IWICImagingFactory					*CD2d::m_pWICImagingFactory = nullptr;

struct TEXT_FORMAT_DT
{
	const TCHAR				*pName;				//!< フォント名
	float					fontSize;			
	DWRITE_FONT_WEIGHT		fontWeight;			//!< DWRITE_FONT_WEIGHT_NORMALなど
	DWRITE_FONT_STYLE		fontStyle;			//!< DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STYLE_OBLIQUE, DWRITE_FONT_STYLE_ITALIC 
	DWRITE_FONT_STRETCH		fontStretch;		//!< DWRITE_FONT_STRETCH_NORMAL(5), 0～9
	const TCHAR				*pLocaleName;		//!< L"ja-jp", L"en-us"
} static const g_tableTextFormat[] =
{
//	フォント名,		高さ,	太さ,						スタイル,					ストレッチ,					ロケール,
	_T("Arial"),	10,		DWRITE_FONT_WEIGHT_NORMAL,	DWRITE_FONT_STYLE_NORMAL,	DWRITE_FONT_STRETCH_NORMAL,	L"ja-jp",
	_T("Arial"),	12,		DWRITE_FONT_WEIGHT_NORMAL,	DWRITE_FONT_STYLE_NORMAL,	DWRITE_FONT_STRETCH_NORMAL,	L"ja-jp",
	_T("Arial"),	16,		DWRITE_FONT_WEIGHT_NORMAL,	DWRITE_FONT_STYLE_NORMAL,	DWRITE_FONT_STRETCH_NORMAL,	L"ja-jp",
	nullptr,		0,		DWRITE_FONT_WEIGHT_NORMAL,	DWRITE_FONT_STYLE_NORMAL,	DWRITE_FONT_STRETCH_NORMAL,	nullptr,
};

/**
* (static)グラデーションの方向情報を作成する
*/
static void makeGradiantProp(
	const float sx, const float sy, const float dx, const float dy,
	const D2DRENDER::GRADMODE mode, D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES *pLgbp)
{
	D2D1_POINT_2F sp, ep;
	switch(mode)
	{
		//!< 左上から右下の方向
		case D2DRENDER::GRADMODE::ForwardDiagonal:
			sp.x = sx, sp.y = sy;
			ep.x = dx, ep.y = dy;
			break;
		//!< 右上から左下の方向
		case D2DRENDER::GRADMODE::BackwardDiagonal:
			sp.x = dx, sp.y = sy;
			ep.x = sx, ep.y = dy;
			break;
		//!< 水平方向（左から右）
		case D2DRENDER::GRADMODE::Horizontal:
			sp.x = sx, sp.y = sy+(dy-sy)/2;
			ep.x = dx, ep.y = sp.y;
			break;
		//!< 垂直方向（上から下）
		case D2DRENDER::GRADMODE::Vertical:
		default:
			sp.x = sx+(dx-sx)/2, sp.y = sy;
			ep.x = sp.x,         ep.y = dy;
			break;
	}
	*pLgbp = D2D1::LinearGradientBrushProperties(sp, ep);
	return;
}

/**
* フォントの生成
*/
void CD2d::createDefaultStockFonts(IDWriteFactory *pFactory)
{
	for( int t = 0; g_tableTextFormat[t].pName != nullptr; t++)
	{
		const TEXT_FORMAT_DT &f_data = g_tableTextFormat[t];
		IDWriteTextFormat *pFormat = nullptr;
		pFactory->CreateTextFormat(
			f_data.pName,		nullptr,			f_data.fontWeight,	f_data.fontStyle,
			f_data.fontStretch,	f_data.fontSize,	f_data.pLocaleName,	&pFormat);
		m_Fonts.push_back(pFormat);
	}
	return;
}

/**
* 全てのフォントの破棄
*/
void CD2d::deleteAllFonts()
{
	for( auto it = m_Fonts.begin(); it != m_Fonts.end(); it++)
	{
		IDWriteTextFormat *pFormat = (*it);
		D2DSafeRelease(&pFormat);
	}
	m_Fonts.clear();
	return;
}

/** ブラシの破棄
*/
void CD2d::deleteAllStocksBrush()
{
	for( auto it = m_pBrushStocker->begin(); it != m_pBrushStocker->end(); it++)
	{
		D2DColorBrushItem *pDt = *it;
		D2DSafeRelease( &(pDt->pBrush) );
		NDELETE(pDt);
	}
	NDELETE(m_pBrushStocker);
}

/**
* テキスト文字列の描画サイズを返す
*
* @param [in] pFormat IDWriteTextFormatへのポインタ
* @param [in] pText 文字列へのポインタ
* @param [in] pSize サイズを格納する構造体へのポインタ
* @return なし
*/
void CD2d::measureText(
	IDWriteTextFormat *pFormat, const TCHAR *pText, SIZE *pSize)
{
	assert( m_pDWriteFactory != nullptr );
	assert( pFormat != nullptr );

	IDWriteTextLayout *pLayout = nullptr;
	HRESULT hr = m_pDWriteFactory->CreateTextLayout(
		pText, static_cast<UINT32>(_tcslen(pText)), pFormat, 0, 0, &pLayout);
	if( SUCCEEDED(hr) )
	{
		DWRITE_TEXT_METRICS metrics;
		pLayout->GetMetrics(&metrics);
		pSize->cx = static_cast<LONG>(metrics.widthIncludingTrailingWhitespace);	// .width は空白文字を含めた値にならない
		pSize->cy = static_cast<LONG>(metrics.height);
		D2DSafeRelease(&pLayout);
	}
	return;
}

void CD2d::measureText(
	IDWriteTextFormat *pFormat, const TCHAR *pText, const UINT len, SIZE *pSize)
{
	assert( m_pDWriteFactory != nullptr );
	assert( pFormat != nullptr );

	IDWriteTextLayout *pLayout = nullptr;
	HRESULT hr = m_pDWriteFactory->CreateTextLayout(pText, len, pFormat, 0, 0, &pLayout);
	if( SUCCEEDED(hr) )
	{
		DWRITE_TEXT_METRICS metrics;
		pLayout->GetMetrics(&metrics);
		pSize->cx = static_cast<LONG>(metrics.widthIncludingTrailingWhitespace);	// .width は空白文字を含めた値にならない
		pSize->cy = static_cast<LONG>(metrics.height);
		D2DSafeRelease(&pLayout);
	}
	return;
}

/** Creates a Direct2D bitmap from the specified file name.
 サンプルプログラム：SimpleDirect2dApplication.cpp より。
*/
bool CD2d::createBitmapFromFile(
	ID2D1RenderTarget *pRenderTarget, IWICImagingFactory *pIWICFactory,
	const tstring uri, ID2D1Bitmap **ppBitmap )
{
	HRESULT	hr = S_OK;
	IWICBitmapDecoder		*pDecoder	= nullptr;
	IWICBitmapFrameDecode	*pSource 	= nullptr;
	IWICFormatConverter		*pConverter	= nullptr;
	hr = pIWICFactory->CreateDecoderFromFilename(
		uri.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder );
	if (SUCCEEDED(hr))
	{
		// Create the initial frame.
		hr = pDecoder->GetFrame(0, &pSource);
	}
	if (SUCCEEDED(hr))
	{
		// Convert the image format	to 32bppPBGRA (DXGI_FORMAT_B8G8R8A8_UNORM + D2D1_ALPHA_MODE_PREMULTIPLIED).
		hr = pIWICFactory->CreateFormatConverter(&pConverter);
	}
	if (SUCCEEDED(hr))
	{
		hr = pConverter->Initialize(
			pSource, GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut );
	}
	if (SUCCEEDED(hr))
	{
		// Create a	Direct2D bitmap	from the WIC bitmap.
		hr = pRenderTarget->CreateBitmapFromWicBitmap( pConverter, nullptr, ppBitmap );
	}
	D2DSafeRelease(&pDecoder);
	D2DSafeRelease(&pSource);
	D2DSafeRelease(&pConverter);
	return SUCCEEDED(hr);
}

/** Creates a Direct2D bitmap from file on memory.
 サンプルプログラム：SimpleDirect2dApplication.cpp より。
*/
bool CD2d::createBitmapFromMemory(
	ID2D1RenderTarget *pRenderTarget, IWICImagingFactory *pIWICFactory,
	BYTE *pMemory, const size_t memorySize, ID2D1Bitmap **ppBitmap )
{
	HRESULT	hr = S_OK;
	IWICBitmapDecoder		*pDecoder	= nullptr;
	IWICBitmapFrameDecode	*pSource 	= nullptr;
	IWICFormatConverter		*pConverter	= nullptr;
    IWICStream 				*pStream	= nullptr;

    if (SUCCEEDED(hr))
    {
        // Create a WIC stream to map onto the memory.
        hr = pIWICFactory->CreateStream(&pStream);
    }
    if (SUCCEEDED(hr))
    {
        // Initialize the stream with the memory pointer and size.
        hr = pStream->InitializeFromMemory( reinterpret_cast<BYTE*>(pMemory), static_cast<DWORD>(memorySize) );
    }
    if (SUCCEEDED(hr))
    {
        // Create a decoder for the stream.
        hr = pIWICFactory->CreateDecoderFromStream( pStream, nullptr, WICDecodeMetadataCacheOnLoad, &pDecoder );
    }
	if (SUCCEEDED(hr))
	{
		// Create the initial frame.
		hr = pDecoder->GetFrame(0, &pSource);
	}
	if (SUCCEEDED(hr))
	{
		// Convert the image format	to 32bppPBGRA (DXGI_FORMAT_B8G8R8A8_UNORM + D2D1_ALPHA_MODE_PREMULTIPLIED).
		hr = pIWICFactory->CreateFormatConverter(&pConverter);
	}
	if (SUCCEEDED(hr))
	{
		hr = pConverter->Initialize(pSource, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut );
	}
	if (SUCCEEDED(hr))
	{
		// Create a	Direct2D bitmap	from the WIC bitmap.
		hr = pRenderTarget->CreateBitmapFromWicBitmap( pConverter, nullptr, ppBitmap );
	}
	D2DSafeRelease(&pDecoder);
	D2DSafeRelease(&pSource);
	D2DSafeRelease(&pStream);
	D2DSafeRelease(&pConverter);
	return SUCCEEDED(hr);
}

/**
* RenderTargetとそれによって生成されたリソースを開放する
*/
void CD2d::releaseResourceOfD2DRenderTarget()
{
	D2DSafeRelease(&m_pD2DRenderTarget);

	for( auto it = m_pBitmapResource->begin(); it != m_pBitmapResource->end(); ++it )
		D2DSafeRelease(&(*it));
	m_pBitmapResource->clear();

	for( auto it = m_pBrushStocker->begin(); it != m_pBrushStocker->end(); ++it)
		D2DSafeRelease( &((*it)->pBrush) );
	m_pBrushStocker->clear();

	return;
}

/**
* RenderTargetとそれに関わるリソースを生成する
*/
void CD2d::restraintResourceOfD2DRenderTarget()
{
	if( m_TargetHWND == nullptr || m_pD2DRenderTarget != nullptr)
		return;
	const int w = m_TargetWind.W;
	const int h = m_TargetWind.H;

	HRESULT hr = m_pD2DFactory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties( m_TargetHWND, D2D1::SizeU(w, h)), &m_pD2DRenderTarget );

	for( auto it = m_pBitmapFileImage->begin(); it != m_pBitmapFileImage->end(); it++ )
	{
		PICTFILEINFO &info = (*it);
		ID2D1Bitmap *pBitmap;
		createBitmapFromMemory( m_pD2DRenderTarget, m_pWICImagingFactory, info.p, info.size, &pBitmap);
		m_pBitmapResource->push_back(pBitmap);
	}

	for( auto it = m_pBrushStocker->begin(); it != m_pBrushStocker->end(); it++)
	{
		D2DColorBrushItem &dt = *(*it);
		D2DSafeRelease( &dt.pBrush );
		if( dt.Color2 == 0)
			dt.pBrush = CreateSolidColorBrush( dt.Color1 & 0xFFFFFF,  ((((dt.Color1)>>24)&0xFF)/255.0f) );
		else
		{
			const RGBREF aColor[] = {dt.Color1&0xFFFFFF, dt.Color2&0xFFFFFF };
			const float aAlpha[] = {((((dt.Color1)>>24)&0xFF)/255.0f), ((((dt.Color2)>>24)&0xFF)/255.0f) };
			dt.pBrush = CreateLinearGradientBrush(dt.X, dt.Y, dt.Width, dt.Height, dt.Gradation, aColor, aAlpha);
		}
	}
	return;
}


/**
* コンストラクタ
*/
CD2d::CD2d()
{
	init();
	return;
}

/**
* デストラクタ
*/
CD2d::~CD2d()
{
	Release();

	// 第一インスタンスの場合は、GDI+やフォントの後始末
	m_Instances--;
	if( m_Instances == 0 )
	{
		D2DSafeRelease(&m_pD2DFactory);
		D2DSafeRelease(&m_pDWriteFactory);
		D2DSafeRelease(&m_pWICImagingFactory);
		deleteAllFonts();
		deleteAllStocksBrush();
		NDELETE(m_pBrushStocker);
	}
	NDELETE(m_pBitmapResource);
	NDELETE(m_pBitmapFileImage);
 	return;
}

/**リソースの解放
*/
void CD2d::Release()
{
	return;
}


/** 描画対象となるウィンドウを指定する
 @param hWnd ウィンドウハンドル
*/
bool CD2d::SetRenderTargetWindow(const HWND hWnd, const int width, const int height)
{
	assert( m_pD2DFactory != nullptr );
	assert( m_pD2DRenderTarget == nullptr );
	assert( m_TargetHWND == nullptr );

	m_TargetHWND = hWnd;
	m_TargetWind.W = width;
	m_TargetWind.H = height;
	restraintResourceOfD2DRenderTarget();
	return true;
}

/** 画像ファイルをDirect2Dリソースに変換して登録する
*/
void CD2d::SetPictureFileImage(const tstring &name, BYTE *pBin, const size_t size)
{
	assert(m_pBitmapFileImage != nullptr );
	PICTFILEINFO info;
	info.name = name, info.p = pBin, info.size = size;
	m_pBitmapFileImage->push_back(info);
	return;
}

/** 指定フォントの高さを得る
*/
float CD2d::GetFontHeight(const FONT fc) const
{
	auto index = static_cast<int>(fc);
	assert( 0 <= index && index < static_cast<int>(FONT::NUM) );
	const TEXT_FORMAT_DT &f_data = g_tableTextFormat[static_cast<int>(fc)];
	return f_data.fontSize;
}

/** 
* 初期処理
*/
void CD2d::init()
{
	// 第一インスタンスの場合は、GDI+やフォントの生成
	if( m_Instances == 0 )
	{
		HRESULT hr;
		// Direct2D Factroryの作成
		hr = D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
		// DirectWrite Factroryの作成 & フォントの準備
		hr = DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_pDWriteFactory) );
		createDefaultStockFonts(m_pDWriteFactory);
		// WIC ImagingFactoryの作成
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, reinterpret_cast<void **>(&m_pWICImagingFactory));
		HRESULT hr2 = hr;
		// ブラシストッカー
		m_pBrushStocker = NNEW std::vector<D2DColorBrushItem*>();
	}
	m_Instances++;

	//
	m_bDrawing = false;
	m_hOldFont = nullptr;
	m_TargetHWND = nullptr;
	m_bUseCanvasSize = false;
	m_pD2DRenderTarget = nullptr;
	m_pBitmapResource = NNEW std::vector<ID2D1Bitmap*>();
	m_pBitmapFileImage = NNEW std::vector<PICTFILEINFO>();
	m_LineScaling = 1.0f;
	return;
}

/**
* 図形描画のアンチエイリアス状態を返す
*
* @param [in] on true アンチエイリアスを有効にする
* @param [in] on false アンチエイリアスを無効にする
* @return なし
*/
bool CD2d::GetAntiAlias()
{
	return (m_pD2DRenderTarget->GetAntialiasMode() == D2D1_ANTIALIAS_MODE_ALIASED) ? false : true;
}

/**
* テキスト文字描画のアンチエイリアス状態を返す
*
* @param [in] on true アンチエイリアスを有効にする
* @param [in] on false アンチエイリアスを無効にする
* @return なし
*/
bool CD2d::GetAntiAliasText()
{
	return (m_pD2DRenderTarget->GetTextAntialiasMode() == D2D1_TEXT_ANTIALIAS_MODE_ALIASED) ? false : true;
}

/**
* 図形描画のアンチエイリアスＯＮ／ＯＦＦ
*
* @param [in] on true アンチエイリアスを有効にする
* @param [in] on false アンチエイリアスを無効にする
* @return なし
*/
void CD2d::AntiAlias(bool on)
{
	m_pD2DRenderTarget->SetAntialiasMode( (on) ? D2D1_ANTIALIAS_MODE_PER_PRIMITIVE : D2D1_ANTIALIAS_MODE_ALIASED );
	return;
}

/**
* テキスト文字描画のアンチエイリアスＯＮ／ＯＦＦ
*
* @param [in] on true アンチエイリアスを有効にする
* @param [in] on false アンチエイリアスを無効にする
* @return なし
*/
void CD2d::AntiAliasText(bool on)
{
	m_pD2DRenderTarget->SetTextAntialiasMode( (on) ? D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE : D2D1_TEXT_ANTIALIAS_MODE_ALIASED );
	return;
}

/**
* ライン幅スケールをセットする
*
* @param [in] scaling ライン幅スケール
* @return なし
*/
void CD2d::SetLineWidthScaling(const float scaling)
{
	m_LineScaling = scaling;
	return;
}

/**
* DPI値をセットする
*
* @param [in] dpiX, dpiY
* @return なし
*/
void CD2d::SetDpi(const float dpiX, const float dpiY)
{
    if( m_pD2DRenderTarget == nullptr )
		return;
	m_pD2DRenderTarget->SetDpi(dpiX, dpiY);
	return;
}


/** Viewサイズを格納する
*/
void CD2d::SetViewSize(const int vw, const int vh)
{
    if( m_pD2DRenderTarget == nullptr )
		return;
	m_pD2DRenderTarget->Resize(D2D1::SizeU(vw, vh));
	return;
}

/** Viewサイズを得る
*/
void CD2d::GetViewSize(int *pVw, int *pVh) const
{
// 未コーディング
//	*pVw = m_pMemDC->ViewWidth;
//	*pVh = m_pMemDC->ViewHeight;
	return;
}

/** 描画を開始する
*/
void CD2d::BeginDraw()
{
	if( m_pD2DRenderTarget != nullptr )
	{
		m_bDrawing = true;
		m_pD2DRenderTarget->BeginDraw();
		m_pD2DRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
	}
	return;
}

/** 描画を終了する
 @return false=リソースを作り直した。true=正常終了
*/
bool CD2d::EndDraw()
{
	if( m_pD2DRenderTarget == nullptr )
		return true;
	HRESULT hr = m_pD2DRenderTarget->EndDraw();
	m_bDrawing = false;

	if( hr == D2DERR_RECREATE_TARGET )
	{
		releaseResourceOfD2DRenderTarget();
		restraintResourceOfD2DRenderTarget();
		return false;
	}
	return true;
}

/** 描画を実行する
*/
void CD2d::Flush()
{
	if( m_pD2DRenderTarget == nullptr )
		return;
	HRESULT hr = m_pD2DRenderTarget->Flush();
	return;
}

/** ストックしているブラシを返す
*/
D2DColorBrushItem *CD2d::GetStockD2dBrush(
	const DWORD col1, const DWORD col2, const D2DRENDER::GRADMODE grad,
	const float sx, const float sy, const float lx, const float ly)
{
	D2DColorBrushItem *pItem = NNEW D2DColorBrushItem(col1, col2, grad, sx, sy, lx, ly);
	std::vector<D2DColorBrushItem*>::iterator it;
	for( it = m_pBrushStocker->begin(); it != m_pBrushStocker->end(); it++)
	{
		D2DColorBrushItem *pDt = *it;
		if( pItem->CompareTo(*pDt) ) 
		{
			NDELETE(pItem);
			return pDt;
		}
	}
	if( pItem->Color2 == 0 )
	{
		pItem->pBrush = CreateSolidColorBrush( pItem->Color1 & 0xFFFFFF,  ((((pItem->Color1)>>24)&0xFF)/255.0f) );
	}
	else
	{
		const RGBREF aColor[] = {pItem->Color1&0xFFFFFF, pItem->Color2&0xFFFFFF };
		const float aAlpha[] = {((((pItem->Color1)>>24)&0xFF)/255.0f), ((((pItem->Color2)>>24)&0xFF)/255.0f) };
		pItem->pBrush = CreateLinearGradientBrush(pItem->X, pItem->Y, pItem->Width, pItem->Height, pItem->Gradation, aColor, aAlpha);
	}
	m_pBrushStocker->push_back(pItem);
	return pItem;
}

/** ブラシの作成
*/
ID2D1SolidColorBrush *CD2d::CreateSolidColorBrush(const RGBREF col, const float alpha)
{
	assert( m_pD2DRenderTarget != nullptr );
	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );
	return pBrush;
}

/** グラデーションブラシの作成
*/
ID2D1LinearGradientBrush *CD2d::CreateLinearGradientBrush(
	const float sx, const float sy, const float dx, const float dy,
	const D2DRENDER::GRADMODE mode, const RGBREF aColor[], const float aAlpha[])
{
	const float aPosition[] = { 0.0f, 1.0f };
	ID2D1LinearGradientBrush *pBrush =
		CreateLinearGradientBrush(sx, sy, dx, dy, mode, aColor, aAlpha, aPosition, 2);
	return pBrush;
}

/** グラデーションブラシの作成（多色版）
*/
ID2D1LinearGradientBrush *CD2d::CreateLinearGradientBrush(
	const float sx, const float sy, const float dx, const float dy,
	const D2DRENDER::GRADMODE mode, const RGBREF aColor[], const float aAlpha[],
	const float aPosition[], const int num, BOOL bSRGB)
{
	// グラデーション色
	ID2D1GradientStopCollection *pGradientStops = nullptr;
	std::unique_ptr<D2D1_GRADIENT_STOP[]> pStops(NNEW D2D1_GRADIENT_STOP[num]);
	for( int t = 0; t < num; t++)
	{
		(pStops.get())[t].color = D2D1::ColorF(aColor[t], aAlpha[t]);
		(pStops.get())[t].position = aPosition[t];
	}
	m_pD2DRenderTarget->CreateGradientStopCollection(
		pStops.get(),
		num,
		(bSRGB) ? D2D1_GAMMA_2_2 : D2D1_GAMMA_1_0,
		D2D1_EXTEND_MODE_CLAMP,
		&pGradientStops );

	// 方向情報の作成
	D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES lgbp;
	makeGradiantProp(sx, sy, dx, dy, mode, &lgbp);

	// グラデーションブラシの作成
	ID2D1LinearGradientBrush *pBrush = nullptr;
    m_pD2DRenderTarget->CreateLinearGradientBrush( lgbp, pGradientStops, &pBrush );

	D2DSafeRelease(&pGradientStops);

	return pBrush;
}


ID2D1Factory *CD2d::GetFactory()
{
	return m_pD2DFactory;
}

void CD2d::ReleaseSolidColorBrush(ID2D1SolidColorBrush **ppBrush)
{
	D2DSafeRelease(ppBrush);
}

/**
* 描画領域を指定色で塗りつぶす
*
* @param [in] col RGB色値（RRGGBB）
* @param [in] alpha 0～1
* @return なし
*/
void CD2d::Clear(const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	m_pD2DRenderTarget->Clear( D2D1::ColorF((UINT32)col, alpha) );
	return;
}

/**
* 線を描画します
* 
* @param [in] sx 始点のＸ座標
* @param [in] sy 始点のＹ座標
* @param [in] dx 終点のＸ座標
* @param [in] dy 終点のＹ座標
* @param [in] width 線の太さ
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/
void CD2d::Line(const float sx, const float sy, const float dx, const float dy, const float width, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

	// 直線の描画
	D2D1_POINT_2F sp, dp;
	sp.x = sx, sp.y = sy;
	dp.x = dx, dp.y = dy;
	m_pD2DRenderTarget->DrawLine( sp, dp, pBrush, width*m_LineScaling);

	D2DSafeRelease(&pBrush);
	return;
}

/**
* 線を描画します
* 
* @param [in] sx 始点のＸ座標
* @param [in] sy 始点のＹ座標
* @param [in] dx 終点のＸ座標
* @param [in] dy 終点のＹ座標
* @param [in] width 線の太さ
* @param [in] pBrush ブラシオブジェクトへのポインタ
* @return なし
*/
void CD2d::Line(const float sx, const float sy, const float dx, const float dy, const float width, ID2D1Brush *pBrush)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	assert( pBrush != nullptr );

	// 直線の描画
	D2D1_POINT_2F sp, dp;
	sp.x = sx, sp.y = sy;
	dp.x = dx, dp.y = dy;
	m_pD2DRenderTarget->DrawLine( sp, dp, pBrush, width*m_LineScaling);
	return;
}

/**
* 枠のみ矩形を描画する
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] width 線の太さ
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/
void CD2d::Rect(const float sx, const float sy, const float lx, const float ly, const float width, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

	// 矩形の描画
	m_pD2DRenderTarget->DrawRectangle( D2D1::RectF( sx, sy, sx+lx-1, sy+ly-1), pBrush, width*m_LineScaling );

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* 枠のみ矩形を描画する
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] width 線の太さ
* @param [in] pBrush ブラシオブジェクトへのポインタ
* @return なし
*/
void CD2d::Rect(const float sx, const float sy, const float lx, const float ly, const float width, ID2D1Brush *pBrush)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	assert( pBrush != nullptr );
	// 矩形の描画
	m_pD2DRenderTarget->DrawRectangle( D2D1::RectF( sx, sy, sx+lx-1, sy+ly-1), pBrush, width*m_LineScaling );
	return;
}

/**
* 枠のみ角の丸い矩形を描画する
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] width 線の太さ
* @param [in] trim 角の丸み幅
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/
void CD2d::Rect(const float sx, const float sy, const float lx, const float ly, const float width, const float trim, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

	D2D1_ROUNDED_RECT rrect;
	rrect.rect = D2D1::RectF( sx, sy, sx+lx-1, sy+ly-1);
	rrect.radiusX = rrect.radiusY = trim;

	// 矩形の描画
	m_pD2DRenderTarget->DrawRoundedRectangle( rrect, pBrush, width*m_LineScaling);

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* 枠のみ角の丸い矩形を描画する
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] width 線の太さ
* @param [in] trim 角の丸み幅
* @param [in] pBrush ブラシオブジェクトへのポインタ
* @return なし
*/
void CD2d::Rect(const float sx, const float sy, const float lx, const float ly, const float width, const float trim, ID2D1Brush *pBrush)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	assert( pBrush != nullptr );

	D2D1_ROUNDED_RECT rrect;
	rrect.rect = D2D1::RectF( sx, sy, sx+lx-1, sy+ly-1);
	rrect.radiusX = rrect.radiusY = trim;

	// 矩形の描画
	m_pD2DRenderTarget->DrawRoundedRectangle( rrect, pBrush, width*m_LineScaling);
	return;
}

/**
* 枠のみ矩形を描画する（スタイル指定付き）
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] width 線の太さ
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/
void CD2d::Rect(const float sx, const float sy, const float lx, const float ly, const float width,
	ID2D1StrokeStyle *pStyle, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

	// 矩形の描画
	m_pD2DRenderTarget->DrawRectangle( D2D1::RectF( sx, sy, sx+lx-1, sy+ly-1), pBrush, width*m_LineScaling, pStyle );

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* 枠のみ矩形を描画する（スタイル指定付き）
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] width 線の太さ
* @param [in] pBrush ブラシオブジェクトへのポインタ
* @return なし
*/
void CD2d::Rect(const float sx, const float sy, const float lx, const float ly, const float width, ID2D1StrokeStyle *pStyle, ID2D1Brush *pBrush)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	assert( pBrush != nullptr );

	// 矩形の描画
	m_pD2DRenderTarget->DrawRectangle( D2D1::RectF( sx, sy, sx+lx-1, sy+ly-1), pBrush, width*m_LineScaling, pStyle );
	return;
}

/**
* 枠のみ角の丸い矩形を描画する（スタイル指定付き）
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] width 線の太さ
* @param [in] trim 角の丸み幅
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/
void CD2d::Rect(const float sx, const float sy, const float lx, const float ly, const float width,
	ID2D1StrokeStyle *pStyle, const float trim, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

	D2D1_ROUNDED_RECT rrect;
	rrect.rect = D2D1::RectF( sx, sy, sx+lx-1, sy+ly-1);
	rrect.radiusX = rrect.radiusY = trim;

	// 矩形の描画
	m_pD2DRenderTarget->DrawRoundedRectangle( rrect, pBrush, width*m_LineScaling, pStyle);

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* 枠のみ角の丸い矩形を描画する（スタイル指定付き）
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] width 線の太さ
* @param [in] trim 角の丸み幅
* @param [in] pBrush ブラシオブジェクトへのポインタ
* @return なし
*/
void CD2d::Rect(const float sx, const float sy, const float lx, const float ly, const float width, ID2D1StrokeStyle *pStyle, const float trim, ID2D1Brush *pBrush)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	assert( pBrush != nullptr );

	D2D1_ROUNDED_RECT rrect;
	rrect.rect = D2D1::RectF( sx, sy, sx+lx-1, sy+ly-1);
	rrect.radiusX = rrect.radiusY = trim;

	// 矩形の描画
	m_pD2DRenderTarget->DrawRoundedRectangle( rrect, pBrush, width*m_LineScaling, pStyle);
	return;
}

/**
* 塗りつぶし矩形を描画する
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/
void CD2d::RectPaint(const float sx, const float sy, const float lx, const float ly, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

	// 矩形の描画
	m_pD2DRenderTarget->FillRectangle( D2D1::RectF( sx, sy, (sx+lx-1), (sy+ly-1)), pBrush);

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* 塗りつぶし矩形を描画する
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] pBrush ブラシオブジェクトへのポインタ
* @return なし
*/
void CD2d::RectPaint(const float sx, const float sy, const float lx, const float ly, ID2D1Brush *pBrush)
{
	assert( pBrush != nullptr );
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// 矩形の描画
	m_pD2DRenderTarget->FillRectangle( D2D1::RectF( sx, sy, (sx+lx-1), (sy+ly-1)), pBrush);
	return;
}

/**
* 塗りつぶし角の丸い矩形を描画する
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] trim 角の丸み幅
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/
void CD2d::RectPaint(const float sx, const float sy, const float lx, const float ly, const float trim, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

	D2D1_ROUNDED_RECT rrect;
	rrect.rect = D2D1::RectF( sx, sy, (sx+lx-1), (sy+ly-1));
	rrect.radiusX = rrect.radiusY = trim;

	// 矩形の描画
	m_pD2DRenderTarget->FillRoundedRectangle( rrect, pBrush);

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* 塗りつぶし角の丸い矩形を描画する
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] trim 角の丸み幅
* @param [in] pBrush ブラシオブジェクトへのポインタ
* @return なし
*/
void CD2d::RectPaint(const float sx, const float sy, const float lx, const float ly, const float trim, ID2D1Brush *pBrush)
{
	assert( pBrush != nullptr );
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	D2D1_ROUNDED_RECT rrect;
	rrect.rect = D2D1::RectF( sx, sy, (sx+lx-1), (sy+ly-1));
	rrect.radiusX = rrect.radiusY = trim;

	// 矩形の描画
	m_pD2DRenderTarget->FillRoundedRectangle( rrect, pBrush);
	return;
}

/**
* グラデーション塗りつぶし矩形を描画する
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] col[] 塗りつぶし色（始点色と終点色の２色分）
* @param [in] alpha[] 塗りつぶし色アルファ（始点色と終点色の２色分）
* @param [in] mode グラデーション方向
* @return なし
*/

void CD2d::RectPaint(const float sx, const float sy, const float lx, const float ly, const RGBREF aColor[], const float aAlpha[], const D2DRENDER::GRADMODE mode)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// グラデーション色
	ID2D1LinearGradientBrush *pBrush = CreateLinearGradientBrush(sx, sy, lx, ly, mode, aColor,  aAlpha);

	// 矩形の描画
	m_pD2DRenderTarget->FillRectangle( D2D1::RectF( sx, sy, sx+lx-1, sy+ly-1), pBrush);

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* グラデーション塗りつぶし角の丸い矩形を描画する
*
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] trim 角の丸み幅
* @param [in] col[] 塗りつぶし色（始点色と終点色の２色分）
* @param [in] alpha[] 塗りつぶし色アルファ（始点色と終点色の２色分）
* @param [in] mode グラデーション方向
* @return なし
*/
void CD2d::RectPaint(const float sx, const float sy, const float lx, const float ly, const float trim, const RGBREF aColor[], const float aAlpha[], const D2DRENDER::GRADMODE mode)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// グラデーション色
	ID2D1LinearGradientBrush *pBrush = CreateLinearGradientBrush(sx, sy, lx, ly, mode, aColor,  aAlpha);

	// 矩形の描画
	D2D1_ROUNDED_RECT rrect;
	rrect.rect = D2D1::RectF( sx, sy, sx+lx-1, sy+ly-1);
	rrect.radiusX = rrect.radiusY = trim;
	m_pD2DRenderTarget->FillRoundedRectangle( rrect, pBrush);

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* 枠のみの円を描画する
* 
* 中心点と半径で指定するのではなく、円を内接する矩形の位置とサイズを指定する
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] width 線の太さ
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/

void CD2d::Ellipse(const float sx, const float sy, const float lx, const float ly, const float width, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

	// 矩形の描画
	D2D1_ELLIPSE ellipse;
	ellipse.radiusX = (lx+1) / 2;
	ellipse.radiusY = (ly+1) / 2;
	ellipse.point.x = sx + ellipse.radiusX;
	ellipse.point.y = sy + ellipse.radiusY;
	m_pD2DRenderTarget->DrawEllipse( ellipse, pBrush, width*m_LineScaling);

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* 枠のみの円を描画する
* 
* 中心点と半径で指定するのではなく、円を内接する矩形の位置とサイズを指定する
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] width 線の太さ
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/

void CD2d::Ellipse(const float sx, const float sy, const float lx, const float ly, const float width, ID2D1Brush *pBrush)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	assert( pBrush != nullptr );

	// 矩形の描画
	D2D1_ELLIPSE ellipse;
	ellipse.radiusX = (lx+1) / 2;
	ellipse.radiusY = (ly+1) / 2;
	ellipse.point.x = sx + ellipse.radiusX;
	ellipse.point.y = sy + ellipse.radiusY;
	m_pD2DRenderTarget->DrawEllipse( ellipse, pBrush, width*m_LineScaling);
	return;
}

/**
* 枠のみの円を描画する
* 
* 中心点と半径で指定する
* @param [in] sx 円の中心X座標
* @param [in] sy 円の中心Y座標
* @param [in] r 半径
* @param [in] width 線の太さ
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/
void CD2d::Ellipse(const float sx, const float sy, const float r, const float width, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

	// 円の描画
	D2D1_ELLIPSE ellipse;
	ellipse.radiusX = r;
	ellipse.radiusY = r;
	ellipse.point.x = sx;
	ellipse.point.y = sy;
	m_pD2DRenderTarget->DrawEllipse( ellipse, pBrush, width*m_LineScaling);

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* 枠のみの円を描画する
* 
* 中心点と半径で指定する
* @param [in] sx 円の中心X座標
* @param [in] sy 円の中心Y座標
* @param [in] r 半径
* @param [in] width 線の太さ
* @param [in] pBrush ブラシオブジェクトへのポインタ
* @return なし
*/
void CD2d::Ellipse(const float sx, const float sy, const float r, const float width, ID2D1Brush *pBrush)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	assert( pBrush != nullptr );

	// 円の描画
	D2D1_ELLIPSE ellipse;
	ellipse.radiusX = r;
	ellipse.radiusY = r;
	ellipse.point.x = sx;
	ellipse.point.y = sy;
	m_pD2DRenderTarget->DrawEllipse( ellipse, pBrush, width*m_LineScaling);
	return;
}

/**
* 塗りつぶし円形を描画
* 
* 中心点と半径で指定するのではなく、円を内包する矩形の位置とサイズを指定する
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/
void CD2d::EllipsePaint(const float sx, const float sy, const float lx, const float ly, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

	// 円の描画
	D2D1_ELLIPSE ellipse;
	ellipse.radiusX = (lx+1) / 2;
	ellipse.radiusY = (ly+1) / 2;
	ellipse.point.x = sx + ellipse.radiusX;
	ellipse.point.y = sy + ellipse.radiusY;
	m_pD2DRenderTarget->FillEllipse( ellipse, pBrush);

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* 塗りつぶし円形を描画
* 
* 中心点と半径で指定するのではなく、円を内包する矩形の位置とサイズを指定する
* @param [in] sx 矩形左上Ｘ座標
* @param [in] sy 矩形左上Ｙ座標
* @param [in] lx 横幅
* @param [in] ly 縦幅
* @param [in] pBrush ブラシオブジェクトへのポインタ
* @return なし
*/
void CD2d::EllipsePaint(const float sx, const float sy, const float lx, const float ly, ID2D1Brush *pBrush)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	assert( pBrush != nullptr );

	// 円の描画
	D2D1_ELLIPSE ellipse;
	ellipse.radiusX = (lx+1) / 2;
	ellipse.radiusY = (ly+1) / 2;
	ellipse.point.x = sx + ellipse.radiusX;
	ellipse.point.y = sy + ellipse.radiusY;
	m_pD2DRenderTarget->FillEllipse( ellipse, pBrush);
	return;
}

/**
* 塗りつぶし円形を描画
* 
* 中心点と半径で指定する
* @param [in] sx 中心のＸ座標
* @param [in] sy 中心のＹ座標
* @param [in] r 半径
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/
void CD2d::EllipsePaint(const float sx, const float sy, const float r, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

	// 円の描画
	D2D1_ELLIPSE ellipse;
	ellipse.radiusX = r;
	ellipse.radiusY = r;
	ellipse.point.x = sx;
	ellipse.point.y = sy;
	m_pD2DRenderTarget->FillEllipse( ellipse, pBrush);

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* 塗りつぶし円形を描画
* 
* 中心点と半径で指定する
* @param [in] sx 中心のＸ座標
* @param [in] sy 中心のＹ座標
* @param [in] r 半径
* @param [in] pBrush ブラシオブジェクトへのポインタ
* @return なし
*/
void CD2d::EllipsePaint(const float sx, const float sy, const float r, ID2D1Brush *pBrush)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// 円の描画
	D2D1_ELLIPSE ellipse;
	ellipse.radiusX = r;
	ellipse.radiusY = r;
	ellipse.point.x = sx;
	ellipse.point.y = sy;
	m_pD2DRenderTarget->FillEllipse( ellipse, pBrush);
	return;
}

/**
* グラデーション塗りつぶし円形を描画
* 
* 中心点と半径で指定する
* @param [in] sx 中心のＸ座標
* @param [in] sy 中心のＹ座標
* @param [in] r 半径
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* pparam [in] mode グラデーション方向
* @return なし
*/
void CD2d::EllipsePaint(const float sx, const float sy, const float r, const RGBREF aColor[], const float aAlpha[], const D2DRENDER::GRADMODE mode)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// グラデーション色
	ID2D1LinearGradientBrush *pBrush = CreateLinearGradientBrush(sx, sy, r*2, r*2, mode, aColor,  aAlpha);

	// 円の描画
	D2D1_ELLIPSE ellipse;
	ellipse.radiusX = r;
	ellipse.radiusY = r;
	ellipse.point.x = sx;
	ellipse.point.y = sy;
	m_pD2DRenderTarget->FillEllipse( ellipse, pBrush);

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* テキスト文字を描画します
* 
* @param [in] sx 描画位置のＸ座標
* @param [in] sy 描画位置のＹ座標
* @param [in] fc フォントコード（CD2d::FONT）
* @param [in] pText 文字列へのポインタ
* @param [in] pBrush ブラシオブジェクトへのポインタ
* @return なし
*/
void CD2d::Text(const float sx, const float sy, const FONT fc, const TCHAR *pText, ID2D1Brush *pBrush)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	assert( pBrush != nullptr );

	// 描画
	IDWriteTextFormat *pFormat = m_Fonts[static_cast<int>(fc)];
	pFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	pFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	pFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	m_pD2DRenderTarget->DrawText( pText, static_cast<UINT>(_tcslen(pText)), pFormat, D2D1::RectF(sx, sy, sx, sy), pBrush, D2D1_DRAW_TEXT_OPTIONS_NONE);
	return;
}

/**
* テキスト文字を描画します
* 
* @param [in] sx 描画位置のＸ座標
* @param [in] sy 描画位置のＹ座標
* @param [in] fc フォントコード（CD2d::FONT）
* @param [in] pText 文字列へのポインタ
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @return なし
*/
void CD2d::Text(const float sx, const float sy, const FONT fc, const TCHAR *pText, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

	// 描画
	IDWriteTextFormat *pFormat = m_Fonts[static_cast<int>(fc)];
	pFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	pFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	pFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	m_pD2DRenderTarget->DrawText( pText, static_cast<UINT>(_tcslen(pText)), pFormat, D2D1::RectF(sx, sy, sx, sy), pBrush, D2D1_DRAW_TEXT_OPTIONS_NONE);
	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

/**
* テキスト文字列の描画サイズを返す
*
* @param [in] fc フォントコード（CD2d::FONT）
* @param [in] pText 文字列へのポインタ
* @param [in] pSize サイズを格納する構造体へのポインタ
* @return なし
*/
void CD2d::MeasureText(const FONT fc, const TCHAR *pText, SIZE *pSize)
{
	assert( m_pDWriteFactory != nullptr );

	IDWriteTextFormat *pFormat = m_Fonts[static_cast<int>(fc)];
	pFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	pFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	pFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	measureText(pFormat, pText, pSize);
	return;
}

/**
* 指定領域の指定アライメントでのテキスト文字列の位置と描画サイズを返す
*
* @param [in] boxWidth 矩形横幅
* @param [in] boxHeight 矩形縦幅
* @param [in] fc フォントコード（CD2d::FONT）
* @param [in] pText 文字列へのポインタ
* @param [in] tav 矩形内での縦位置
* @param [in] tah 矩形内での横位置
* @param [out] pX X座標を格納する領域へのポインタ
* @param [out] pY Y座標を格納する領域へのポインタ
* @param [out] pSize サイズを格納する構造体へのポインタ
* @return なし
*/
void CD2d::MeasureTextInRect(const float boxWidth, const float boxHeight, const FONT fc,
	const TCHAR *pText, const D2DRENDER::TEXT_ALIMENT_V tav, const D2DRENDER::TEXT_ALIMENT_H tah, float *pX, float *pY, SIZE *pSize)
{
 	// 文字列の大きさ -> boxsize;
 	SIZE boxsize;
 	IDWriteTextFormat *pFormat = m_Fonts[static_cast<int>(fc)];
 	pFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
 	pFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
 	pFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
 	measureText(pFormat, pText, &boxsize);
 
 	// 表示位置の算出
 	float ssx = 0, ssy = 0;
 	switch(tav)
 	{
 		case D2DRENDER::TEXT_ALIMENT_V::TOP:	ssy += 0;							break;
 		case D2DRENDER::TEXT_ALIMENT_V::CENTER:	ssy += boxHeight/2 - boxsize.cy/2;	break;
 		case D2DRENDER::TEXT_ALIMENT_V::BOTTOM:	ssy += boxHeight - boxsize.cy;		break;
 	}
 	switch(tah)
 	{
 		case D2DRENDER::TEXT_ALIMENT_H::LEFT:	ssx += 0;							break;
 		case D2DRENDER::TEXT_ALIMENT_H::CENTER:	ssx += boxWidth/2 - boxsize.cx/2;	break;
 		case D2DRENDER::TEXT_ALIMENT_H::RIGHT:	ssx += boxWidth - boxsize.cx;		break;
 	}
	*pX = ssx;
	*pY = ssy;
	*pSize = boxsize;
	return;
}

/**
* テキスト文字（一文字）の描画サイズを返す
*
* @param [in] fc フォントコード（CD2d::FONT）
* @param [in] ch 文字
* @param [in] pSize サイズを格納する構造体へのポインタ
* @return なし
*/
void CD2d::MeasureCharactor(const FONT fc, const TCHAR ch, SIZE *pSize)
{
	assert( m_pDWriteFactory != nullptr );

	IDWriteTextFormat *pFormat = m_Fonts[static_cast<int>(fc)];
	pFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	pFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	pFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	const TCHAR aText[2] = { ch, '\0'};
	measureText(pFormat, aText, pSize);
	return;
}

/**
* 指定エリア内にテキスト文字を描画します
* 
* @param [in] sx 矩形左上のＸ座標
* @param [in] sy 矩形左上のＹ座標
* @param [in] boxWidth 矩形横幅
* @param [in] boxHeight 矩形縦幅
* @param [in] fc フォントコード（CD2d::FONT）
* @param [in] pText 文字列へのポインタ
* @param [in] col 文字色 RRGGBBh
* @param [in] alpha 文字色アルファ値 0～1
* @param [in] tav 矩形内での縦位置
* @param [in] tah 矩形内での横位置
* @return なし
*/
void CD2d::TextInRect(const float sx, const float sy, const float boxWidth, const float boxHeight,
	const FONT fc, const TCHAR *pText, const RGBREF col, const float alpha, const D2DRENDER::TEXT_ALIMENT_V tav, const D2DRENDER::TEXT_ALIMENT_H tah)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

	UINT len = static_cast<UINT>(_tcslen(pText));

	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );

 	// 文字列の大きさ -> boxsize;
 	SIZE boxsize;
 	IDWriteTextFormat *pFormat = m_Fonts[static_cast<int>(fc)];
 	pFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
 	pFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
 	pFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
 	measureText(pFormat, pText, len, &boxsize);

 	// 表示位置の算出
 	float ssx = sx, ssy = sy;
 	switch(tav)
 	{
 		case D2DRENDER::TEXT_ALIMENT_V::TOP:	ssy += 0;							break;
 		case D2DRENDER::TEXT_ALIMENT_V::CENTER:	ssy += boxHeight/2 - boxsize.cy/2;	break;
 		case D2DRENDER::TEXT_ALIMENT_V::BOTTOM:	ssy += boxHeight - boxsize.cy;		break;
 	}
 	switch(tah)
 	{
 		case D2DRENDER::TEXT_ALIMENT_H::LEFT:	ssx += 0;							break;
 		case D2DRENDER::TEXT_ALIMENT_H::CENTER:	ssx += boxWidth/2 - boxsize.cx/2;	break;
 		case D2DRENDER::TEXT_ALIMENT_H::RIGHT:	ssx += boxWidth - boxsize.cx;		break;
 	}
 
 	// 描画
 	m_pD2DRenderTarget->DrawText(
		pText, len,
		pFormat, D2D1::RectF(ssx, ssy, ssx+boxWidth, ssy+boxHeight), pBrush,
		D2D1_DRAW_TEXT_OPTIONS_NONE);

	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}


/**
* 指定エリア内にテキスト文字を描画します
* 
* @param [in] sx 矩形左上のＸ座標
* @param [in] sy 矩形左上のＹ座標
* @param [in] boxWidth 矩形横幅
* @param [in] boxHeight 矩形縦幅
* @param [in] fc フォントコード（CD2d::FONT）
* @param [in] pText 文字列へのポインタ
* @param [in] pBrush ブラシオブジェクトへのポインタ
* @param [in] tav 矩形内での縦位置
* @param [in] tah 矩形内での横位置
* @return なし
*/
void CD2d::TextInRect(const float sx, const float sy, const float boxWidth, const float boxHeight,
	const FONT fc, const TCHAR *pText, ID2D1Brush *pBrush, const D2DRENDER::TEXT_ALIMENT_V tav, const D2DRENDER::TEXT_ALIMENT_H tah)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );

 	// 文字列の大きさ -> boxsize;
 	SIZE boxsize;
 	IDWriteTextFormat *pFormat = m_Fonts[static_cast<int>(fc)];
 	pFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
 	pFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
 	pFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
 	measureText(pFormat, pText, &boxsize);

 	// 表示位置の算出
 	float ssx = sx, ssy = sy;
 	switch(tav)
 	{
		case D2DRENDER::TEXT_ALIMENT_V::TOP:	ssy += 0;							break;
 		case D2DRENDER::TEXT_ALIMENT_V::CENTER:	ssy += boxHeight/2 - boxsize.cy/2;	break;
 		case D2DRENDER::TEXT_ALIMENT_V::BOTTOM:	ssy += boxHeight - boxsize.cy;		break;
 	}
 	switch(tah)
 	{
		case D2DRENDER::TEXT_ALIMENT_H::LEFT:	ssx += 0;							break;
		case D2DRENDER::TEXT_ALIMENT_H::CENTER:	ssx += boxWidth/2 - boxsize.cx/2;	break;
		case D2DRENDER::TEXT_ALIMENT_H::RIGHT:	ssx += boxWidth - boxsize.cx;		break;
 	}
 
 	// 描画
 	m_pD2DRenderTarget->DrawText(
		pText, static_cast<UINT>(_tcslen(pText)),
		pFormat, D2D1::RectF(ssx, ssy, ssx+boxWidth, ssy+boxHeight), pBrush,
		D2D1_DRAW_TEXT_OPTIONS_NONE);
	return;
}

/**
* ジオメトリパスで指定された図形を塗りつぶしで描画します
*/
void CD2d::GeometryPaint(ID2D1PathGeometry *pGeo, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );
	// 描画
	m_pD2DRenderTarget->FillGeometry(pGeo, pBrush);
	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}

void CD2d::GeometryPaint(ID2D1PathGeometry *pGeo, ID2D1Brush *pBrush)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	// 描画
	m_pD2DRenderTarget->FillGeometry(pGeo, pBrush);
	return;
}

/**
* ジオメトリパスで指定された図形を描画します
*/
void CD2d::Geometry(ID2D1PathGeometry *pGeo, const float width, const RGBREF col, const float alpha)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	// ブラシの作成
	ID2D1SolidColorBrush *pBrush = nullptr;
	m_pD2DRenderTarget->CreateSolidColorBrush( D2D1::ColorF((UINT32)col, alpha), &pBrush );
	// 描画
	m_pD2DRenderTarget->DrawGeometry(pGeo, pBrush, width);
	// 後片付け
	D2DSafeRelease(&pBrush);
	return;
}


/**
* ジオメトリパスで指定された図形を描画します
*/
void CD2d::Geometry(ID2D1PathGeometry *pGeo, const float width, ID2D1Brush *pBrush)
{
	assert( m_bDrawing );
	assert( m_pD2DRenderTarget != nullptr );
	assert( pBrush != nullptr );
	// 描画
	m_pD2DRenderTarget->DrawGeometry(pGeo, pBrush, width);
	return;
}

/**
* 指定した画像のサイズを返す
* 
* @param [in] pictNo 画像番号 １～
* @param [in] *pWidth 横幅
* @param [in] *pHeight 縦幅
*/
void CD2d::GetPictureSize(const int pictNo, float *pWidth, float *pHeight)
{
	assert( 0 < pictNo && pictNo <= (int)m_pBitmapResource->size() );
	if( m_pBitmapResource->empty() )
		return;	
	ID2D1Bitmap *pBitmap = m_pBitmapResource->at(pictNo-1);
	D2D1_SIZE_F size = pBitmap->GetSize();
	*pWidth = size.width;
	*pHeight = size.height;
	return;
}

/**
* 指定名称に対応する画像へのポインタを返す
* 
* @param [in] name 画像名称
* @return オブジェクトへのポインタ（見つからなかった場合は、nullptr）
*/
ID2D1Bitmap *CD2d::GetPictureObj(const tstring &name)
{
	if( m_pBitmapResource->empty() )
		return nullptr;
	for( int t = 0; t < (int)m_pBitmapFileImage->size(); t++ )
	{
		const PICTFILEINFO &info = m_pBitmapFileImage->at(t);
		if( info.name == name )
		{
			ID2D1Bitmap *pBitmap = m_pBitmapResource->at(t);
			return pBitmap;
		}
	}
	return nullptr;
}

/**
* 画像を描画します
* 
* @param [in] sx 左上のＸ座標
* @param [in] sy 左上のＹ座標
* @param [in] pBitmap 画像オブジェクトへのポインタ
*/
void CD2d::DrawPicture(const float sx, const float sy, ID2D1Bitmap *pBitmap)
{
	assert( nullptr != pBitmap );
	D2D1_SIZE_F size = pBitmap->GetSize();
	m_pD2DRenderTarget->DrawBitmap( pBitmap, D2D1::RectF(sx, sy, sx+size.width, sy+size.height) );	// sx+size.width等に-1しなくてOKです。
	return;
}






