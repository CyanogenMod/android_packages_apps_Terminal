/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.terminal;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Paint.FontMetrics;
import android.graphics.Rect;
import android.view.View;

import android.os.SystemClock;
import android.util.Log;

import com.android.terminal.Terminal.TerminalClient;

/**
 * Rendered contents of a {@link Terminal} session.
 */
public class TerminalView extends View {
    private static final String TAG = "Terminal";

    private final Context mContext;
    private final Terminal mTerm;

    private final Paint mBgPaint = new Paint();
    private final Paint mTextPaint = new Paint();

    private int mCharTop;
    private int mCharWidth;
    private int mCharHeight;

    private TerminalClient mClient = new TerminalClient() {
        @Override
        public void damage(int startRow, int endRow, int startCol, int endCol) {
            final int top = startRow * mCharHeight;
            final int bottom = (endRow + 1) * mCharHeight;
            final int left = startCol * mCharWidth;
            final int right = (endCol + 1) * mCharWidth;

            // Invalidate region on screen
            postInvalidate(left, top, right, bottom);
        }

        @Override
        public void bell() {
            Log.i(TAG, "DING!");
        }
    };

    public TerminalView(Context context, Terminal term) {
        super(context);
        mContext = context;
        mTerm = term;

        setBackgroundColor(Color.BLACK);
        setTextSize(20);

        // TODO: remove this test code that triggers invalidates
        setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                v.invalidate();
            }
        });
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mTerm.setClient(mClient);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mTerm.setClient(null);
    }

    public void setTextSize(float textSize) {
        mTextPaint.setTextSize(textSize);

        // Read metrics to get exact pixel dimensions
        final FontMetrics fm = mTextPaint.getFontMetrics();
        mCharTop = (int) Math.ceil(fm.top);

        final float[] widths = new float[1];
        mTextPaint.getTextWidths("X", widths);
        mCharWidth = (int) Math.ceil(widths[0]);
        mCharHeight = (int) Math.ceil(fm.descent - fm.top);

        updateTerminalSize();
    }

    /**
     * Determine terminal dimensions based on current dimensions and font size,
     * and request that {@link Terminal} change to that size.
     */
    public void updateTerminalSize() {
        if (getWidth() > 0 && getHeight() > 0) {
            final int rows = getHeight() / mCharHeight;
            final int cols = getWidth() / mCharWidth;
            mTerm.resize(rows, cols);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (changed) {
            updateTerminalSize();
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        final long start = SystemClock.elapsedRealtime();

        // Only draw dirty region of console
        final Rect dirty = canvas.getClipBounds();

        final int startRow = dirty.top / mCharHeight;
        final int endRow = dirty.bottom / mCharHeight;
        final int startCol = dirty.left / mCharWidth;
        final int endCol = dirty.right / mCharWidth;

        final Terminal.Cell cell = new Terminal.Cell();

        for (int row = startRow; row <= endRow; row++) {
            for (int col = startCol; col <= endCol;) {
                mTerm.getCell(row, col, cell);

                mBgPaint.setColor(cell.bgColor);
                mTextPaint.setColor(cell.fgColor);

                final int y = row * mCharHeight;
                final int x = col * mCharWidth;
                final int xEnd = x + (cell.width * mCharWidth);

                canvas.drawRect(x, y, xEnd, y + mCharHeight, mBgPaint);
                canvas.drawText(cell.chars, 0, cell.chars.length, x, y - mCharTop, mTextPaint);

                col += cell.width;
            }
        }

        final long delta = SystemClock.elapsedRealtime() - start;
        Log.d(TAG, "onDraw() took " + delta + "ms");
    }
}
