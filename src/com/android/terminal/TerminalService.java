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

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Background service that keeps {@link Terminal} instances running and warm
 * when UI isn't present.
 */
public class TerminalService extends Service {
    private static final String TAG = "Terminal";

    private final ArrayList<Terminal> mTerminals = new ArrayList<Terminal>();

    public class ServiceBinder extends Binder {
        public TerminalService getService() {
            return TerminalService.this;
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return new ServiceBinder();
    }

    public List<Terminal> getTerminals() {
        return Collections.unmodifiableList(mTerminals);
    }

    public Terminal createTerminal() {
        // If our first terminal, start ourselves as long-lived service
        if (mTerminals.isEmpty()) {
            startService(new Intent(this, TerminalService.class));
        }

        final Terminal term = new Terminal();
        term.start();
        mTerminals.add(term);
        return term;
    }

    public void destroyTerminal(Terminal term) {
        term.stop();
        mTerminals.remove(term);

        // If our last terminal, tear down long-lived service
        if (mTerminals.isEmpty()) {
            stopService(new Intent(this, TerminalService.class));
        }
    }
}
