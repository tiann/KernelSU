/*
 * Copyright 2023 The Android Open Source Project
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

package me.weishu.kernelsu.ui.webui;

import java.net.URLConnection;

class MimeUtil {

    public static String getMimeFromFileName(String fileName) {
        if (fileName == null) {
            return null;
        }

        // Copying the logic and mapping that Chromium follows.
        // First we check against the OS (this is a limited list by default)
        // but app developers can extend this.
        // We then check against a list of hardcoded mime types above if the
        // OS didn't provide a result.
        String mimeType = URLConnection.guessContentTypeFromName(fileName);

        if (mimeType != null) {
            return mimeType;
        }

        return guessHardcodedMime(fileName);
    }

    // We should keep this map in sync with the lists under
    // //net/base/mime_util.cc in Chromium.
    // A bunch of the mime types don't really apply to Android land
    // like word docs so feel free to filter out where necessary.
    private static String guessHardcodedMime(String fileName) {
        int finalFullStop = fileName.lastIndexOf('.');
        if (finalFullStop == -1) {
            return null;
        }

        final String extension = fileName.substring(finalFullStop + 1).toLowerCase();

        return switch (extension) {
            case "webm" -> "video/webm";
            case "mpeg", "mpg" -> "video/mpeg";
            case "mp3" -> "audio/mpeg";
            case "wasm" -> "application/wasm";
            case "xhtml", "xht", "xhtm" -> "application/xhtml+xml";
            case "flac" -> "audio/flac";
            case "ogg", "oga", "opus" -> "audio/ogg";
            case "wav" -> "audio/wav";
            case "m4a" -> "audio/x-m4a";
            case "gif" -> "image/gif";
            case "jpeg", "jpg", "jfif", "pjpeg", "pjp" -> "image/jpeg";
            case "png" -> "image/png";
            case "apng" -> "image/apng";
            case "svg", "svgz" -> "image/svg+xml";
            case "webp" -> "image/webp";
            case "mht", "mhtml" -> "multipart/related";
            case "css" -> "text/css";
            case "html", "htm", "shtml", "shtm", "ehtml" -> "text/html";
            case "js", "mjs" -> "application/javascript";
            case "xml" -> "text/xml";
            case "mp4", "m4v" -> "video/mp4";
            case "ogv", "ogm" -> "video/ogg";
            case "ico" -> "image/x-icon";
            case "woff" -> "application/font-woff";
            case "gz", "tgz" -> "application/gzip";
            case "json" -> "application/json";
            case "pdf" -> "application/pdf";
            case "zip" -> "application/zip";
            case "bmp" -> "image/bmp";
            case "tiff", "tif" -> "image/tiff";
            default -> null;
        };
    }
}
