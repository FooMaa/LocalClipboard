package org.localclipboard;

import android.content.Context;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/** Only the wrapped ciphertext is persisted; the wrapping key stays in Android Keystore. */
public final class ProfileSecrets {
    private static final String ALIAS = "LocalClipboard.profile.v1";
    private static final byte[] DOMAIN = "LocalClipboard/profile/v2".getBytes(StandardCharsets.US_ASCII);
    private static SecretKey key(boolean create) throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore"); store.load(null);
        if (store.containsAlias(ALIAS)) return (SecretKey)store.getKey(ALIAS, null);
        if (!create) throw new Exception("Missing profile wrapping key");
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256).setRandomizedEncryptionRequired(true).build());
        return generator.generateKey();
    }
    public static synchronized String wrap(Context context, String plain) {
        if (plain == null || !plain.matches("[0-9a-f]{64}")) return "";
        byte[] bytes = plain.getBytes(StandardCharsets.US_ASCII);
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, key(true)); cipher.updateAAD(DOMAIN);
            byte[] encrypted = cipher.doFinal(bytes);
            byte[] output = new byte[12 + encrypted.length];
            if (cipher.getIV().length != 12) return "";
            System.arraycopy(cipher.getIV(), 0, output, 0, 12);
            System.arraycopy(encrypted, 0, output, 12, encrypted.length);
            return "AK1." + Base64.encodeToString(output, Base64.NO_WRAP);
        } catch (Exception e) {
            android.util.Log.e("LocalClipboard", "Cannot protect profile key"); return "";
        } finally { Arrays.fill(bytes, (byte)0); }
    }
    public static synchronized String unwrap(Context context, String stored) {
        if (stored == null || !stored.startsWith("AK1.") || stored.length() != 128) return "";
        byte[] plain = null;
        try {
            byte[] bytes = Base64.decode(stored.substring(4), Base64.NO_WRAP);
            if (bytes.length != 92) return "";
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, key(false), new GCMParameterSpec(128, bytes, 0, 12));
            cipher.updateAAD(DOMAIN); plain = cipher.doFinal(bytes, 12, bytes.length - 12);
            return new String(plain, StandardCharsets.US_ASCII);
        } catch (Exception e) {
            android.util.Log.e("LocalClipboard", "Cannot unlock profile key"); return "";
        } finally { if (plain != null) Arrays.fill(plain, (byte)0); }
    }
    private ProfileSecrets() { }
}
