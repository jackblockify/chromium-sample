// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

/** Simple object representing important information of a pdf native page. */
public class PdfInfo {
    public final String filename;
    public final String filepath;

    public PdfInfo(String filename, String filepath) {
        this.filename = filename;
        this.filepath = filepath;
    }

    public PdfInfo() {
        filename = null;
        filepath = null;
    }
}
