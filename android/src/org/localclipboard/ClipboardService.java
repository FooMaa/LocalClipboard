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
    private static ClipboardService instance;
    private WifiManager.MulticastLock multicast;
    private Notification notification(String text, boolean ongoing) {
        Intent open = new Intent(this, ClipboardActivity.class);
        PendingIntent pending = PendingIntent.getActivity(this, 0, open,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        return new Notification.Builder(this, "clipboard")
                .setSmallIcon(org.localclipboard.R.drawable.ic_notification)
                .setContentTitle("LocalClipboard").setContentText(text)
                .setContentIntent(pending).setOngoing(ongoing).setAutoCancel(!ongoing).build();
    }
    @Override public void onCreate() {
        instance = this;
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel("clipboard", ClipboardActivity.text(this, "Clipboard synchronization", "Синхронизация буфера"), NotificationManager.IMPORTANCE_LOW));
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
        if (service != null) service.getSystemService(NotificationManager.class)
                .notify(2, service.notification(ClipboardActivity.text(service, "Clipboard received — tap to apply", "Буфер получен — нажмите, чтобы применить"), false));
    }
    @Override public void onDestroy() {
        instance = null;
        if (multicast != null && multicast.isHeld()) multicast.release();
        super.onDestroy();
    }
}
