package com.russia.game.gui.util

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.util.AttributeSet
import androidx.appcompat.widget.AppCompatTextView

class StrokedTextView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : AppCompatTextView(context, attrs, defStyleAttr) {


    var strokeWidthPx: Float = 2f

    override fun onDraw(canvas: Canvas) {
        val paint = paint
        val oldStyle = paint.style
        val oldStrokeWidth = paint.strokeWidth
        paint.style = Paint.Style.STROKE
        paint.strokeWidth = strokeWidthPx

        super.onDraw(canvas)

        paint.style = oldStyle
        paint.strokeWidth = oldStrokeWidth
    }
}
