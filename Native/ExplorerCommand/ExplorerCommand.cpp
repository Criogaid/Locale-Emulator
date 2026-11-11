// Windows 11 first-level context-menu verb ("Run with Locale Emulator") for .exe files.
// Hosted by a sparse package (see AppxManifest.xml): Explorer loads this x64 DLL through
// the package's COM surrogate and calls IExplorerCommand. The verb only forwards the
// selected file to the LEProc.exe next to this DLL, which already selects the helper
// matching the target's architecture.
#include <windows.h>
#include <new>
#include <shellapi.h>
#include <shobjidl_core.h>
#include <shlwapi.h>
#include <strsafe.h>

// {7F0B3C5A-2D41-4E8B-9A67-5C1E3B8D2A94}
static const CLSID CLSID_LERunCommand =
    {0x7f0b3c5a, 0x2d41, 0x4e8b, {0x9a, 0x67, 0x5c, 0x1e, 0x3b, 0x8d, 0x2a, 0x94}};

static HMODULE g_module = nullptr;
static volatile LONG g_objectCount = 0;

namespace {

struct Title {
    LANGID primary;
    PCWSTR text;
};

// The product UI follows the Windows UI language; the verb does the same.
const Title kTitles[] = {
    {LANG_CHINESE, L"使用 Locale Emulator 运行"},
    {LANG_JAPANESE, L"Locale Emulator で実行"},
    {LANG_KOREAN, L"Locale Emulator로 실행"},
    {LANG_RUSSIAN, L"Запустить через Locale Emulator"},
};
const WCHAR kDefaultTitle[] = L"Run with Locale Emulator";

PCWSTR ResolveTitle() {
    const LANGID primary = PRIMARYLANGID(GetUserDefaultUILanguage());
    for (const Title& title : kTitles) {
        if (title.primary == primary)
            return title.text;
    }
    return kDefaultTitle;
}

// Full path of LEProc.exe in the directory that holds this DLL.
HRESULT GetLauncherPath(WCHAR (&path)[MAX_PATH]) {
    const DWORD length = GetModuleFileNameW(g_module, path, ARRAYSIZE(path));
    if (length == 0 || length >= ARRAYSIZE(path))
        return HRESULT_FROM_WIN32(length == 0 ? GetLastError() : ERROR_INSUFFICIENT_BUFFER);
    if (!PathRemoveFileSpecW(path) || !PathAppendW(path, L"LEProc.exe"))
        return E_FAIL;
    return S_OK;
}

class RunCommand final : public IExplorerCommand {
public:
    RunCommand() { InterlockedIncrement(&g_objectCount); }
    ~RunCommand() { InterlockedDecrement(&g_objectCount); }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
        if (object == nullptr)
            return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IExplorerCommand) {
            *object = static_cast<IExplorerCommand*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&m_refs)); }
    IFACEMETHODIMP_(ULONG) Release() override {
        const LONG refs = InterlockedDecrement(&m_refs);
        if (refs == 0)
            delete this;
        return static_cast<ULONG>(refs);
    }

    // IExplorerCommand
    IFACEMETHODIMP GetTitle(IShellItemArray*, LPWSTR* name) override {
        return SHStrDupW(ResolveTitle(), name);
    }
    IFACEMETHODIMP GetIcon(IShellItemArray*, LPWSTR* icon) override {
        WCHAR path[MAX_PATH];
        *icon = nullptr;
        HRESULT hr = GetLauncherPath(path);
        if (FAILED(hr))
            return hr;
        WCHAR spec[MAX_PATH + 4];
        hr = StringCchPrintfW(spec, ARRAYSIZE(spec), L"%s,0", path);
        return SUCCEEDED(hr) ? SHStrDupW(spec, icon) : hr;
    }
    IFACEMETHODIMP GetToolTip(IShellItemArray*, LPWSTR* tip) override {
        *tip = nullptr;
        return E_NOTIMPL;
    }
    IFACEMETHODIMP GetCanonicalName(GUID* name) override {
        *name = CLSID_LERunCommand;
        return S_OK;
    }
    IFACEMETHODIMP GetState(IShellItemArray* items, BOOL, EXPCMDSTATE* state) override {
        // The manifest already limits the verb to .exe; hide it for multi-selection
        // because LEProc starts exactly one target.
        DWORD count = 0;
        *state = ECS_ENABLED;
        if (items != nullptr && SUCCEEDED(items->GetCount(&count)) && count != 1)
            *state = ECS_HIDDEN;
        return S_OK;
    }
    IFACEMETHODIMP Invoke(IShellItemArray* items, IBindCtx*) override {
        if (items == nullptr)
            return E_INVALIDARG;
        IShellItem* item = nullptr;
        HRESULT hr = items->GetItemAt(0, &item);
        if (FAILED(hr))
            return hr;
        PWSTR target = nullptr;
        hr = item->GetDisplayName(SIGDN_FILESYSPATH, &target);
        item->Release();
        if (FAILED(hr))
            return hr;

        WCHAR launcher[MAX_PATH];
        hr = GetLauncherPath(launcher);
        if (SUCCEEDED(hr)) {
            // LEProc takes the target as its single argument and parses it with the
            // standard Windows rules, so a quoted path is enough.
            WCHAR parameters[32768];
            hr = StringCchPrintfW(parameters, ARRAYSIZE(parameters), L"\"%s\"", target);
            if (SUCCEEDED(hr)) {
                SHELLEXECUTEINFOW info = {sizeof(info)};
                info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
                info.lpFile = launcher;
                info.lpParameters = parameters;
                info.nShow = SW_SHOWNORMAL;
                hr = ShellExecuteExW(&info) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
            }
        }
        CoTaskMemFree(target);
        return hr;
    }
    IFACEMETHODIMP GetFlags(EXPCMDFLAGS* flags) override {
        *flags = ECF_DEFAULT;
        return S_OK;
    }
    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** commands) override {
        *commands = nullptr;
        return E_NOTIMPL;
    }

private:
    LONG m_refs = 1;
};

class Factory final : public IClassFactory {
public:
    Factory() { InterlockedIncrement(&g_objectCount); }
    ~Factory() { InterlockedDecrement(&g_objectCount); }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
        if (object == nullptr)
            return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *object = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&m_refs)); }
    IFACEMETHODIMP_(ULONG) Release() override {
        const LONG refs = InterlockedDecrement(&m_refs);
        if (refs == 0)
            delete this;
        return static_cast<ULONG>(refs);
    }
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** object) override {
        if (object == nullptr)
            return E_POINTER;
        *object = nullptr;
        if (outer != nullptr)
            return CLASS_E_NOAGGREGATION;
        RunCommand* command = new (std::nothrow) RunCommand();
        if (command == nullptr)
            return E_OUTOFMEMORY;
        const HRESULT hr = command->QueryInterface(riid, object);
        command->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL lock) override {
        if (lock)
            InterlockedIncrement(&g_objectCount);
        else
            InterlockedDecrement(&g_objectCount);
        return S_OK;
    }

private:
    LONG m_refs = 1;
};

} // namespace

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* object) {
    if (object == nullptr)
        return E_POINTER;
    *object = nullptr;
    if (!IsEqualCLSID(clsid, CLSID_LERunCommand))
        return CLASS_E_CLASSNOTAVAILABLE;
    Factory* factory = new (std::nothrow) Factory();
    if (factory == nullptr)
        return E_OUTOFMEMORY;
    const HRESULT hr = factory->QueryInterface(riid, object);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return g_objectCount == 0 ? S_OK : S_FALSE;
}
