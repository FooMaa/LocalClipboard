package org.localclipboard;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.net.wifi.WifiManager;
import org.qtproject.qt.android.bindings.QtService;

public final class ClipboardService extends QtService {
    private static volatile ClipboardService instance;
    private WifiManager.MulticastLock multicast;
    private Notification notification(String text, boolean ongoing) {
        Intent open = new Intent(this, ClipboardActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent pending = PendingIntent.getActivity(this, 0, open,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        return new Notification.Builder(this, ongoing ? "clipboard" : "clipboard-received-v1")
                .setSmallIcon(org.localclipboard.R.drawable.ic_notification)
                .setContentTitle("LocalClipboard").setContentText(text)
                .setContentIntent(pending).setOngoing(ongoing).setAutoCancel(!ongoing)
                .setCategory(ongoing ? Notification.CATEGORY_SERVICE : Notification.CATEGORY_MESSAGE)
                .setVisibility(Notification.VISIBILITY_PRIVATE).setOnlyAlertOnce(ongoing).build();
    }
    @Override public void onCreate() {
        instance = this;
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel("clipboard", ClipboardActivity.text(this, "Clipboard synchronization", "Синхронизация буфера"), NotificationManager.IMPORTANCE_LOW));
        manager.createNotificationChannel(new NotificationChannel("clipboard-received-v1", ClipboardActivity.text(this,
                "Received clipboard", "Получение буфера"), NotificationManager.IMPORTANCE_HIGH));
        Notification current = notification(ClipboardActivity.text(this, "Listening for your registered devices", "Ожидание ваших устройств"), true);
        if (Build.VERSION.SDK_INT >= 29) startForeground(1, current, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE);
        else startForeground(1, current);
        WifiManager wifi = (WifiManager)getApplicationContext().getSystemService(WIFI_SERVICE);
        if (wifi != null) {
            multicast = wifi.createMulticastLock("localclipboard-discovery");
            multicast.setReferenceCounted(false); multicast.acquire();
        }
        super.onCreate();
    }
    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        super.onStartCommand(intent, flags, startId);
        return START_STICKY;
    }
    public static void received() {
        ClipboardService service = instance;
        if (service == null) return;
        NotificationManager manager = service.getSystemService(NotificationManager.class);
        if (!manager.areNotificationsEnabled()) return;
        manager.notify(2, service.notification(ClipboardActivity.text(service,
                "Clipboard received — tap to apply", "Буфер получен — нажмите, чтобы применить"), false));
    }
    @Override public void onDestroy() {
        instance = null;
        if (multicast != null && multicast.isHeld()) multicast.release();
        super.onDestroy();
    }
}
