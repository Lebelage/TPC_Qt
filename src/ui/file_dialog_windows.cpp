#include "ui/file_dialog.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>

namespace tpc_slint::ui {

std::optional<std::filesystem::path> showVtkSaveDialog() {
    const HRESULT initialization = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool should_uninitialize = SUCCEEDED(initialization);

    IFileSaveDialog* dialog = nullptr;
    const HRESULT created = CoCreateInstance(
        CLSID_FileSaveDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog)
    );
    if (FAILED(created)) {
        if (should_uninitialize) {
            CoUninitialize();
        }
        return std::nullopt;
    }

    const COMDLG_FILTERSPEC filters[]{{L"VTK field (*.vtk)", L"*.vtk"}};
    dialog->SetTitle(L"Export VTK field");
    dialog->SetFileName(L"field.vtk");
    dialog->SetDefaultExtension(L"vtk");
    dialog->SetFileTypes(1, filters);
    dialog->SetFileTypeIndex(1);

    DWORD options{};
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT);
    }

    std::optional<std::filesystem::path> selected_path;
    if (SUCCEEDED(dialog->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                selected_path.emplace(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }

    dialog->Release();
    if (should_uninitialize) {
        CoUninitialize();
    }
    return selected_path;
}

}  // namespace tpc_slint::ui
