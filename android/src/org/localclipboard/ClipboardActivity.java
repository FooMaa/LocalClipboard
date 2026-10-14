package org.localclipboard;

import android.Manifest;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.webkit.MimeTypeMap;
import androidx.core.content.FileProvider;
import org.json.JSONArray;
import org.json.JSONObject;
import org.qtproject.qt.android.bindings.QtActivity;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.UUID;
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.atomic.AtomicReference;

public final class ClipboardActivity extends QtActivity {
    private static final AtomicReference<String> pendingShare = new AtomicReference<>("");
    private final ExecutorService importer = Executors.newSingleThreadExecutor();
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED)
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 7);
        handleShare(getIntent());
    }
    @Override public void onNewIntent(Intent intent) { super.onNewIntent(intent); setIntent(intent); handleShare(intent); }
    @Override public void onDestroy() { importer.shutdown(); super.onDestroy(); }
    public static void setBootEnabled(Context context, boolean enabled) {
        context.getSharedPreferences("startup", Context.MODE_PRIVATE).edit().putBoolean("enabled", enabled).apply();
    }
    public static void startSync(Context context) {
        Intent service = new Intent(context, ClipboardService.class);
        if (Build.VERSION.SDK_INT >= 26) context.startForegroundService(service); else context.startService(service);
    }
    public static void stopSync(Context context) { context.stopService(new Intent(context, ClipboardService.class)); }
    public static String takeShare() { return pendingShare.getAndSet(""); }
    private void handleShare(Intent intent) {
        String action = intent.getAction();
        if (!Intent.ACTION_SEND.equals(action) && !Intent.ACTION_SEND_MULTIPLE.equals(action)) return;
        importer.execute(() -> {
            File folder = new File(getFilesDir(), "shared/" + UUID.randomUUID());
            try {
                ArrayList<Uri> uris = new ArrayList<>();
                if (Intent.ACTION_SEND_MULTIPLE.equals(action)) {
                    ArrayList<Uri> supplied = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
                    if (supplied != null) uris.addAll(supplied);
                } else {
                    Uri uri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
                    if (uri != null) uris.add(uri);
                }
                JSONObject result = new JSONObject();
                if (uris.isEmpty()) {
                    CharSequence text = intent.getCharSequenceExtra(Intent.EXTRA_TEXT);
                    if (text == null) throw new Exception("Nothing to share.");
                    result.put("text", text.toString());
                } else {
                    if (uris.size() > 32) throw new Exception("Select at most 32 files.");
                    if (!folder.mkdirs()) throw new Exception("Cannot create share directory.");
                    JSONArray files = new JSONArray();
                    long total = 0;
                    for (Uri uri : uris) {
                        if (!"content".equals(uri.getScheme())) throw new Exception("Only content-provider files can be shared.");
                        String name = "shared-file";
                        try (Cursor cursor = getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) name = cursor.getString(0);
                        }
                        name = name.replaceAll("[\\x00-\\x1f<>:\"/\\\\|?*]", "_");
                        if (name.length() > 110) name = name.substring(name.length() - 110);
                        File file = new File(folder, files.length() + "-" + name);
                        try (InputStream input = getContentResolver().openInputStream(uri); FileOutputStream output = new FileOutputStream(file)) {
                            if (input == null) throw new Exception("Cannot read shared file.");
                            byte[] buffer = new byte[65536]; int count;
                            while ((count = input.read(buffer)) != -1) {
                                total += count;
                                if (total > 32L * 1024 * 1024) throw new Exception("Files exceed 32 MiB.");
                                output.write(buffer, 0, count);
                            }
                        }
                        files.put(file.getAbsolutePath());
                    }
                    result.put("files", files);
                }
                // Do not overwrite an unconsumed share; keep the user informed.
                if (!pendingShare.compareAndSet("", result.toString())) {
                    deleteFolder(folder);
                    runOnUiThread(() -> android.widget.Toast.makeText(this, "Previous share is still waiting. Open LocalClipboard and try again.", android.widget.Toast.LENGTH_LONG).show());
                }
            } catch (Exception e) {
                deleteFolder(folder);
                try { pendingShare.compareAndSet("", new JSONObject().put("error", e.getMessage()).toString()); } catch (Exception ignored) { }
            }
        });
    }
    private static void deleteFolder(File folder) {
        File[] files = folder.listFiles();
        if (files != null) for (File file : files) file.delete();
        folder.delete();
    }
    private static Uri uri(Context context, String path) {
        return FileProvider.getUriForFile(context, context.getPackageName() + ".qtprovider", new File(path));
    }
    private static String mime(String path) {
        String extension = MimeTypeMap.getFileExtensionFromUrl(Uri.fromFile(new File(path)).toString());
        String type = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension.toLowerCase(java.util.Locale.ROOT));
        return type == null ? "application/octet-stream" : type;
    }
    public static void copyFiles(Context context, String json) {
        try {
            JSONArray paths = new JSONArray(json);
            if (paths.length() == 0) return;
            ClipData clip = ClipData.newUri(context.getContentResolver(), "LocalClipboard files", uri(context, paths.getString(0)));
            for (int i = 1; i < paths.length(); ++i) clip.addItem(new ClipData.Item(uri(context, paths.getString(i))));
            ((ClipboardManager)context.getSystemService(CLIPBOARD_SERVICE)).setPrimaryClip(clip);
        } catch (Exception e) { android.util.Log.e("LocalClipboard", "Cannot copy received files", e); }
    }
    public static void openFile(Context context, String path) {
        try {
            Intent open = new Intent(Intent.ACTION_VIEW).setDataAndType(uri(context, path), mime(path))
                    .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(open);
        } catch (Exception e) { android.widget.Toast.makeText(context, "No app can open this file.", android.widget.Toast.LENGTH_LONG).show(); }
    }
}
