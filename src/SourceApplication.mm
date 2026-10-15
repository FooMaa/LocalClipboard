#include "SourceApplication.h"
#import <AppKit/AppKit.h>
bool SourceApplication::supported() { return true; }
QString SourceApplication::current() {
    @autoreleasepool {
        NSRunningApplication *app = NSWorkspace.sharedWorkspace.frontmostApplication;
        return app.bundleIdentifier ? QString::fromNSString(app.bundleIdentifier) : QString();
    }
}
