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

import static com.android.terminal.Terminal.TAG;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcelable;
import android.preference.PreferenceManager;
import android.support.v4.view.PagerAdapter;
import android.support.v4.view.PagerTitleStrip;
import android.support.v4.view.ViewPager;
import android.util.Log;
import android.util.SparseArray;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

/**
 * Activity that displays all {@link Terminal} instances running in a bound
 * {@link TerminalService}.
 */
public class TerminalActivity extends Activity {

    private TerminalService mService;

    private ViewPager mPager;
    private PagerTitleStrip mTitles;

    private final ServiceConnection mServiceConn = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mService = ((TerminalService.ServiceBinder) service).getService();

            final int size = mService.getTerminals().size();
            Log.d(TAG, "Bound to service with " + size + " active terminals");

            // Give ourselves at least one terminal session
            if (size == 0) {
                mService.createTerminal();
            }

            // Bind UI to known terminals
            mTermAdapter.notifyDataSetChanged();
            invalidateOptionsMenu();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            mService = null;
            throw new RuntimeException("Service in same process disconnected?");
        }
    };

    private final PagerAdapter mTermAdapter = new PagerAdapter() {
        private SparseArray<SparseArray<Parcelable>>
                mSavedState = new SparseArray<SparseArray<Parcelable>>();

        @Override
        public int getCount() {
            if (mService != null) {
                return mService.getTerminals().size();
            } else {
                return 0;
            }
        }

        @Override
        public Object instantiateItem(ViewGroup container, int position) {
            final TerminalView view = new TerminalView(container.getContext());
            view.setId(android.R.id.list);

            final Terminal term = mService.getTerminals().valueAt(position);
            view.setTerminal(term);

            final SparseArray<Parcelable> state = mSavedState.get(term.key);
            if (state != null) {
                view.restoreHierarchyState(state);
            }

            container.addView(view);
            view.requestFocus();
            return view;
        }

        @Override
        public void destroyItem(ViewGroup container, int position, Object object) {
            final TerminalView view = (TerminalView) object;

            final int key = view.getTerminal().key;
            SparseArray<Parcelable> state = mSavedState.get(key);
            if (state == null) {
                state = new SparseArray<Parcelable>();
                mSavedState.put(key, state);
            }
            view.saveHierarchyState(state);

            view.setTerminal(null);
            container.removeView(view);
        }

        @Override
        public int getItemPosition(Object object) {
            final TerminalView view = (TerminalView) object;
            final int key = view.getTerminal().key;
            final int index = mService.getTerminals().indexOfKey(key);
            if (index == -1) {
                return POSITION_NONE;
            } else {
                return index;
            }
        }

        @Override
        public boolean isViewFromObject(View view, Object object) {
            return view == object;
        }

        @Override
        public CharSequence getPageTitle(int position) {
            return mService.getTerminals().valueAt(position).getTitle();
        }
    };

    private final View.OnSystemUiVisibilityChangeListener mUiVisibilityChangeListener = new View.OnSystemUiVisibilityChangeListener() {
        @Override
        public void onSystemUiVisibilityChange(int visibility) {
            if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) != 0) {
                getActionBar().hide();
            }
            else {
                getActionBar().show();
            }
        }
    };

    public void updatePreferences() {
        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(this);
        boolean bval;
        bval = sp.getBoolean(TerminalSettingsActivity.KEY_FULLSCREEN_MODE, false);
        if (bval) {
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN);
            getActionBar().hide();
        }
        else {
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
            getActionBar().show();
        }

        String sval;
        sval = sp.getString(TerminalSettingsActivity.KEY_SCREEN_ORIENTATION, "automatic");
        if (sval.equals("automatic")) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR);
        }
        if (sval.equals("portrait")) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        }
        if (sval.equals("landscape")) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        }

        for (int i = 0; i < mPager.getChildCount(); ++i) {
            View v = mPager.getChildAt(i);
            if (v instanceof TerminalView) {
                TerminalView view = (TerminalView) v;
                view.updatePreferences();
                view.invalidateViews();
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity);

        mPager = (ViewPager) findViewById(R.id.pager);
        mTitles = (PagerTitleStrip) findViewById(R.id.titles);

        mPager.setAdapter(mTermAdapter);

        View decorView = getWindow().getDecorView();
        decorView.setOnSystemUiVisibilityChangeListener(mUiVisibilityChangeListener);
    }

    @Override
    protected void onStart() {
        super.onStart();
        updatePreferences();
        bindService(
                new Intent(this, TerminalService.class), mServiceConn, Context.BIND_AUTO_CREATE);
    }

    @Override
    protected void onResume() {
        super.onResume();
        updatePreferences();
    }

    @Override
    protected void onStop() {
        super.onStop();
        unbindService(mServiceConn);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.activity, menu);
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        super.onPrepareOptionsMenu(menu);
        menu.findItem(R.id.menu_close_tab).setEnabled(mTermAdapter.getCount() > 0);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.menu_new_tab: {
                mService.createTerminal();
                mTermAdapter.notifyDataSetChanged();
                invalidateOptionsMenu();
                final int index = mService.getTerminals().size() - 1;
                mPager.setCurrentItem(index, true);
                return true;
            }
            case R.id.menu_close_tab: {
                final int index = mPager.getCurrentItem();
                final int key = mService.getTerminals().keyAt(index);
                mService.destroyTerminal(key);
                mTermAdapter.notifyDataSetChanged();
                invalidateOptionsMenu();
                return true;
            }
            case R.id.menu_item_settings: {
                startActivity(new Intent(TerminalActivity.this, TerminalSettingsActivity.class));
                return true;
            }
        }
        return false;
    }
}
