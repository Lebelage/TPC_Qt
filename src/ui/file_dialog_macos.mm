#include "ui/file_dialog.hpp"

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace tpc_slint::ui {

std::optional<std::filesystem::path> showVtkSaveDialog() {
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];
        panel.title = @"Export VTK field";
        panel.nameFieldStringValue = @"field.vtk";
        UTType* vtk_type = [UTType typeWithFilenameExtension:@"vtk"];
        if (vtk_type != nil) {
            panel.allowedContentTypes = @[vtk_type];
        }
        panel.allowsOtherFileTypes = NO;
        panel.canCreateDirectories = YES;
        panel.extensionHidden = NO;

        if ([panel runModal] != NSModalResponseOK || panel.URL == nil) {
            return std::nullopt;
        }

        const char* path = panel.URL.fileSystemRepresentation;
        if (path == nullptr) {
            return std::nullopt;
        }
        return std::filesystem::path{path};
    }
}

}  // namespace tpc_slint::ui
