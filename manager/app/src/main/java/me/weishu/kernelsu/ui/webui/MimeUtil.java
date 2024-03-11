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

        switch (extension) {
            case "webm":
                return "video/webm";
            case "mpeg":
            case "mpg":
                return "video/mpeg";
            case "mp3":
                return "audio/mpeg";
            case "wasm":
                return "application/wasm";
            case "xhtml":
            case "xht":
            case "xhtm":
                return "application/xhtml+xml";
            case "flac":
                return "audio/flac";
            case "ogg":
            case "oga":
            case "opus":
                return "audio/ogg";
            case "wav":
                return "audio/wav";
            case "m4a":
                return "audio/x-m4a";
            case "gif":
                return "image/gif";
            case "jpeg":
            case "jpg":
            case "jfif":
            case "pjpeg":
            case "pjp":
                return "image/jpeg";
            case "png":
                return "image/png";
            case "apng":
                return "image/apng";
            case "svg":
            case "svgz":
                return "image/svg+xml";
            case "webp":
                return "image/webp";
            case "mht":
            case "mhtml":
                return "multipart/related";
            case "css":
                return "text/css";
            case "html":
            case "htm":
            case "shtml":
            case "shtm":
            case "ehtml":
                return "text/html";
            case "js":
            case "mjs":
                return "application/javascript";
            case "xml":
                return "text/xml";
            case "mp4":
            case "m4v":
                return "video/mp4";
            case "ogv":
            case "ogm":
                return "video/ogg";
            case "ico":
                return "image/x-icon";
            case "woff":
                return "application/font-woff";
            case "gz":
            case "tgz":
                return "application/gzip";
            case "json":
                return "application/json";
            case "pdf":
                return "application/pdf";
            case "zip":
                return "application/zip";
            case "bmp":
                return "image/bmp";
            case "tiff":
            case "tif":
                return "image/tiff";
            default:
                return null;
        }
    }
}
