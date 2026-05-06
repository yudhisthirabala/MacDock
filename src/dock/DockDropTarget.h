// DockDropTarget.h
// OLE IDropTarget implementation for the Dock window.
// Replaces WM_DROPFILES (unreliable with layered/topmost windows on Win10/11)
// with the robust RegisterDragDrop / IDropTarget COM pipeline.

#pragma once
#include <windows.h>
#include <oleidl.h>
#include <string>
#include <vector>

class DockWindow; // forward declaration

class DockDropTarget : public IDropTarget
{
public:
    explicit DockDropTarget(DockWindow* dock);

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;
    ULONG   STDMETHODCALLTYPE AddRef() override;
    ULONG   STDMETHODCALLTYPE Release() override;

    // IDropTarget
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState,
                                         POINTL pt, DWORD* pdwEffect) override;
    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt,
                                        DWORD* pdwEffect) override;
    HRESULT STDMETHODCALLTYPE DragLeave() override;
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState,
                                    POINTL pt, DWORD* pdwEffect) override;

private:
    // Extract file paths from the data object's CF_HDROP format
    static std::vector<std::wstring> ExtractFilePaths(IDataObject* pDataObj);

    // Returns true if the data object contains CF_HDROP data
    static bool HasFileData(IDataObject* pDataObj);

    LONG        m_refCount;
    DockWindow* m_dock;
    bool        m_hasValidData; // cached from DragEnter
};
