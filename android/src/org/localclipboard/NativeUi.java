package org.localclipboard;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.widget.ArrayAdapter;
import org.json.JSONArray;
import java.lang.ref.WeakReference;

/** Native Android dialogs; Qt polls the result without blocking Android's UI thread. */
public final class NativeUi {
    private static int request;
    private static String value = "cancel";
    private static AlertDialog dialog;
    private static volatile WeakReference<Activity> host = new WeakReference<>(null);
    private static volatile WeakReference<QrScannerActivity> scanner = new WeakReference<>(null);
    private static synchronized int begin() { ++request; value = ""; return request; }
    public static synchronized String result(int id) { return id == request ? value : "cancel"; }
    public static synchronized void complete(int id, String result) {
        if (id == request && value.isEmpty()) value = result;
    }
    public static void cancel(int id) {
        complete(id, "cancel");
        Activity activity = host.get();
        if (activity != null) activity.runOnUiThread(() -> {
            synchronized (NativeUi.class) { if (id != request) return; }
            if (dialog != null) dialog.dismiss();
            QrScannerActivity camera = scanner.get();
            if (camera != null && !camera.isFinishing()) camera.finish();
        });
    }
    static void scannerOpened(QrScannerActivity activity, int id) {
        scanner = new WeakReference<>(activity);
        if (!result(id).isEmpty()) activity.finish();
    }
    public static int choose(Context context, String title, String json, int selected, String cancel) {
        final int id = begin();
        if (!(context instanceof Activity)) { complete(id, "cancel"); return id; }
        Activity activity = (Activity)context;
        host = new WeakReference<>(activity);
        activity.runOnUiThread(() -> {
            try {
                if (activity.isFinishing() || !result(id).isEmpty()) { complete(id, "cancel"); return; }
                JSONArray rows = new JSONArray(json);
                String[] labels = new String[rows.length()];
                boolean[] enabled = new boolean[rows.length()];
                for (int i = 0; i < rows.length(); ++i) {
                    labels[i] = rows.getJSONObject(i).getString("text");
                    enabled[i] = rows.getJSONObject(i).optBoolean("enabled", true);
                }
                ArrayAdapter<String> adapter = new ArrayAdapter<String>(activity, android.R.layout.select_dialog_singlechoice, labels) {
                    @Override public boolean areAllItemsEnabled() { return false; }
                    @Override public boolean isEnabled(int position) { return enabled[position]; }
                    @Override public android.view.View getView(int position, android.view.View recycled, android.view.ViewGroup parent) {
                        android.view.View view = super.getView(position, recycled, parent);
                        if (view instanceof android.widget.TextView) ((android.widget.TextView)view).setSingleLine(false);
                        view.setEnabled(enabled[position]);
                        return view;
                    }
                };
                dialog = new AlertDialog.Builder(activity).setTitle(title)
                    .setSingleChoiceItems(adapter, selected, (whichDialog, which) -> {
                        if (!enabled[which]) return;
                        complete(id, Integer.toString(which)); whichDialog.dismiss();
                    }).setNegativeButton(cancel, (d, which) -> complete(id, "cancel"))
                    .setOnCancelListener(d -> complete(id, "cancel")).create();
                dialog.setOnDismissListener(d -> { complete(id, "cancel"); dialog = null; });
                dialog.show();
            } catch (Exception e) {
                android.util.Log.e("LocalClipboard", "Cannot show choices", e);
                complete(id, "cancel");
            }
        });
        return id;
    }
    public static int scan(Context context) {
        final int id = begin();
        if (!(context instanceof Activity)) { complete(id, "cancel"); return id; }
        Activity activity = (Activity)context;
        host = new WeakReference<>(activity);
        activity.runOnUiThread(() -> {
            try { activity.startActivity(new Intent(activity, QrScannerActivity.class).putExtra("request", id)); }
            catch (Exception e) { android.util.Log.e("LocalClipboard", "Cannot open scanner", e); complete(id, "cancel"); }
        });
        return id;
    }
    public static void ownerDestroyed(Activity activity) {
        // A native scanner intentionally pauses QtActivity; only destruction cancels its request.
        cancel(request);
    }
    private NativeUi() { }
}
