package org.localclipboard;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.graphics.Matrix;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.*;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.os.Bundle;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Size;
import android.view.Surface;
import android.view.TextureView;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Camera preview is rendered by Android. Bounded, throttled QR decoding runs off the UI thread. */
public final class QrScannerActivity extends Activity implements TextureView.SurfaceTextureListener {
    private TextureView preview;
    private TextView status;
    private HandlerThread worker;
    private Handler handler;
    private CameraDevice camera;
    private CameraCaptureSession session;
    private ImageReader reader;
    private Surface surface;
    private Size size;
    private volatile boolean resumed;
    private boolean opening;
    private volatile int generation;
    private int focusMode;
    private int request;
    private long lastFrame;
    private int closing;
    private volatile boolean readingImage;
    private final ExecutorService imageDecoder = Executors.newSingleThreadExecutor();
    private static native String decode(byte[] luminance, int width, int height);

    private String text(String en, String ru) { return ClipboardActivity.text(this, en, ru); }
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        request = getIntent().getIntExtra("request", -1);
        NativeUi.scannerOpened(this, request);
        LinearLayout layout = new LinearLayout(this); layout.setOrientation(LinearLayout.VERTICAL);
        int padding = (int)(16 * getResources().getDisplayMetrics().density);
        layout.setPadding(padding, padding, padding, padding);
        if (Build.VERSION.SDK_INT >= 35) layout.setOnApplyWindowInsetsListener((view, insets) -> {
            android.graphics.Insets bars = insets.getInsets(android.view.WindowInsets.Type.systemBars()
                    | android.view.WindowInsets.Type.displayCutout());
            view.setPadding(padding + bars.left, padding + bars.top, padding + bars.right, padding + bars.bottom);
            return insets;
        });
        status = new TextView(this); status.setTextSize(18);
        status.setText(text("Point the camera at your other device's connection QR code.", "Наведите камеру на QR-код подключения на другом устройстве."));
        layout.addView(status);
        preview = new TextureView(this); preview.setSurfaceTextureListener(this);
        layout.addView(preview, new LinearLayout.LayoutParams(-1, 0, 1));
        Button image = new Button(this); image.setText(text("Read QR from image…", "Распознать QR на изображении…"));
        image.setOnClickListener(v -> {
            if (readingImage) return;
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("image/*").addCategory(Intent.CATEGORY_OPENABLE);
            try { startActivityForResult(pick, 2); }
            catch (Exception e) { error(text("No image picker available.", "Нет приложения для выбора изображения.")); }
        });
        layout.addView(image);
        Button permission = new Button(this); permission.setText(text("Camera settings", "Настройки камеры"));
        permission.setOnClickListener(v -> startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                android.net.Uri.parse("package:" + getPackageName()))));
        layout.addView(permission);
        Button cancel = new Button(this); cancel.setText(text("Cancel", "Отмена"));
        cancel.setOnClickListener(v -> finish()); layout.addView(cancel);
        setContentView(layout);
    }
    @Override public void onResume() {
        super.onResume(); resumed = true;
        worker = new HandlerThread("LocalClipboard-QR"); worker.start(); handler = new Handler(worker.getLooper());
        if (checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED)
            requestPermissions(new String[]{Manifest.permission.CAMERA}, 1);
        else openCamera();
    }
    @Override public void onRequestPermissionsResult(int code, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(code, permissions, results);
        if (code != 1) return;
        if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) openCamera();
        else error(text("Allow camera access in settings, or choose an image.", "Разрешите доступ к камере в настройках или выберите изображение."));
    }
    @Override public void onPause() {
        resumed = false; opening = false; ++generation;
        final CameraCaptureSession oldSession = session; session = null;
        final CameraDevice oldCamera = camera; camera = null;
        final ImageReader oldReader = reader; reader = null;
        final Surface oldSurface = surface; surface = null;
        final HandlerThread oldWorker = worker; worker = null;
        // Close after the decoder releases its Image; never join it from Android's UI thread.
        if (handler != null) {
            ++closing;
            handler.post(() -> {
                try {
                    if (oldSession != null) oldSession.close();
                    if (oldCamera != null) oldCamera.close();
                    if (oldReader != null) oldReader.close();
                    if (oldSurface != null) oldSurface.release();
                } finally {
                    oldWorker.quitSafely();
                    runOnUiThread(() -> { --closing; openCamera(); });
                }
            });
        }
        handler = null;
        super.onPause();
    }
    @Override public void onDestroy() {
        if (!isChangingConfigurations()) NativeUi.complete(request, "cancel");
        imageDecoder.shutdownNow();
        super.onDestroy();
    }
    private void error(String message) { runOnUiThread(() -> status.setText(message)); }
    private void found(String invitation) {
        if (invitation == null || invitation.isEmpty()) return;
        runOnUiThread(() -> {
            if (isFinishing() || isDestroyed()) return;
            NativeUi.complete(request, invitation); finish();
        });
    }
    private void openCamera() {
        if (!resumed || isFinishing() || closing != 0 || !preview.isAvailable() || opening || camera != null
                || checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) return;
        try {
            CameraManager manager = getSystemService(CameraManager.class);
            String chosen = null;
            for (String id : manager.getCameraIdList()) {
                if (chosen == null) chosen = id;
                if (Integer.valueOf(CameraCharacteristics.LENS_FACING_BACK).equals(manager.getCameraCharacteristics(id)
                        .get(CameraCharacteristics.LENS_FACING))) { chosen = id; break; }
            }
            if (chosen == null) throw new IllegalStateException("No camera");
            CameraCharacteristics characteristics = manager.getCameraCharacteristics(chosen);
            focusMode = CaptureRequest.CONTROL_AF_MODE_OFF;
            int[] focusModes = characteristics.get(CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES);
            if (focusModes != null) for (int mode : focusModes)
                if (mode == CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE) focusMode = mode;
            StreamConfigurationMap map = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            if (map == null) throw new IllegalStateException("No camera streams");
            Size[] textures = map.getOutputSizes(SurfaceTexture.class);
            size = null;
            for (Size candidate : map.getOutputSizes(ImageFormat.YUV_420_888)) {
                if (candidate.getWidth() > 1280 || candidate.getHeight() > 1280 || !Arrays.asList(textures).contains(candidate)) continue;
                if (size == null || (long)candidate.getWidth() * candidate.getHeight() > (long)size.getWidth() * size.getHeight()) size = candidate;
            }
            if (size == null) throw new IllegalStateException("No supported preview size");
            final int currentGeneration = ++generation;
            preview.getSurfaceTexture().setDefaultBufferSize(size.getWidth(), size.getHeight());
            transform();
            surface = new Surface(preview.getSurfaceTexture());
            reader = ImageReader.newInstance(size.getWidth(), size.getHeight(), ImageFormat.YUV_420_888, 2);
            reader.setOnImageAvailableListener(source -> readFrame(source, currentGeneration), handler);
            opening = true;
            manager.openCamera(chosen, new CameraDevice.StateCallback() {
                @Override public void onOpened(CameraDevice device) {
                    if (currentGeneration != generation) { device.close(); return; }
                    opening = false;
                    if (!resumed || handler == null) { device.close(); return; }
                    camera = device; startPreview();
                }
                @Override public void onDisconnected(CameraDevice device) {
                    device.close();
                    if (currentGeneration == generation) { camera = null; opening = false; }
                }
                @Override public void onError(CameraDevice device, int reason) {
                    device.close();
                    if (currentGeneration != generation) return;
                    camera = null; opening = false;
                    error(text("Camera unavailable. Close other camera apps, then reopen the scanner.", "Камера недоступна. Закройте другие приложения с камерой и откройте сканер снова."));
                }
            }, new Handler(getMainLooper()));
        } catch (Exception e) {
            opening = false;
            android.util.Log.e("LocalClipboard", "Cannot open QR camera", e);
            error(text("Could not open the camera. Check camera access or choose an image.", "Не удалось открыть камеру. Проверьте разрешение или выберите изображение."));
        }
    }
    private void startPreview() {
        try {
            final CameraDevice device = camera;
            final Surface analysis = reader.getSurface();
            final Surface display = surface;
            device.createCaptureSession(Arrays.asList(display, analysis), new CameraCaptureSession.StateCallback() {
                @Override public void onConfigured(CameraCaptureSession configured) {
                    if (!resumed || camera != device) { configured.close(); return; }
                    session = configured;
                    try {
                        CaptureRequest.Builder capture = device.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
                        capture.addTarget(display); capture.addTarget(analysis);
                        capture.set(CaptureRequest.CONTROL_AF_MODE, focusMode);
                        configured.setRepeatingRequest(capture.build(), null, handler);
                    } catch (Exception e) { error(text("Camera preview failed. Reopen the scanner.", "Не удалось запустить предпросмотр. Откройте сканер снова.")); }
                }
                @Override public void onConfigureFailed(CameraCaptureSession configured) {
                    error(text("Camera preview is unavailable. Choose an image instead.", "Предпросмотр камеры недоступен. Выберите изображение."));
                }
            }, new Handler(getMainLooper()));
        } catch (Exception e) { error(text("Camera preview failed.", "Не удалось запустить предпросмотр камеры.")); }
    }
    private void readFrame(ImageReader source, int currentGeneration) {
        try (Image frame = source.acquireLatestImage()) {
            long now = SystemClock.elapsedRealtime();
            if (frame == null || !resumed || generation != currentGeneration || now - lastFrame < 250) return;
            lastFrame = now;
            int width = frame.getWidth(), height = frame.getHeight();
            Image.Plane plane = frame.getPlanes()[0]; ByteBuffer buffer = plane.getBuffer();
            byte[] pixels = new byte[width * height];
            for (int y = 0; y < height; ++y) {
                int row = y * plane.getRowStride();
                if (plane.getPixelStride() == 1) { buffer.position(row); buffer.get(pixels, y * width, width); }
                else for (int x = 0; x < width; ++x) pixels[y * width + x] = buffer.get(row + x * plane.getPixelStride());
            }
            String invitation = decode(pixels, width, height);
            if (invitation != null && !invitation.isEmpty()) runOnUiThread(() -> {
                if (resumed && generation == currentGeneration) found(invitation);
            });
        } catch (IllegalStateException ignored) { // Reader may close while the Activity pauses.
        } catch (Exception e) { android.util.Log.e("LocalClipboard", "Cannot decode QR frame", e); }
    }
    private void transform() {
        if (size == null || preview.getWidth() == 0 || preview.getHeight() == 0) return;
        int rotation = getWindowManager().getDefaultDisplay().getRotation();
        Matrix matrix = new Matrix();
        float width = preview.getWidth(), height = preview.getHeight();
        float bufferWidth = size.getHeight(), bufferHeight = size.getWidth();
        if (rotation == Surface.ROTATION_90 || rotation == Surface.ROTATION_270) {
            bufferWidth = size.getWidth(); bufferHeight = size.getHeight();
        }
        float scale = Math.max(width / bufferWidth, height / bufferHeight);
        matrix.setScale(bufferWidth * scale / width, bufferHeight * scale / height, width / 2, height / 2);
        if (rotation == Surface.ROTATION_90 || rotation == Surface.ROTATION_270)
            matrix.postRotate(rotation == Surface.ROTATION_90 ? -90 : 90, width / 2, height / 2);
        else if (rotation == Surface.ROTATION_180) matrix.postRotate(180, width / 2, height / 2);
        preview.setTransform(matrix);
    }
    @Override protected void onActivityResult(int code, int result, Intent data) {
        super.onActivityResult(code, result, data);
        if (code != 2 || result != RESULT_OK || data == null || data.getData() == null) return;
        if (readingImage) return;
        readingImage = true;
        // The picker resumes this Activity after this callback; use a separate bounded worker.
        imageDecoder.execute(() -> {
            try {
                android.graphics.BitmapFactory.Options options = new android.graphics.BitmapFactory.Options();
                options.inJustDecodeBounds = true;
                try (java.io.InputStream stream = getContentResolver().openInputStream(data.getData())) {
                    android.graphics.BitmapFactory.decodeStream(stream, null, options);
                }
                if (options.outWidth <= 0 || options.outHeight <= 0 || options.outWidth > 8192 || options.outHeight > 8192) throw new IllegalArgumentException("Image too large");
                options.inJustDecodeBounds = false; options.inSampleSize = 1;
                while (options.outWidth / options.inSampleSize > 1280 || options.outHeight / options.inSampleSize > 1280) options.inSampleSize *= 2;
                android.graphics.Bitmap bitmap;
                try (java.io.InputStream stream = getContentResolver().openInputStream(data.getData())) {
                    bitmap = android.graphics.BitmapFactory.decodeStream(stream, null, options);
                }
                if (bitmap == null) throw new IllegalArgumentException("Invalid image");
                int width = bitmap.getWidth(), height = bitmap.getHeight();
                int[] colors = new int[width * height]; bitmap.getPixels(colors, 0, width, 0, 0, width, height); bitmap.recycle();
                byte[] pixels = new byte[colors.length];
                for (int i = 0; i < colors.length; ++i) {
                    int color = colors[i]; pixels[i] = (byte)((((color >> 16) & 255) * 306 + ((color >> 8) & 255) * 601 + (color & 255) * 117) >> 10);
                }
                String invitation = decode(pixels, width, height);
                if (invitation == null || invitation.isEmpty()) error(text("No valid LocalClipboard QR code in this image.", "На изображении нет действительного QR-кода LocalClipboard."));
                else found(invitation);
            } catch (Exception e) { error(text("Cannot read this QR image.", "Не удалось прочитать QR на изображении.")); }
            finally { readingImage = false; }
        });
    }
    @Override public void onSurfaceTextureAvailable(SurfaceTexture texture, int width, int height) { openCamera(); }
    @Override public void onSurfaceTextureSizeChanged(SurfaceTexture texture, int width, int height) { transform(); }
    @Override public boolean onSurfaceTextureDestroyed(SurfaceTexture texture) { return true; }
    @Override public void onSurfaceTextureUpdated(SurfaceTexture texture) { }
}
