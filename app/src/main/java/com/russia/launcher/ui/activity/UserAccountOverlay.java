//////////////////////////////////////////
//  CROSS SYSTEM
//  Author: Cross
//  Telegram: https://t.me/taskJson
//  Date: 23.11.2025
//  Private Development
//////////////////////////////////////////

package com.russia.launcher.ui.activity;

import android.app.Activity;
import android.graphics.RenderEffect;
import android.graphics.Shader;
import android.os.Build;
import android.view.View;
import android.view.animation.DecelerateInterpolator;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.constraintlayout.widget.ConstraintLayout;

import com.russia.game.R;
import com.russia.launcher.storage.NativeStorage;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.List;

public class UserAccountOverlay {

    private final Activity activity;
    private final ConstraintLayout root;
    private final View blurTarget;

    private ImageView backButton;
    private EditText nicknameValue;
    private TextView saveNicknameButton;
    private TextView exitAccountButton;
    private ConstraintLayout nicknameLayout;
    private ConstraintLayout linkedContainer;

    private static final Charset SETTINGS_CHARSET = Charset.forName("windows-1251");

    public UserAccountOverlay(Activity activity, ConstraintLayout root, View blurTarget) {
        this.activity = activity;
        this.root = root;
        this.blurTarget = blurTarget;

        bindViews();
        setupListeners();
        loadNickname();
    }

    private void bindViews() {
        backButton = root.findViewById(R.id.backButton);
        nicknameValue = root.findViewById(R.id.nicknameValue);
        saveNicknameButton = root.findViewById(R.id.saveNicknameButton);
        exitAccountButton = root.findViewById(R.id.exitAccountButton);
        nicknameLayout = root.findViewById(R.id.nicknameLayout);
        linkedContainer = root.findViewById(R.id.linkedContainer);
    }

    private void setupListeners() {
        backButton.setOnClickListener(v -> hide());

        root.setOnClickListener(v -> {});

        View.OnClickListener focusNick = v -> {
            nicknameValue.requestFocus();
            nicknameValue.setSelection(nicknameValue.getText().length());
        };
        nicknameLayout.setOnClickListener(focusNick);
        nicknameValue.setOnClickListener(focusNick);

        saveNicknameButton.setOnClickListener(v -> {
            String newNick = nicknameValue.getText().toString().trim();
            if (newNick.isEmpty()) {
                Toast.makeText(activity, "Ник не может быть пустым", Toast.LENGTH_SHORT).show();
                return;
            }

            boolean ok = saveNicknameToSettings(newNick);
            if (ok) {
                NativeStorage.addClientProperty("name", newNick, activity);

                if (activity instanceof MainActivity) {
                    ((MainActivity) activity).onNicknameChanged(newNick);
                }

                Toast.makeText(activity, "Никнейм сохранён", Toast.LENGTH_SHORT).show();
                hide();
            } else {
                Toast.makeText(activity, "Не удалось изменить ник (settings.ini повреждён)", Toast.LENGTH_SHORT).show();
            }
        });

        exitAccountButton.setOnClickListener(v ->
                Toast.makeText(activity, "Выход из аккаунта ещё не реализован", Toast.LENGTH_SHORT).show()
        );
    }

    public void show() {
        if (blurTarget != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            blurTarget.setRenderEffect(RenderEffect.createBlurEffect(25f, 25f, Shader.TileMode.CLAMP));
        }

        root.setVisibility(View.VISIBLE);
        root.setAlpha(0f);
        root.animate()
                .alpha(1f)
                .setDuration(150)
                .setInterpolator(new DecelerateInterpolator())
                .start();

        if (linkedContainer != null) {
            linkedContainer.setScaleX(0.95f);
            linkedContainer.setScaleY(0.95f);
            linkedContainer.setAlpha(0f);
            linkedContainer.animate()
                    .alpha(1f)
                    .scaleX(1f)
                    .scaleY(1f)
                    .setDuration(180)
                    .setInterpolator(new DecelerateInterpolator())
                    .start();
        }
    }

    public void hide() {
        if (linkedContainer != null) {
            linkedContainer.animate()
                    .alpha(0f)
                    .scaleX(0.97f)
                    .scaleY(0.97f)
                    .setDuration(130)
                    .setInterpolator(new DecelerateInterpolator())
                    .start();
        }

        root.animate()
                .alpha(0f)
                .setDuration(150)
                .setInterpolator(new DecelerateInterpolator())
                .withEndAction(() -> {
                    root.setVisibility(View.GONE);
                    if (blurTarget != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                        blurTarget.setRenderEffect(null);
                    }
                })
                .start();
    }

    private void loadNickname() {
        String nickname = readNicknameFromSettings();
        if (nickname == null || nickname.isEmpty()) {
            String nativeNick = NativeStorage.getClientProperty("name", activity);
            if (nativeNick != null) {
                nickname = nativeNick;
            }
        }
        if (nickname == null) nickname = "";
        nicknameValue.setText(nickname);
        nicknameValue.setSelection(nickname.length());
    }

    private File getSettingsFile() {
        File extDir = activity.getExternalFilesDir(null);
        if (extDir == null) return null;
        return new File(extDir, "SAMP/settings.ini");
    }

    private String readNicknameFromSettings() {
        try {
            File settingsFile = getSettingsFile();
            if (settingsFile == null || !settingsFile.exists()) {
                return "";
            }

            boolean inClientSection = false;
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(new FileInputStream(settingsFile), SETTINGS_CHARSET))) {

                String line;
                while ((line = reader.readLine()) != null) {
                    String trimmed = line.trim();

                    if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                        inClientSection = "[client]".equalsIgnoreCase(trimmed);
                        continue;
                    }

                    if (inClientSection && trimmed.toLowerCase().startsWith("name")) {
                        int idx = trimmed.indexOf('=');
                        if (idx != -1 && idx + 1 < trimmed.length()) {
                            return trimmed.substring(idx + 1).trim();
                        }
                    }
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "";
    }

    private boolean saveNicknameToSettings(String newNick) {
        try {
            File settingsFile = getSettingsFile();
            if (settingsFile == null || !settingsFile.exists()) {
                return true;
            }

            List<String> lines = new ArrayList<>();
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(new FileInputStream(settingsFile), SETTINGS_CHARSET))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    lines.add(line);
                }
            }

            boolean inClientSection = false;
            boolean nameUpdated = false;
            for (int i = 0; i < lines.size(); i++) {
                String original = lines.get(i);
                String trimmed = original.trim();

                if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                    inClientSection = "[client]".equalsIgnoreCase(trimmed);
                    continue;
                }

                if (inClientSection && trimmed.toLowerCase().startsWith("name")) {
                    lines.set(i, "name = " + newNick);
                    nameUpdated = true;
                    break;
                }
            }

            if (!nameUpdated) {
                boolean clientSectionFound = false;
                for (int i = 0; i < lines.size(); i++) {
                    String trimmed = lines.get(i).trim();
                    if ("[client]".equalsIgnoreCase(trimmed)) {
                        clientSectionFound = true;
                        lines.add(i + 1, "name = " + newNick);
                        break;
                    }
                }

                if (!clientSectionFound) {
                    return false;
                }
            }

            writeLines(settingsFile, lines);
            return true;

        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
    }

    private void writeLines(File file, List<String> lines) throws Exception {
        try (BufferedWriter writer = new BufferedWriter(
                new OutputStreamWriter(new FileOutputStream(file, false), SETTINGS_CHARSET))) {
            for (int i = 0; i < lines.size(); i++) {
                writer.write(lines.get(i));
                if (i < lines.size() - 1) {
                    writer.newLine();
                }
            }
            writer.flush();
        }
    }
}
