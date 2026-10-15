package org.localclipboard;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

// Private notification entry point: another app cannot request clipboard writes
// by sending extras to the exported Share/launcher activity.
public final class CopyReceivedActivity extends Activity {
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        ClipboardActivity.requestCopy();
        startActivity(new Intent(this, ClipboardActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP));
        finish();
    }
}
