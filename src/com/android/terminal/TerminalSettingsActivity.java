/*
 * Copyright (C) 2014 The Android Open Source Project
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

import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;

import static com.android.terminal.Terminal.TAG;

/**
 * Settings for Terminal.
 */
public class TerminalSettingsActivity extends PreferenceActivity
        implements Preference.OnPreferenceChangeListener {

    public static final String KEY_FULLSCREEN_MODE = "fullscreen_mode";
    public static final String KEY_SCREEN_ORIENTATION = "screen_orientation";
    public static final String KEY_FONT_SIZE = "font_size";
    public static final String KEY_TEXT_COLORS = "text_colors";

    private CheckBoxPreference mFullscreenModePref;
    private ListPreference mScreenOrientationPref;
    private ListPreference mFontSizePref;
    private ListPreference mTextColorsPref;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.settings);

        mFullscreenModePref = (CheckBoxPreference) findPreference(KEY_FULLSCREEN_MODE);
        mScreenOrientationPref = (ListPreference) findPreference(KEY_SCREEN_ORIENTATION);
        mScreenOrientationPref.setOnPreferenceChangeListener(this);
        mFontSizePref = (ListPreference) findPreference(KEY_FONT_SIZE);
        mFontSizePref.setOnPreferenceChangeListener(this);
        mTextColorsPref = (ListPreference) findPreference(KEY_TEXT_COLORS);
        mTextColorsPref.setOnPreferenceChangeListener(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen,
            Preference preference) {
        if (KEY_FULLSCREEN_MODE.equals(preference.getKey())) {
            CheckBoxPreference pref = (CheckBoxPreference) preference;
        }
        return super.onPreferenceTreeClick(preferenceScreen, preference);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (KEY_SCREEN_ORIENTATION.equals(preference.getKey())) {
            ListPreference pref = (ListPreference) preference;
            String val = (String) newValue;
            int idx = pref.findIndexOfValue(val);
        }
        if (KEY_FONT_SIZE.equals(preference.getKey())) {
            ListPreference pref = (ListPreference) preference;
            String val = (String) newValue;
            int idx = pref.findIndexOfValue(val);
        }
        if (KEY_TEXT_COLORS.equals(preference.getKey())) {
            ListPreference pref = (ListPreference) preference;
            String val = (String) newValue;
            int idx = pref.findIndexOfValue(val);
        }
        return true;
    }
}
