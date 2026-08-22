/*
 * Copyright 2005 Jacek Caban
 * Copyright 2010 Ilya Shpigor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "ieframe.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ieframe);

#define CAPTURE_WINDOW_ROP (SRCCOPY | 0x40000000) /* CAPTUREBLT */

/**********************************************************************
 * Implement the IViewObject interface
 */

static inline WebBrowser *impl_from_IViewObject2(IViewObject2 *iface)
{
    return CONTAINING_RECORD(iface, WebBrowser, IViewObject2_iface);
}

static HRESULT WINAPI ViewObject_QueryInterface(IViewObject2 *iface, REFIID riid, void **ppv)
{
    WebBrowser *This = impl_from_IViewObject2(iface);
    return IUnknown_QueryInterface(This->hlink_frame.outer, riid, ppv);
}

static ULONG WINAPI ViewObject_AddRef(IViewObject2 *iface)
{
    WebBrowser *This = impl_from_IViewObject2(iface);
    return IUnknown_AddRef(This->hlink_frame.outer);
}

static ULONG WINAPI ViewObject_Release(IViewObject2 *iface)
{
    WebBrowser *This = impl_from_IViewObject2(iface);
    return IUnknown_Release(This->hlink_frame.outer);
}

static HRESULT capture_window_tree(HWND hwnd, HWND root, const RECT *parent_clip, HDC hdc,
        BOOL (STDMETHODCALLTYPE *continue_func)(ULONG_PTR), ULONG_PTR continue_cookie,
        BOOL *copied)
{
    RECT client_rect, window_rect, clip_rect;
    POINT origin = {0, 0};
    HWND child, next;
    HDC window_dc;

    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd))
        return S_OK;
    if (continue_func && !continue_func(continue_cookie))
        return E_ABORT;
    if (!GetClientRect(hwnd, &client_rect))
        return VIEW_E_DRAW;
    if (IsRectEmpty(&client_rect))
        return S_OK;

    SetLastError(ERROR_SUCCESS);
    if (!MapWindowPoints(hwnd, root, &origin, 1) && GetLastError() != ERROR_SUCCESS)
        return VIEW_E_DRAW;
    SetRect(&window_rect, origin.x, origin.y, origin.x + client_rect.right,
            origin.y + client_rect.bottom);
    if (!IntersectRect(&clip_rect, &window_rect, parent_clip))
        return S_OK;

    window_dc = GetDC(hwnd);
    if (!window_dc)
        return VIEW_E_DRAW;
    if (!BitBlt(hdc, clip_rect.left, clip_rect.top,
            clip_rect.right - clip_rect.left, clip_rect.bottom - clip_rect.top,
            window_dc, clip_rect.left - origin.x, clip_rect.top - origin.y,
            CAPTURE_WINDOW_ROP))
    {
        ReleaseDC(hwnd, window_dc);
        return VIEW_E_DRAW;
    }
    ReleaseDC(hwnd, window_dc);
    *copied = TRUE;

    child = GetWindow(hwnd, GW_CHILD);
    if (child)
        child = GetWindow(child, GW_HWNDLAST);
    while (child)
    {
        HRESULT hr;

        next = GetWindow(child, GW_HWNDPREV);
        hr = capture_window_tree(child, root, &clip_rect, hdc, continue_func,
                continue_cookie, copied);
        if (FAILED(hr))
            return hr;
        child = next;
    }
    return S_OK;
}

static HRESULT WINAPI ViewObject_Draw(IViewObject2 *iface, DWORD dwDrawAspect,
        LONG lindex, void *pvAspect, DVTARGETDEVICE *ptd, HDC hdcTargetDev,
        HDC hdcDraw, LPCRECTL lprcBounds, LPCRECTL lprcWBounds,
        BOOL (STDMETHODCALLTYPE *pfnContinue)(ULONG_PTR),
        ULONG_PTR dwContinue)
{
    WebBrowser *This = impl_from_IViewObject2(iface);
    HBITMAP bitmap, old_bitmap;
    HDC capture_dc;
    RECT capture_rect;
    HRESULT hr;
    BOOL copied = FALSE;
    INT old_stretch_mode;

    TRACE("(%p)->(%ld %ld %p %p %p %p %p %p %p %08Ix)\n", This, dwDrawAspect, lindex,
            pvAspect, ptd, hdcTargetDev, hdcDraw, lprcBounds, lprcWBounds, pfnContinue,
            dwContinue);

    if (dwDrawAspect != DVASPECT_CONTENT)
        return DV_E_DVASPECT;
    if (lindex != -1)
        return DV_E_LINDEX;
    if (!hdcDraw || !lprcBounds)
        return E_INVALIDARG;
    if (lprcBounds->right <= lprcBounds->left
            || lprcBounds->bottom <= lprcBounds->top)
        return OLE_E_INVALIDRECT;
    if (!This->doc_host.document)
        return OLE_E_BLANK;
    if (pfnContinue && !pfnContinue(dwContinue))
        return E_ABORT;
    if (!IsWindow(This->shell_embedding_hwnd))
        return VIEW_E_DRAW;
    if (!RedrawWindow(This->shell_embedding_hwnd, NULL, NULL,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW))
        return VIEW_E_DRAW;
    if (!This->doc_host.document)
        return OLE_E_BLANK;
    if (!IsWindow(This->shell_embedding_hwnd)
            || !GetClientRect(This->shell_embedding_hwnd, &capture_rect)
            || IsRectEmpty(&capture_rect))
        return VIEW_E_DRAW;

    capture_dc = CreateCompatibleDC(hdcDraw);
    if (!capture_dc)
        return VIEW_E_DRAW;
    bitmap = CreateCompatibleBitmap(hdcDraw, capture_rect.right, capture_rect.bottom);
    if (!bitmap)
    {
        DeleteDC(capture_dc);
        return VIEW_E_DRAW;
    }
    old_bitmap = SelectObject(capture_dc, bitmap);
    if (!old_bitmap || old_bitmap == HGDI_ERROR)
    {
        DeleteObject(bitmap);
        DeleteDC(capture_dc);
        return VIEW_E_DRAW;
    }

    if (!FillRect(capture_dc, &capture_rect, (HBRUSH)(COLOR_WINDOW + 1)))
        hr = VIEW_E_DRAW;
    else
        hr = capture_window_tree(This->shell_embedding_hwnd, This->shell_embedding_hwnd,
                &capture_rect, capture_dc, pfnContinue, dwContinue, &copied);
    if (SUCCEEDED(hr) && !copied)
        hr = VIEW_E_DRAW;
    if (SUCCEEDED(hr) && pfnContinue && !pfnContinue(dwContinue))
        hr = E_ABORT;
    if (SUCCEEDED(hr))
    {
        old_stretch_mode = SetStretchBltMode(hdcDraw, COLORONCOLOR);
        if (!StretchBlt(hdcDraw, lprcBounds->left, lprcBounds->top,
                lprcBounds->right - lprcBounds->left,
                lprcBounds->bottom - lprcBounds->top, capture_dc, 0, 0,
                capture_rect.right, capture_rect.bottom, SRCCOPY))
            hr = VIEW_E_DRAW;
        if (old_stretch_mode)
            SetStretchBltMode(hdcDraw, old_stretch_mode);
    }

    SelectObject(capture_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(capture_dc);
    return hr;
}

static HRESULT WINAPI ViewObject_GetColorSet(IViewObject2 *iface, DWORD dwAspect,
        LONG lindex, void *pvAspect, DVTARGETDEVICE *ptd, HDC hicTargetDev,
        LOGPALETTE **ppColorSet)
{
    WebBrowser *This = impl_from_IViewObject2(iface);
    FIXME("(%p)->(%ld %ld %p %p %p %p)\n", This, dwAspect, lindex, pvAspect, ptd,
            hicTargetDev, ppColorSet);
    return E_NOTIMPL;
}

static HRESULT WINAPI ViewObject_Freeze(IViewObject2 *iface, DWORD dwDrawAspect, LONG lindex,
                                        void *pvAspect, DWORD *pdwFreeze)
{
    WebBrowser *This = impl_from_IViewObject2(iface);
    FIXME("(%p)->(%ld %ld %p %p)\n", This, dwDrawAspect, lindex, pvAspect, pdwFreeze);
    return E_NOTIMPL;
}

static HRESULT WINAPI ViewObject_Unfreeze(IViewObject2 *iface, DWORD dwFreeze)
{
    WebBrowser *This = impl_from_IViewObject2(iface);
    FIXME("(%p)->(%ld)\n", This, dwFreeze);
    return E_NOTIMPL;
}

static HRESULT WINAPI ViewObject_SetAdvise(IViewObject2 *iface, DWORD aspects, DWORD advf,
        IAdviseSink *pAdvSink)
{
    WebBrowser *This = impl_from_IViewObject2(iface);

    TRACE("(%p)->(%ld %08lx %p)\n", This, aspects, advf, pAdvSink);

    if (aspects || advf) FIXME("aspects and/or flags not supported yet\n");

    This->sink_aspects = aspects;
    This->sink_flags = advf;
    if (This->sink) IAdviseSink_Release(This->sink);
    This->sink = pAdvSink;
    if (This->sink) IAdviseSink_AddRef(This->sink);

    return S_OK;
}

static HRESULT WINAPI ViewObject_GetAdvise(IViewObject2 *iface, DWORD *pAspects,
        DWORD *pAdvf, IAdviseSink **ppAdvSink)
{
    WebBrowser *This = impl_from_IViewObject2(iface);

    TRACE("(%p)->(%p %p %p)\n", This, pAspects, pAdvf, ppAdvSink);

    if (pAspects) *pAspects = This->sink_aspects;
    if (pAdvf) *pAdvf = This->sink_flags;
    if (ppAdvSink)
    {
        *ppAdvSink = This->sink;
        if (*ppAdvSink) IAdviseSink_AddRef(*ppAdvSink);
    }

    return S_OK;
}

static HRESULT WINAPI ViewObject_GetExtent(IViewObject2 *iface, DWORD dwAspect, LONG lindex,
        DVTARGETDEVICE *ptd, LPSIZEL lpsizel)
{
    WebBrowser *This = impl_from_IViewObject2(iface);
    FIXME("(%p)->(%ld %ld %p %p)\n", This, dwAspect, lindex, ptd, lpsizel);
    return E_NOTIMPL;
}

static const IViewObject2Vtbl ViewObjectVtbl = {
    ViewObject_QueryInterface,
    ViewObject_AddRef,
    ViewObject_Release,
    ViewObject_Draw,
    ViewObject_GetColorSet,
    ViewObject_Freeze,
    ViewObject_Unfreeze,
    ViewObject_SetAdvise,
    ViewObject_GetAdvise,
    ViewObject_GetExtent
};

/**********************************************************************
 * Implement the IDataObject interface
 */

static inline WebBrowser *impl_from_IDataObject(IDataObject *iface)
{
    return CONTAINING_RECORD(iface, WebBrowser, IDataObject_iface);
}

static HRESULT WINAPI DataObject_QueryInterface(LPDATAOBJECT iface, REFIID riid, LPVOID * ppvObj)
{
    WebBrowser *This = impl_from_IDataObject(iface);
    return IUnknown_QueryInterface(This->hlink_frame.outer, riid, ppvObj);
}

static ULONG WINAPI DataObject_AddRef(LPDATAOBJECT iface)
{
    WebBrowser *This = impl_from_IDataObject(iface);
    return IUnknown_AddRef(This->hlink_frame.outer);
}

static ULONG WINAPI DataObject_Release(LPDATAOBJECT iface)
{
    WebBrowser *This = impl_from_IDataObject(iface);
    return IUnknown_Release(This->hlink_frame.outer);
}

static HRESULT WINAPI DataObject_GetData(LPDATAOBJECT iface, LPFORMATETC pformatetcIn, STGMEDIUM *pmedium)
{
    WebBrowser *This = impl_from_IDataObject(iface);
    FIXME("(%p)->()\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObject_GetDataHere(LPDATAOBJECT iface, LPFORMATETC pformatetc, STGMEDIUM *pmedium)
{
    WebBrowser *This = impl_from_IDataObject(iface);
    FIXME("(%p)->()\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObject_QueryGetData(LPDATAOBJECT iface, LPFORMATETC pformatetc)
{
    WebBrowser *This = impl_from_IDataObject(iface);
    FIXME("(%p)->()\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObject_GetCanonicalFormatEtc(LPDATAOBJECT iface, LPFORMATETC pformatectIn, LPFORMATETC pformatetcOut)
{
    WebBrowser *This = impl_from_IDataObject(iface);
    FIXME("(%p)->()\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObject_SetData(LPDATAOBJECT iface, LPFORMATETC pformatetc, STGMEDIUM *pmedium, BOOL fRelease)
{
    WebBrowser *This = impl_from_IDataObject(iface);
    FIXME("(%p)->()\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObject_EnumFormatEtc(LPDATAOBJECT iface, DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc)
{
    WebBrowser *This = impl_from_IDataObject(iface);
    FIXME("(%p)->()\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObject_DAdvise(LPDATAOBJECT iface, FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection)
{
    WebBrowser *This = impl_from_IDataObject(iface);
    FIXME("(%p)->()\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObject_DUnadvise(LPDATAOBJECT iface, DWORD dwConnection)
{
    WebBrowser *This = impl_from_IDataObject(iface);
    FIXME("(%p)->()\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObject_EnumDAdvise(LPDATAOBJECT iface, IEnumSTATDATA **ppenumAdvise)
{
    WebBrowser *This = impl_from_IDataObject(iface);
    FIXME("(%p)->()\n", This);
    return E_NOTIMPL;
}

static const IDataObjectVtbl DataObjectVtbl = {
    DataObject_QueryInterface,
    DataObject_AddRef,
    DataObject_Release,
    DataObject_GetData,
    DataObject_GetDataHere,
    DataObject_QueryGetData,
    DataObject_GetCanonicalFormatEtc,
    DataObject_SetData,
    DataObject_EnumFormatEtc,
    DataObject_DAdvise,
    DataObject_DUnadvise,
    DataObject_EnumDAdvise
};

void WebBrowser_ViewObject_Init(WebBrowser *This)
{
    This->IViewObject2_iface.lpVtbl = &ViewObjectVtbl;
    This->IDataObject_iface.lpVtbl  = &DataObjectVtbl;
}
