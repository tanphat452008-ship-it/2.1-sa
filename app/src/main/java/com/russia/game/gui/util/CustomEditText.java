package com.russia.game.gui.util;

import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;

public class CustomEditText extends androidx.appcompat.widget.AppCompatEditText {

    public CustomEditText(Context context) {
        super(context);
        init();
    }

    public CustomEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public CustomEditText(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        // Установка цвета текста по умолчанию
        setTextColor(Color.BLUE);

        // Другие инициализации при создании вашего кастомного EditText
    }

    // Вы также можете переопределить другие методы, например, для установки текста или поведения
}
