#include "IOSBridge.h"
#include "I18n.h"
#include "Ui.h"
#include "ClipboardPayload.h"
#include "Profile.h"
#include "PendingFiles.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#import <UIKit/UIKit.h>
#import <PhotosUI/PhotosUI.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace {
UIViewController *presenter() {
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if (scene.activationState != UISceneActivationStateForegroundActive || ![scene isKindOfClass:UIWindowScene.class]) continue;
        for (UIWindow *window in ((UIWindowScene *)scene).windows) {
            if (!window.isKeyWindow) continue;
            UIViewController *controller = window.rootViewController;
            while (controller.presentedViewController) controller = controller.presentedViewController;
            return controller;
        }
    }
    // Qt 6.8 also supports the UIApplication lifecycle without a scene manifest.
    for (UIWindow *window in UIApplication.sharedApplication.windows) {
        if (!window.isKeyWindow) continue;
        UIViewController *controller = window.rootViewController;
        while (controller.presentedViewController) controller = controller.presentedViewController;
        return controller;
    }
    return nil;
}
void anchor(UIPopoverPresentationController *popover, UIViewController *controller) {
    popover.sourceView = controller.view;
    popover.sourceRect = CGRectMake(CGRectGetMidX(controller.view.bounds), CGRectGetMidY(controller.view.bounds), 1, 1);
}
NSURL *newImportDirectory() {
    NSString *root = [Profile::dataDirectory().toNSString() stringByAppendingPathComponent:@"imported"];
    NSURL *folder = [NSURL fileURLWithPath:[root stringByAppendingPathComponent:NSUUID.UUID.UUIDString] isDirectory:YES];
    NSError *error = nil;
    if (![NSFileManager.defaultManager createDirectoryAtURL:folder withIntermediateDirectories:YES
        attributes:@{NSFileProtectionKey: NSFileProtectionCompleteUntilFirstUserAuthentication} error:&error]) return nil;
    return folder;
}
NSURL *copyFile(NSURL *source, NSURL *directory, NSUInteger index, unsigned long long *total, NSError **error) {
    BOOL scoped = [source startAccessingSecurityScopedResource];
    __block NSURL *destination = nil;
    // File providers may need to download the selected file before it is readable.
    NSFileCoordinator *coordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];
    __block NSError *copyError = nil;
    [coordinator coordinateReadingItemAtURL:source options:0 error:&copyError byAccessor:^(NSURL *readURL) {
        NSNumber *size = nil, *regular = nil, *symlink = nil;
        [readURL getResourceValue:&size forKey:NSURLFileSizeKey error:&copyError];
        [readURL getResourceValue:&regular forKey:NSURLIsRegularFileKey error:&copyError];
        [readURL getResourceValue:&symlink forKey:NSURLIsSymbolicLinkKey error:&copyError];
        if (!size || !regular.boolValue || symlink.boolValue || size.unsignedLongLongValue > ClipboardPayload::MaxContentBytes - *total) {
            copyError = [NSError errorWithDomain:@"LocalClipboard" code:1 userInfo:@{NSLocalizedDescriptionKey:@"Select regular files, up to 32 MiB combined."}];
            return;
        }
        NSString *name = readURL.lastPathComponent;
        if (!ClipboardPayload::safeName(QString::fromNSString(name))) name = @"file";
        NSURL *candidate = [directory URLByAppendingPathComponent:[NSString stringWithFormat:@"%lu-%@", (unsigned long)index, name]];
        if ([NSFileManager.defaultManager copyItemAtURL:readURL toURL:candidate error:&copyError]) {
            *total += size.unsignedLongLongValue;
            destination = candidate;
        }
    }];
    if (scoped) [source stopAccessingSecurityScopedResource];
    if (error) *error = copyError;
    return destination;
}
void shareFiles(const QList<QUrl> &urls) {
    UIViewController *controller = presenter();
    if (!controller) return;
    NSMutableArray *items = [NSMutableArray array];
    for (const auto &url : urls) [items addObject:[NSURL fileURLWithPath:url.toLocalFile().toNSString()]];
    UIActivityViewController *activity = [[UIActivityViewController alloc] initWithActivityItems:items applicationActivities:nil];
    anchor(activity.popoverPresentationController, controller);
    [controller presentViewController:activity animated:YES completion:nil];
}
}

@interface LCFilePicker : NSObject <UIDocumentPickerDelegate, PHPickerViewControllerDelegate>
@property(nonatomic, copy) void (^completion)(NSArray<NSURL *> *, NSString *);
@property(nonatomic, strong) NSURL *directory;
@property(nonatomic, strong) NSMutableArray<NSURL *> *files;
@property(nonatomic) unsigned long long total;
- (void)loadResults:(NSArray<PHPickerResult *> *)results index:(NSUInteger)index;
@end
static LCFilePicker *activePicker;
@implementation LCFilePicker
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    QList<QUrl> inputs;
    for (NSURL *url in urls) inputs.append(QUrl::fromLocalFile(QString::fromNSString(url.path)));
    auto completion = self.completion;
    IOSBridge::importFiles(inputs, [completion](const QList<QUrl> &files, const QString &error) {
        NSMutableArray *output = [NSMutableArray array];
        for (const auto &url : files) [output addObject:[NSURL fileURLWithPath:url.toLocalFile().toNSString()]];
        completion(output, error.toNSString());
    });
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller { self.completion(@[], nil); }
- (void)picker:(PHPickerViewController *)picker didFinishPicking:(NSArray<PHPickerResult *> *)results {
    [picker dismissViewControllerAnimated:YES completion:^{
        if (results.count == 0) { self.completion(@[], nil); return; }
        self.directory = newImportDirectory();
        self.files = [NSMutableArray array];
        self.total = 0;
        if (!self.directory) { self.completion(@[], @"Cannot create import folder."); return; }
        [self loadResults:results index:0];
    }];
}
- (void)loadResults:(NSArray<PHPickerResult *> *)results index:(NSUInteger)index {
    if (index == results.count) { self.completion(self.files, nil); return; }
    NSItemProvider *provider = results[index].itemProvider;
    NSString *type = nil;
    for (NSString *candidate in provider.registeredTypeIdentifiers) {
        UTType *uti = [UTType typeWithIdentifier:candidate];
        if ([uti conformsToType:UTTypeImage] || [uti conformsToType:UTTypeMovie]) { type = candidate; break; }
    }
    if (!type) { self.completion(@[], @"This photo or video format cannot be imported."); return; }
    [provider loadFileRepresentationForTypeIdentifier:type completionHandler:^(NSURL *url, NSError *providerError) {
        NSError *copyError = providerError;
        unsigned long long total = self.total;
        NSURL *copied = url ? copyFile(url, self.directory, index + 1, &total, &copyError) : nil;
        // Provider URLs expire when this callback returns: copy before dispatching.
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!copied) {
                [NSFileManager.defaultManager removeItemAtURL:self.directory error:nil];
                self.completion(@[], copyError.localizedDescription ?: @"Cannot import selected photo.");
                return;
            }
            self.total = total;
            [self.files addObject:copied];
            [self loadResults:results index:index + 1];
        });
    }];
}
@end

void IOSBridge::importFiles(const QList<QUrl> &urls, FilesReady ready) {
    if (urls.isEmpty() || urls.size() > ClipboardPayload::MaxFiles) { ready({}, "Select between 1 and 32 files."); return; }
    NSURL *folder = newImportDirectory();
    if (!folder) { ready({}, "Cannot create import folder."); return; }
    QList<QUrl> copied;
    unsigned long long total = 0;
    for (const auto &url : urls) {
        if (!url.isLocalFile()) {
            [NSFileManager.defaultManager removeItemAtURL:folder error:nil];
            ready({}, "Only local files can be imported."); return;
        }
        NSError *error = nil;
        NSURL *file = copyFile([NSURL fileURLWithPath:url.toLocalFile().toNSString()], folder, copied.size() + 1, &total, &error);
        if (!file) {
            [NSFileManager.defaultManager removeItemAtURL:folder error:nil];
            ready({}, error ? QString::fromNSString(error.localizedDescription) : QStringLiteral("Cannot import file.")); return;
        }
        copied.append(QUrl::fromLocalFile(QString::fromNSString(file.path)));
    }
    ready(copied, {});
}
void IOSBridge::pickFiles(QWidget *parent, FilesReady ready) {
    Q_UNUSED(parent);
    UIViewController *controller = presenter();
    if (!controller || activePicker) { ready({}, "A file picker is already open or the app is inactive."); return; }
    activePicker = [LCFilePicker new];
    activePicker.completion = ^(NSArray<NSURL *> *urls, NSString *error) {
        QList<QUrl> files;
        for (NSURL *url in urls) files.append(QUrl::fromLocalFile(QString::fromNSString(url.path)));
        activePicker = nil;
        ready(files, QString::fromNSString(error ?: @""));
    };
    UIAlertController *menu = [UIAlertController alertControllerWithTitle:I18n::text("Send files / photos").toNSString() message:nil preferredStyle:UIAlertControllerStyleActionSheet];
    [menu addAction:[UIAlertAction actionWithTitle:I18n::text("Photos and videos").toNSString() style:UIAlertActionStyleDefault handler:^(UIAlertAction *) {
        PHPickerConfiguration *config = [[PHPickerConfiguration alloc] init];
        config.selectionLimit = ClipboardPayload::MaxFiles;
        config.filter = [PHPickerFilter anyFilterMatchingSubfilters:@[PHPickerFilter.imagesFilter, PHPickerFilter.videosFilter]];
        PHPickerViewController *picker = [[PHPickerViewController alloc] initWithConfiguration:config];
        picker.delegate = activePicker;
        [controller presentViewController:picker animated:YES completion:nil];
    }]];
    [menu addAction:[UIAlertAction actionWithTitle:I18n::text("Files").toNSString() style:UIAlertActionStyleDefault handler:^(UIAlertAction *) {
        UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeData] asCopy:YES];
        picker.allowsMultipleSelection = YES;
        picker.delegate = activePicker;
        [controller presentViewController:picker animated:YES completion:nil];
    }]];
    [menu addAction:[UIAlertAction actionWithTitle:I18n::text("Cancel").toNSString() style:UIAlertActionStyleCancel handler:^(UIAlertAction *) { activePicker.completion(@[], nil); }]];
    anchor(menu.popoverPresentationController, controller);
    [controller presentViewController:menu animated:YES completion:nil];
}
void IOSBridge::copyFiles(const QList<QUrl> &urls) {
    NSMutableArray *items = [NSMutableArray array];
    for (const auto &url : urls) {
        NSString *path = url.toLocalFile().toNSString();
        NSData *data = [NSData dataWithContentsOfFile:path];
        if (!data) continue;
        UTType *type = [UTType typeWithFilenameExtension:path.pathExtension] ?: UTTypeData;
        [items addObject:@{type.identifier: data}];
    }
    // Transfer bytes, not inaccessible file:// URLs from our private sandbox.
    [UIPasteboard.generalPasteboard setItems:items options:@{UIPasteboardOptionLocalOnly:@YES}];
}
void IOSBridge::openReceivedFiles(QWidget *parent) {
    QDialog dialog(parent); Ui::title(&dialog, "Received / imported files");
    const auto form = Ui::form(&dialog, "Received / imported files", "Select files to share, save or send again.");
    auto *layout = form.body;
    auto *list = new QListWidget(&dialog); list->setSelectionMode(QAbstractItemView::ExtendedSelection); list->setMinimumHeight(160); list->setWordWrap(true); layout->addWidget(list);
    for (const QString &folder : {QStringLiteral("received"), QStringLiteral("imported")}) {
        QDirIterator it(QDir(Profile::dataDirectory()).filePath(folder), QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            auto *item = new QListWidgetItem(folder + "/" + QFileInfo(path).fileName(), list); item->setData(Qt::UserRole, path);
        }
    }
    auto *share = Ui::button("Share / Save…", &dialog); layout->addWidget(share);
    QObject::connect(share, &QPushButton::clicked, &dialog, [list] {
        QList<QUrl> urls;
        for (auto *item : list->selectedItems()) urls.append(QUrl::fromLocalFile(item->data(Qt::UserRole).toString()));
        if (!urls.isEmpty()) shareFiles(urls);
    });
    auto *retry = Ui::button("Send again", &dialog); layout->addWidget(retry);
    QObject::connect(retry, &QPushButton::clicked, &dialog, [&dialog, list] {
        QList<QUrl> urls;
        for (auto *item : list->selectedItems()) urls.append(QUrl::fromLocalFile(item->data(Qt::UserRole).toString()));
        if (urls.isEmpty()) return;
        IOSBridge::importFiles(urls, [&dialog](const QList<QUrl> &copies, const QString &error) {
            QString message = error;
            if (message.isEmpty() && IOSBridge::queueFiles(Profile::load(), copies, &message)) dialog.accept();
            else QMessageBox::warning(&dialog, I18n::text("Files"), I18n::text(message));
        });
    });
    auto *remove = Ui::button("Delete local copies", &dialog); layout->addWidget(remove);
    QObject::connect(remove, &QPushButton::clicked, &dialog, [&dialog, list] {
        if (list->selectedItems().isEmpty() || QMessageBox::question(&dialog, I18n::text("Delete files"), I18n::text("Delete the selected copies stored in LocalClipboard?")) != QMessageBox::Yes) return;
        for (auto *item : list->selectedItems()) {
            if (QFile::remove(item->data(Qt::UserRole).toString())) delete item;
        }
    });
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, &dialog); form.footer->addWidget(close);
    QObject::connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

QList<QUrl> IOSBridge::queuedFiles(const Profile &profile) { return PendingFiles::load(profile); }
bool IOSBridge::queueFiles(const Profile &profile, const QList<QUrl> &files, QString *error) {
    return PendingFiles::append(profile, files, error);
}
void IOSBridge::clearQueue(const Profile &profile) { PendingFiles::clear(profile); }
