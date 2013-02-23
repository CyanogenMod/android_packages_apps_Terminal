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
import android.graphics.Typeface;
import android.os.SystemClock;
import android.util.Log;
import android.view.View;

import com.android.terminal.Terminal.CellRun;
import com.android.terminal.Terminal.TerminalClient;

/**
 * Rendered contents of a {@link Terminal} session.
 */
public class TerminalView extends View {
    private static final String TAG = "Terminal";

    private static final int MAX_RUN_LENGTH = 128;

    private final Context mContext;
    private final Terminal mTerm;

    private final Paint mBgPaint = new Paint();
    private final Paint mTextPaint = new Paint();

    /** Run of cells used when drawing */
    private final CellRun mRun;
    /** Screen coordinates to draw chars into */
    private final float[] mPos;

    private int mCharTop;
    private int mCharWidth;
    private int mCharHeight;

    // TODO: for atomicity we might need to snapshot runs when processing
    // callbacks driven by vterm thread

    private TerminalClient mClient = new TerminalClient() {
        @Override
        public void damage(int startRow, int endRow, int startCol, int endCol) {
            Log.d(TAG, "damage(" + startRow + ", " + endRow + ", " + startCol + ", " + endCol + ")");

            // Invalidate region on screen
            final int top = startRow * mCharHeight;
            final int bottom = (endRow + 1) * mCharHeight;
            final int left = startCol * mCharWidth;
            final int right = (endCol + 1) * mCharWidth;
            postInvalidate(left, top, right, bottom);
        }

        @Override
        public void moveRect(int destStartRow, int destEndRow, int destStartCol, int destEndCol,
                int srcStartRow, int srcEndRow, int srcStartCol, int srcEndCol) {
            // Treat as normal damage and perform full redraw
            final int startRow = Math.min(destStartRow, srcStartRow);
            final int endRow = Math.max(destEndRow, srcEndRow);
            final int startCol = Math.min(destStartCol, srcStartCol);
            final int endCol = Math.max(destEndCol, srcEndCol);
            damage(startRow, endRow, startCol, endCol);
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

        mRun = new Terminal.CellRun();
        mRun.data = new char[MAX_RUN_LENGTH];

        // Positions of each possible cell
        // TODO: make sure this works with surrogate pairs
        mPos = new float[MAX_RUN_LENGTH * 2];

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
        mTextPaint.setTypeface(Typeface.MONOSPACE);
        mTextPaint.setTextSize(textSize);

        // Read metrics to get exact pixel dimensions
        final FontMetrics fm = mTextPaint.getFontMetrics();
        mCharTop = (int) Math.ceil(fm.top);

        final float[] widths = new float[1];
        mTextPaint.getTextWidths("X", widths);
        mCharWidth = (int) Math.ceil(widths[0]);
        mCharHeight = (int) Math.ceil(fm.descent - fm.top);

        // Update drawing positions
        for (int i = 0; i < MAX_RUN_LENGTH; i++) {
            mPos[i * 2] = i * mCharWidth;
            mPos[(i * 2) + 1] = -mCharTop;
        }

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
            mTerm.flushDamage();
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

        final int rows = mTerm.getRows();
        final int cols = mTerm.getCols();

        final int startRow = dirty.top / mCharHeight;
        final int endRow = Math.min(dirty.bottom / mCharHeight, rows - 1);
        final int startCol = dirty.left / mCharWidth;
        final int endCol = Math.min(dirty.right / mCharWidth, cols - 1);

        final CellRun run = mRun;
        final float[] pos = mPos;

        for (int row = startRow; row <= endRow; row++) {
            for (int col = startCol; col <= endCol;) {
                mTerm.getCellRun(row, col, run);

                mBgPaint.setColor(run.bgColor);
                mTextPaint.setColor(run.fgColor);

                final int y = row * mCharHeight;
                final int x = col * mCharWidth;
                final int xEnd = x + (run.colSize * mCharWidth);

                canvas.save(Canvas.MATRIX_SAVE_FLAG | Canvas.CLIP_SAVE_FLAG);
                canvas.translate(x, y);
                canvas.clipRect(0, 0, run.colSize * mCharWidth, mCharHeight);

                canvas.drawPaint(mBgPaint);
                canvas.drawPosText(run.data, 0, run.dataSize, pos, mTextPaint);

                canvas.restore();

                col += run.colSize;
            }
        }

        final long delta = SystemClock.elapsedRealtime() - start;
        Log.d(TAG, "onDraw() took " + delta + "ms");
    }
}
