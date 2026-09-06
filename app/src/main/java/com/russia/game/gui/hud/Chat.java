package com.russia.game.gui.hud;

import static com.russia.game.core.Samp.activity;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.text.Spanned;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.DecelerateInterpolator;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.russia.game.R;
import com.russia.game.gui.util.FadingEdgeLayout;
import com.russia.game.gui.util.Utils;
import com.russia.launcher.storage.Storage;

import org.jetbrains.annotations.NotNull;

import java.io.UnsupportedEncodingException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class Chat {

    native void SendChatButton(int buttonID);
    static native void SendChatMessage(byte str[]);
    native void toggleNativeKeyboard(boolean toggle);
    native void nativeToggleInputState(boolean toggle);
    native void clickHistoryButt(int buttId);

    static EditText chat_input;
    ConstraintLayout chat_input_layout;

    TextView me_button;
    TextView try_button;
    TextView do_button;
    ImageView hide_chat;
    ConstraintLayout chat_box;
    ConstraintLayout chat_binder_butt;

    private final int INVALID = -1;
    private final int ME_BUTTON = 0;
    private final int DO_BUTTON = 1;
    private final int TRY_BUTTON = 2;
    private int chat_button = INVALID;

    private int chatFontSize;
    private int defaultChatFontSize;

    FadingEdgeLayout chatFadeBox;
    private RecyclerView chat;

    int defaultChatHeight;

    // формат времени для чата
    private static final SimpleDateFormat CHAT_TIME_FORMAT =
            new SimpleDateFormat("HH:mm:ss", Locale.getDefault());

    // адаптер для списка сообщений
    ChatAdapter adapter;

    // сообщения: текст + время
    ArrayList<ChatLine> chat_lines = new ArrayList<>();

    // визуальные фишки
    private float chatAlpha = 0.8f;        // прозрачность блока чата
    private boolean isChatShow = true;     // открыт ли чат
    private boolean isChatAnimating = false;
    private String currentChatAction = null;

    private final int scrollThreshold = 10;
    private float startX;
    private float startY;

    public Chat() {

        chat_box = activity.findViewById(R.id.chat_box);

        chat_binder_butt = activity.findViewById(R.id.chat_binder_butt);
        chat_binder_butt.setOnClickListener(view -> {
            new Binder();
            toggleKeyboard(false);
        });

        hide_chat = activity.findViewById(R.id.hide_chat);
        hide_chat.setOnClickListener(view -> {
            // анимация кнопки скрытия/показа
            hide_chat.animate()
                    .scaleX(0.9f).scaleY(0.9f)
                    .setDuration(60)
                    .withEndAction(() ->
                            hide_chat.animate()
                                    .scaleX(1f).scaleY(1f)
                                    .setDuration(100)
                                    .setInterpolator(new DecelerateInterpolator())
                                    .start()
                    ).start();

            if (isChatShow) {
                hideChat(true);
            } else {
                showChat(true);
            }
        });

        me_button = activity.findViewById(R.id.me_button);
        me_button.setOnClickListener(view -> {
            if (chat_button == ME_BUTTON) {
                me_button.setBackgroundTintList(null);
                chat_button = INVALID;
            } else {
                chat_button = ME_BUTTON;
                me_button.setBackgroundTintList(ColorStateList.valueOf(Color.parseColor("#9c27b0")));
                try_button.setBackgroundTintList(null);
                do_button.setBackgroundTintList(null);
            }
            SendChatButton(chat_button);
        });

        try_button = activity.findViewById(R.id.try_button);
        try_button.setOnClickListener(view -> {
            if (chat_button == TRY_BUTTON) {
                try_button.setBackgroundTintList(null);
                chat_button = INVALID;
            } else {
                chat_button = TRY_BUTTON;
                try_button.setBackgroundTintList(ColorStateList.valueOf(Color.parseColor("#087f23")));
                me_button.setBackgroundTintList(null);
                do_button.setBackgroundTintList(null);
            }
            SendChatButton(chat_button);
        });

        do_button = activity.findViewById(R.id.do_button);
        do_button.setOnClickListener(view -> {
            if (chat_button == DO_BUTTON) {
                do_button.setBackgroundTintList(null);
                chat_button = INVALID;
            } else {
                chat_button = DO_BUTTON;
                do_button.setBackgroundTintList(ColorStateList.valueOf(Color.parseColor("#c67100")));
                try_button.setBackgroundTintList(null);
                me_button.setBackgroundTintList(null);
            }
            SendChatButton(chat_button);
        });

        chat_input_layout = activity.findViewById(R.id.chat_input_layout);
        chat_input_layout.setVisibility(View.GONE);
        chat_input = activity.findViewById(R.id.chat_input);
        chat_input.setShowSoftInputOnFocus(false);

        chat_input.setOnEditorActionListener((TextView v, int actionId, KeyEvent event) -> {
            if (actionId == EditorInfo.IME_ACTION_SEND) {
                try {
                    SendChatMessage(chat_input.getText().toString().getBytes("windows-1251"));
                } catch (UnsupportedEncodingException e) {
                    e.printStackTrace();
                }

                toggleKeyboard(false);
                return true;
            }
            return false;
        });

        // чуть меньше дефолтный шрифт, чтобы больше строк влезало
        defaultChatFontSize = 15;
        chatFontSize = defaultChatFontSize;

        chat = activity.findViewById(R.id.chat);
        chatFadeBox = activity.findViewById(R.id.chat_fade_box);

        // высота чата
        Storage.setInt("defaultChatHeight", chatFadeBox.getMinimumHeight());
        int height = Storage.getInt("chatHeight");
        if (height > 100) {
            ConstraintLayout.LayoutParams layoutParams =
                    (ConstraintLayout.LayoutParams) chatFadeBox.getLayoutParams();
            layoutParams.height = height;
            chatFadeBox.setLayoutParams(layoutParams);
        }

        // прозрачность чата (можно завязать на настройки)
        int storedAlpha = Storage.getInt("chatAlpha");
        if (storedAlpha > 0 && storedAlpha <= 10) {
            chatAlpha = storedAlpha / 10f;
        } else {
            chatAlpha = 0.8f;
        }
        chat_box.setAlpha(chatAlpha);

        LinearLayoutManager mLayoutManager = new LinearLayoutManager(activity);
        mLayoutManager.setStackFromEnd(true);
        chat.setLayoutManager(mLayoutManager);
        chat.setVerticalScrollBarEnabled(false);

        // тап по области чата — показать/спрятать инпут (как в новом чате)
        chat.setOnTouchListener((v, event) -> {
            int action = event.getAction();
            if (action == MotionEvent.ACTION_DOWN) {
                startX = event.getX();
                startY = event.getY();
                return false;
            }
            if (action == MotionEvent.ACTION_UP) {
                if (Math.abs(event.getY() - startY) <= scrollThreshold) {
                    boolean show = chat_input_layout.getVisibility() != View.VISIBLE;
                    toggleChatInput(show);
                    return true;
                }
            }
            return false;
        });

        // адаптер
        adapter = new ChatAdapter(activity, chat_lines);
        chat.setAdapter(adapter);

        ConstraintLayout chat_up_butt = activity.findViewById(R.id.chat_up_butt);
        chat_up_butt.setOnClickListener(view -> clickHistoryButt(1));

        ConstraintLayout chat_down_butt = activity.findViewById(R.id.chat_down_butt);
        chat_down_butt.setOnClickListener(view -> clickHistoryButt(0));
    }

    /** Модель сообщения чата: текст + время */
    public static class ChatLine {
        public final Spanned text;
        public final String time;

        public ChatLine(Spanned text, String time) {
            this.text = text;
            this.time = time;
        }
    }

    /*** Плавное скрытие чата ***/
    void hideChat() {
        hideChat(false);
    }

    void hideChat(boolean fromButton) {
        activity.runOnUiThread(() -> {
            if ("show".equals(currentChatAction)) {
                currentChatAction = "hide";
            } else if (isChatAnimating) {
                return;
            }

            currentChatAction = "hide";
            isChatAnimating = true;
            isChatShow = false;

            chat_box.animate().cancel();
            hide_chat.animate().cancel();

            ObjectAnimator.ofFloat(hide_chat, "rotation",
                            hide_chat.getRotation(), 180f)
                    .setDuration(150)
                    .start();

            if (fromButton) {
                float targetY = -chat_box.getHeight();
                float currentY = chat_box.getTranslationY();
                long duration = currentY <= targetY / 2f ? 80L : 150L;

                chat_box.animate()
                        .translationY(targetY)
                        .alpha(0f)
                        .setDuration(duration)
                        .setInterpolator(new DecelerateInterpolator())
                        .withEndAction(() -> {
                            if ("hide".equals(currentChatAction)) {
                                chat_box.setVisibility(View.GONE);
                                isChatAnimating = false;
                                currentChatAction = null;
                            }
                        })
                        .start();
            } else {
                chat_box.animate()
                        .alpha(0f)
                        .setDuration(150)
                        .setInterpolator(new DecelerateInterpolator())
                        .withEndAction(() -> {
                            if ("hide".equals(currentChatAction)) {
                                chat_box.setVisibility(View.GONE);
                                isChatAnimating = false;
                                currentChatAction = null;
                            }
                        })
                        .start();
            }
        });
    }

    /*** Плавный показ чата ***/
    void showChat() {
        showChat(false);
    }

    void showChat(boolean fromButton) {
        activity.runOnUiThread(() -> {
            if ("hide".equals(currentChatAction)) {
                currentChatAction = "show";
            } else if (isChatAnimating) {
                return;
            }

            currentChatAction = "show";
            isChatAnimating = true;
            isChatShow = true;

            chat_box.setVisibility(View.VISIBLE);
            chat_box.animate().cancel();
            hide_chat.animate().cancel();

            ObjectAnimator.ofFloat(hide_chat, "rotation",
                            hide_chat.getRotation(), 0f)
                    .setDuration(150)
                    .start();

            if (fromButton) {
                float currentY = chat_box.getTranslationY();
                long duration = currentY >= 0f ? 80L : 150L;

                chat_box.animate()
                        .translationY(0f)
                        .alpha(1f)
                        .setDuration(duration)
                        .setInterpolator(new DecelerateInterpolator())
                        .withEndAction(() -> {
                            if ("show".equals(currentChatAction)) {
                                isChatAnimating = false;
                                currentChatAction = null;
                            }
                        })
                        .start();
            } else {
                chat_box.setTranslationY(0f);
                chat_box.setAlpha(0f);
                chat_box.animate()
                        .alpha(1f)
                        .setDuration(150)
                        .setInterpolator(new DecelerateInterpolator())
                        .withEndAction(() -> {
                            if ("show".equals(currentChatAction)) {
                                isChatAnimating = false;
                                currentChatAction = null;
                            }
                        })
                        .start();
            }
        });
    }

    /*** Публичный тумблер чата ***/
    public void ToggleChat(boolean toggle) {
        activity.runOnUiThread(() -> {
            if (toggle) {
                showChat(false);
            } else {
                hideChat(false);
            }
        });
    }

    /** Добавление сообщения (вызывается из нативки) */
    public void AddChatMessage(String msg) {
        adapter.addItem(msg);
    }

    public void ChangeChatFontSize(int size) {
        activity.runOnUiThread(() -> {
            if (size == -1) {
                chatFontSize = defaultChatFontSize;
            } else {
                chatFontSize = size;
            }
            adapter = new ChatAdapter(activity, adapter.getItems());
            chat.setAdapter(adapter);
        });
    }

    public void AddToChatInput(String msg) {
        activity.runOnUiThread(() -> {
            chat_input.setText(msg);
            int len = chat_input.getText().length();
            if (len >= 0) chat_input.setSelection(len);
        });
    }

    public void ToggleChatInput(boolean toggle) {
        toggleChatInput(toggle);
    }

    /** Плавное появление/скрытие инпута и кнопок **/
    private void toggleChatInput(boolean toggle) {
        activity.runOnUiThread(() -> {
            ConstraintLayout chat_up_butt = activity.findViewById(R.id.chat_up_butt);
            ConstraintLayout chat_down_butt = activity.findViewById(R.id.chat_down_butt);
            ConstraintLayout binderButton = chat_binder_butt;

            chat_input.animate().cancel();
            chat_up_butt.animate().cancel();
            chat_down_butt.animate().cancel();
            binderButton.animate().cancel();
            chat_box.animate().cancel();

            float alphaInput = chat_input.getAlpha();
            float alphaUp = chat_up_butt.getAlpha();
            float alphaDown = chat_down_butt.getAlpha();
            float alphaBinder = binderButton.getAlpha();
            float alphaBox = chat_box.getAlpha();

            if (toggle) {
                chat.setVerticalScrollBarEnabled(true);
                chat_input_layout.setVisibility(View.VISIBLE);

                long base = 180L;
                chat_input.animate().alpha(1f).setDuration((long) ((1f - alphaInput) * base)).start();
                chat_up_butt.animate().alpha(1f).setDuration((long) ((1f - alphaUp) * base)).start();
                chat_down_butt.animate().alpha(1f).setDuration((long) ((1f - alphaDown) * base)).start();
                binderButton.animate().alpha(1f).setDuration((long) ((1f - alphaBinder) * base)).start();

                chat_box.animate()
                        .alpha(1f)
                        .setDuration((long) (200 * (1f - alphaBox)))
                        .setInterpolator(new DecelerateInterpolator())
                        .start();

                chat_input.requestFocus();
            } else {
                chat.setVerticalScrollBarEnabled(false);

                long base = 180L;
                chat_input.animate()
                        .alpha(0f)
                        .setDuration((long) (alphaInput * base))
                        .withEndAction(() -> {
                            if (chat_input.getAlpha() == 0f) {
                                chat_input_layout.setVisibility(View.GONE);
                                chat_input.clearFocus();
                                chat_input.getText().clear();
                            }
                        }).start();

                chat_up_butt.animate().alpha(0f).setDuration((long) (alphaUp * base)).start();
                chat_down_butt.animate().alpha(0f).setDuration((long) (alphaDown * base)).start();
                binderButton.animate().alpha(0f).setDuration((long) (alphaBinder * base)).start();

                chat_box.animate()
                        .alpha(chatAlpha)
                        .setDuration((long) (200 * alphaBox))
                        .setInterpolator(new DecelerateInterpolator())
                        .start();
            }
        });
    }

    void toggleKeyboard(boolean toggle) {
        ToggleChatInput(toggle);

        if (Storage.getBoolean("isAndroidKeyboard")) {
            nativeToggleInputState(toggle);
            // android клава
            chat_input.requestFocus();

            InputMethodManager imm =
                    (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);

            if (toggle)
                imm.showSoftInput(chat_input, InputMethodManager.SHOW_IMPLICIT);
            else
                imm.hideSoftInputFromWindow(chat_input.getWindowToken(), 0);
        } else {
            // нативная
            toggleNativeKeyboard(toggle);
        }
    }

    public void ClickChatj() {
        activity.runOnUiThread(() -> {
            if (chat_input_layout.getVisibility() == View.VISIBLE) {
                toggleKeyboard(false);
            } else {
                toggleKeyboard(true);
            }
        });
    }

    /*** Адаптер под твою разметку chatline.xml с временем и анимациями ***/
    public class ChatAdapter extends RecyclerView.Adapter<ChatAdapter.ViewHolder> {

        private final LayoutInflater inflater;
        private final List<ChatLine> chat_lines;

        ChatAdapter(Context context, List<ChatLine> chat_lines) {
            this.chat_lines = chat_lines;
            this.inflater = LayoutInflater.from(context);
        }

        @NotNull
        @Override
        public ChatAdapter.ViewHolder onCreateViewHolder(@NotNull ViewGroup parent, int viewType) {
            View view = inflater.inflate(R.layout.chatline, parent, false);
            view.setOnClickListener(view1 -> ClickChatj());
            // долгий тап — можно потом сделать копирование текста в буфер
            view.setOnLongClickListener(v -> {
                // TODO: скопировать в буфер, показать небольшое Toast
                return true;
            });
            return new ViewHolder(view);
        }

        @Override
        public void onBindViewHolder(ChatAdapter.ViewHolder holder, int position) {
            ChatLine line = chat_lines.get(position);

            holder.text.setTextSize(TypedValue.COMPLEX_UNIT_PX, chatFontSize);
            holder.text.setText(line.text);

            // время отправки
            holder.time.setText(line.time);
        }

        @Override
        public void onViewAttachedToWindow(@NotNull ViewHolder holder) {
            super.onViewAttachedToWindow(holder);
            // лёгкая анимация появления строки
            View itemView = holder.itemView;
            itemView.setAlpha(0f);
            itemView.setTranslationY(10f);
            itemView.animate()
                    .alpha(1f)
                    .translationY(0f)
                    .setDuration(160)
                    .setInterpolator(new DecelerateInterpolator())
                    .start();
        }

        @Override
        public int getItemCount() {
            return chat_lines.size();
        }

        public List<ChatLine> getItems() {
            return chat_lines;
        }

        public class ViewHolder extends RecyclerView.ViewHolder {

            final TextView text;
            final TextView time;

            ViewHolder(View itemView) {
                super(itemView);
                text = itemView.findViewById(R.id.text);
                time = itemView.findViewById(R.id.time);
            }
        }

        public void addItem(String item) {
            activity.runOnUiThread(() -> {
                if (this.chat_lines.size() > 40) {
                    this.chat_lines.remove(0);
                    notifyItemRemoved(0);
                }

                // раскрашенный текст
                Spanned spanned = Utils.transfromColors(item);
                // время сообщения
                String timeStr = CHAT_TIME_FORMAT.format(new Date());

                this.chat_lines.add(new ChatLine(spanned, timeStr));
                int lastIndex = this.chat_lines.size() - 1;
                notifyItemInserted(lastIndex);

                if (chat.getScrollState() == RecyclerView.SCROLL_STATE_IDLE) {
                    chat.scrollToPosition(lastIndex);
                }
            });
        }
    }

}
