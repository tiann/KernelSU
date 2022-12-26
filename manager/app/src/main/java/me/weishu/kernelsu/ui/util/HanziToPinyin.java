package me.weishu.kernelsu.ui.util;
/*
 * Copyright (C) 2009 The Android Open Source Project
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

import android.text.TextUtils;
import android.util.Log;

import java.text.Collator;
import java.util.ArrayList;
import java.util.Locale;

/**
 * An object to convert Chinese character to its corresponding pinyin string. For characters with
 * multiple possible pinyin string, only one is selected according to collator. Polyphone is not
 * supported in this implementation. This class is implemented to achieve the best runtime
 * performance and minimum runtime resources with tolerable sacrifice of accuracy. This
 * implementation highly depends on zh_CN ICU collation data and must be always synchronized with
 * ICU.
 * <p>
 * Currently this file is aligned to zh.txt in ICU 4.6
 */
public class HanziToPinyin {
    private static final String TAG = "HanziToPinyin";

    // Turn on this flag when we want to check internal data structure.
    private static final boolean DEBUG = false;

    /**
     * Unihans array.
     * <p>
     * Each unihans is the first one within same pinyin when collator is zh_CN.
     */
    public static final char[] UNIHANS = {
            '\u963f', '\u54ce', '\u5b89', '\u80ae', '\u51f9', '\u516b',
            '\u6300', '\u6273', '\u90a6', '\u52f9', '\u9642', '\u5954',
            '\u4f3b', '\u5c44', '\u8fb9', '\u706c', '\u618b', '\u6c43',
            '\u51ab', '\u7676', '\u5cec', '\u5693', '\u5072', '\u53c2',
            '\u4ed3', '\u64a1', '\u518a', '\u5d7e', '\u66fd', '\u66fe',
            '\u5c64', '\u53c9', '\u8286', '\u8fbf', '\u4f25', '\u6284',
            '\u8f66', '\u62bb', '\u6c88', '\u6c89', '\u9637', '\u5403',
            '\u5145', '\u62bd', '\u51fa', '\u6b3b', '\u63e3', '\u5ddb',
            '\u5205', '\u5439', '\u65fe', '\u9034', '\u5472', '\u5306',
            '\u51d1', '\u7c97', '\u6c46', '\u5d14', '\u90a8', '\u6413',
            '\u5491', '\u5446', '\u4e39', '\u5f53', '\u5200', '\u561a',
            '\u6265', '\u706f', '\u6c10', '\u55f2', '\u7538', '\u5201',
            '\u7239', '\u4e01', '\u4e1f', '\u4e1c', '\u543a', '\u53be',
            '\u8011', '\u8968', '\u5428', '\u591a', '\u59b8', '\u8bf6',
            '\u5940', '\u97a5', '\u513f', '\u53d1', '\u5e06', '\u531a',
            '\u98de', '\u5206', '\u4e30', '\u8985', '\u4ecf', '\u7d11',
            '\u4f15', '\u65ee', '\u4f85', '\u7518', '\u5188', '\u768b',
            '\u6208', '\u7ed9', '\u6839', '\u522f', '\u5de5', '\u52fe',
            '\u4f30', '\u74dc', '\u4e56', '\u5173', '\u5149', '\u5f52',
            '\u4e28', '\u5459', '\u54c8', '\u548d', '\u4f44', '\u592f',
            '\u8320', '\u8bc3', '\u9ed2', '\u62eb', '\u4ea8', '\u5677',
            '\u53ff', '\u9f41', '\u4e6f', '\u82b1', '\u6000', '\u72bf',
            '\u5ddf', '\u7070', '\u660f', '\u5419', '\u4e0c', '\u52a0',
            '\u620b', '\u6c5f', '\u827d', '\u9636', '\u5dfe', '\u5755',
            '\u5182', '\u4e29', '\u51e5', '\u59e2', '\u5658', '\u519b',
            '\u5494', '\u5f00', '\u520a', '\u5ffc', '\u5c3b', '\u533c',
            '\u808e', '\u52a5', '\u7a7a', '\u62a0', '\u625d', '\u5938',
            '\u84af', '\u5bbd', '\u5321', '\u4e8f', '\u5764', '\u6269',
            '\u5783', '\u6765', '\u5170', '\u5577', '\u635e', '\u808b',
            '\u52d2', '\u5d1a', '\u5215', '\u4fe9', '\u5941', '\u826f',
            '\u64a9', '\u5217', '\u62ce', '\u5222', '\u6e9c', '\u56d6',
            '\u9f99', '\u779c', '\u565c', '\u5a08', '\u7567', '\u62a1',
            '\u7f57', '\u5463', '\u5988', '\u57cb', '\u5ada', '\u7264',
            '\u732b', '\u4e48', '\u5445', '\u95e8', '\u753f', '\u54aa',
            '\u5b80', '\u55b5', '\u4e5c', '\u6c11', '\u540d', '\u8c2c',
            '\u6478', '\u54de', '\u6bea', '\u55ef', '\u62cf', '\u8149',
            '\u56e1', '\u56d4', '\u5b6c', '\u7592', '\u5a1e', '\u6041',
            '\u80fd', '\u59ae', '\u62c8', '\u5b22', '\u9e1f', '\u634f',
            '\u56dc', '\u5b81', '\u599e', '\u519c', '\u7fba', '\u5974',
            '\u597b', '\u759f', '\u9ec1', '\u90cd', '\u5594', '\u8bb4',
            '\u5991', '\u62cd', '\u7705', '\u4e53', '\u629b', '\u5478',
            '\u55b7', '\u5309', '\u4e15', '\u56e8', '\u527d', '\u6c15',
            '\u59d8', '\u4e52', '\u948b', '\u5256', '\u4ec6', '\u4e03',
            '\u6390', '\u5343', '\u545b', '\u6084', '\u767f', '\u4eb2',
            '\u72c5', '\u828e', '\u4e18', '\u533a', '\u5cd1', '\u7f3a',
            '\u590b', '\u5465', '\u7a63', '\u5a06', '\u60f9', '\u4eba',
            '\u6254', '\u65e5', '\u8338', '\u53b9', '\u909a', '\u633c',
            '\u5827', '\u5a51', '\u77a4', '\u637c', '\u4ee8', '\u6be2',
            '\u4e09', '\u6852', '\u63bb', '\u95aa', '\u68ee', '\u50e7',
            '\u6740', '\u7b5b', '\u5c71', '\u4f24', '\u5f30', '\u5962',
            '\u7533', '\u8398', '\u6552', '\u5347', '\u5c38', '\u53ce',
            '\u4e66', '\u5237', '\u8870', '\u95e9', '\u53cc', '\u8c01',
            '\u542e', '\u8bf4', '\u53b6', '\u5fea', '\u635c', '\u82cf',
            '\u72fb', '\u590a', '\u5b59', '\u5506', '\u4ed6', '\u56fc',
            '\u574d', '\u6c64', '\u5932', '\u5fd1', '\u71a5', '\u5254',
            '\u5929', '\u65eb', '\u5e16', '\u5385', '\u56f2', '\u5077',
            '\u51f8', '\u6e4d', '\u63a8', '\u541e', '\u4e47', '\u7a75',
            '\u6b6a', '\u5f2f', '\u5c23', '\u5371', '\u6637', '\u7fc1',
            '\u631d', '\u4e4c', '\u5915', '\u8672', '\u4eda', '\u4e61',
            '\u7071', '\u4e9b', '\u5fc3', '\u661f', '\u51f6', '\u4f11',
            '\u5401', '\u5405', '\u524a', '\u5743', '\u4e2b', '\u6079',
            '\u592e', '\u5e7a', '\u503b', '\u4e00', '\u56d9', '\u5e94',
            '\u54df', '\u4f63', '\u4f18', '\u625c', '\u56e6', '\u66f0',
            '\u6655', '\u7b60', '\u7b7c', '\u5e00', '\u707d', '\u5142',
            '\u5328', '\u50ae', '\u5219', '\u8d3c', '\u600e', '\u5897',
            '\u624e', '\u635a', '\u6cbe', '\u5f20', '\u957f', '\u9577',
            '\u4f4b', '\u8707', '\u8d1e', '\u4e89', '\u4e4b', '\u5cd9',
            '\u5ea2', '\u4e2d', '\u5dde', '\u6731', '\u6293', '\u62fd',
            '\u4e13', '\u5986', '\u96b9', '\u5b92', '\u5353', '\u4e72',
            '\u5b97', '\u90b9', '\u79df', '\u94bb', '\u539c', '\u5c0a',
            '\u6628', '\u5159', '\u9fc3', '\u9fc4',};

    /**
     * Pinyin array.
     * <p>
     * Each pinyin is corresponding to unihans of same
     * offset in the unihans array.
     */
    public static final byte[][] PINYINS = {
            {65, 0, 0, 0, 0, 0}, {65, 73, 0, 0, 0, 0},
            {65, 78, 0, 0, 0, 0}, {65, 78, 71, 0, 0, 0},
            {65, 79, 0, 0, 0, 0}, {66, 65, 0, 0, 0, 0},
            {66, 65, 73, 0, 0, 0}, {66, 65, 78, 0, 0, 0},
            {66, 65, 78, 71, 0, 0}, {66, 65, 79, 0, 0, 0},
            {66, 69, 73, 0, 0, 0}, {66, 69, 78, 0, 0, 0},
            {66, 69, 78, 71, 0, 0}, {66, 73, 0, 0, 0, 0},
            {66, 73, 65, 78, 0, 0}, {66, 73, 65, 79, 0, 0},
            {66, 73, 69, 0, 0, 0}, {66, 73, 78, 0, 0, 0},
            {66, 73, 78, 71, 0, 0}, {66, 79, 0, 0, 0, 0},
            {66, 85, 0, 0, 0, 0}, {67, 65, 0, 0, 0, 0},
            {67, 65, 73, 0, 0, 0}, {67, 65, 78, 0, 0, 0},
            {67, 65, 78, 71, 0, 0}, {67, 65, 79, 0, 0, 0},
            {67, 69, 0, 0, 0, 0}, {67, 69, 78, 0, 0, 0},
            {67, 69, 78, 71, 0, 0}, {90, 69, 78, 71, 0, 0},
            {67, 69, 78, 71, 0, 0}, {67, 72, 65, 0, 0, 0},
            {67, 72, 65, 73, 0, 0}, {67, 72, 65, 78, 0, 0},
            {67, 72, 65, 78, 71, 0}, {67, 72, 65, 79, 0, 0},
            {67, 72, 69, 0, 0, 0}, {67, 72, 69, 78, 0, 0},
            {83, 72, 69, 78, 0, 0}, {67, 72, 69, 78, 0, 0},
            {67, 72, 69, 78, 71, 0}, {67, 72, 73, 0, 0, 0},
            {67, 72, 79, 78, 71, 0}, {67, 72, 79, 85, 0, 0},
            {67, 72, 85, 0, 0, 0}, {67, 72, 85, 65, 0, 0},
            {67, 72, 85, 65, 73, 0}, {67, 72, 85, 65, 78, 0},
            {67, 72, 85, 65, 78, 71}, {67, 72, 85, 73, 0, 0},
            {67, 72, 85, 78, 0, 0}, {67, 72, 85, 79, 0, 0},
            {67, 73, 0, 0, 0, 0}, {67, 79, 78, 71, 0, 0},
            {67, 79, 85, 0, 0, 0}, {67, 85, 0, 0, 0, 0},
            {67, 85, 65, 78, 0, 0}, {67, 85, 73, 0, 0, 0},
            {67, 85, 78, 0, 0, 0}, {67, 85, 79, 0, 0, 0},
            {68, 65, 0, 0, 0, 0}, {68, 65, 73, 0, 0, 0},
            {68, 65, 78, 0, 0, 0}, {68, 65, 78, 71, 0, 0},
            {68, 65, 79, 0, 0, 0}, {68, 69, 0, 0, 0, 0},
            {68, 69, 78, 0, 0, 0}, {68, 69, 78, 71, 0, 0},
            {68, 73, 0, 0, 0, 0}, {68, 73, 65, 0, 0, 0},
            {68, 73, 65, 78, 0, 0}, {68, 73, 65, 79, 0, 0},
            {68, 73, 69, 0, 0, 0}, {68, 73, 78, 71, 0, 0},
            {68, 73, 85, 0, 0, 0}, {68, 79, 78, 71, 0, 0},
            {68, 79, 85, 0, 0, 0}, {68, 85, 0, 0, 0, 0},
            {68, 85, 65, 78, 0, 0}, {68, 85, 73, 0, 0, 0},
            {68, 85, 78, 0, 0, 0}, {68, 85, 79, 0, 0, 0},
            {69, 0, 0, 0, 0, 0}, {69, 73, 0, 0, 0, 0},
            {69, 78, 0, 0, 0, 0}, {69, 78, 71, 0, 0, 0},
            {69, 82, 0, 0, 0, 0}, {70, 65, 0, 0, 0, 0},
            {70, 65, 78, 0, 0, 0}, {70, 65, 78, 71, 0, 0},
            {70, 69, 73, 0, 0, 0}, {70, 69, 78, 0, 0, 0},
            {70, 69, 78, 71, 0, 0}, {70, 73, 65, 79, 0, 0},
            {70, 79, 0, 0, 0, 0}, {70, 79, 85, 0, 0, 0},
            {70, 85, 0, 0, 0, 0}, {71, 65, 0, 0, 0, 0},
            {71, 65, 73, 0, 0, 0}, {71, 65, 78, 0, 0, 0},
            {71, 65, 78, 71, 0, 0}, {71, 65, 79, 0, 0, 0},
            {71, 69, 0, 0, 0, 0}, {71, 69, 73, 0, 0, 0},
            {71, 69, 78, 0, 0, 0}, {71, 69, 78, 71, 0, 0},
            {71, 79, 78, 71, 0, 0}, {71, 79, 85, 0, 0, 0},
            {71, 85, 0, 0, 0, 0}, {71, 85, 65, 0, 0, 0},
            {71, 85, 65, 73, 0, 0}, {71, 85, 65, 78, 0, 0},
            {71, 85, 65, 78, 71, 0}, {71, 85, 73, 0, 0, 0},
            {71, 85, 78, 0, 0, 0}, {71, 85, 79, 0, 0, 0},
            {72, 65, 0, 0, 0, 0}, {72, 65, 73, 0, 0, 0},
            {72, 65, 78, 0, 0, 0}, {72, 65, 78, 71, 0, 0},
            {72, 65, 79, 0, 0, 0}, {72, 69, 0, 0, 0, 0},
            {72, 69, 73, 0, 0, 0}, {72, 69, 78, 0, 0, 0},
            {72, 69, 78, 71, 0, 0}, {72, 77, 0, 0, 0, 0},
            {72, 79, 78, 71, 0, 0}, {72, 79, 85, 0, 0, 0},
            {72, 85, 0, 0, 0, 0}, {72, 85, 65, 0, 0, 0},
            {72, 85, 65, 73, 0, 0}, {72, 85, 65, 78, 0, 0},
            {72, 85, 65, 78, 71, 0}, {72, 85, 73, 0, 0, 0},
            {72, 85, 78, 0, 0, 0}, {72, 85, 79, 0, 0, 0},
            {74, 73, 0, 0, 0, 0}, {74, 73, 65, 0, 0, 0},
            {74, 73, 65, 78, 0, 0}, {74, 73, 65, 78, 71, 0},
            {74, 73, 65, 79, 0, 0}, {74, 73, 69, 0, 0, 0},
            {74, 73, 78, 0, 0, 0}, {74, 73, 78, 71, 0, 0},
            {74, 73, 79, 78, 71, 0}, {74, 73, 85, 0, 0, 0},
            {74, 85, 0, 0, 0, 0}, {74, 85, 65, 78, 0, 0},
            {74, 85, 69, 0, 0, 0}, {74, 85, 78, 0, 0, 0},
            {75, 65, 0, 0, 0, 0}, {75, 65, 73, 0, 0, 0},
            {75, 65, 78, 0, 0, 0}, {75, 65, 78, 71, 0, 0},
            {75, 65, 79, 0, 0, 0}, {75, 69, 0, 0, 0, 0},
            {75, 69, 78, 0, 0, 0}, {75, 69, 78, 71, 0, 0},
            {75, 79, 78, 71, 0, 0}, {75, 79, 85, 0, 0, 0},
            {75, 85, 0, 0, 0, 0}, {75, 85, 65, 0, 0, 0},
            {75, 85, 65, 73, 0, 0}, {75, 85, 65, 78, 0, 0},
            {75, 85, 65, 78, 71, 0}, {75, 85, 73, 0, 0, 0},
            {75, 85, 78, 0, 0, 0}, {75, 85, 79, 0, 0, 0},
            {76, 65, 0, 0, 0, 0}, {76, 65, 73, 0, 0, 0},
            {76, 65, 78, 0, 0, 0}, {76, 65, 78, 71, 0, 0},
            {76, 65, 79, 0, 0, 0}, {76, 69, 0, 0, 0, 0},
            {76, 69, 73, 0, 0, 0}, {76, 69, 78, 71, 0, 0},
            {76, 73, 0, 0, 0, 0}, {76, 73, 65, 0, 0, 0},
            {76, 73, 65, 78, 0, 0}, {76, 73, 65, 78, 71, 0},
            {76, 73, 65, 79, 0, 0}, {76, 73, 69, 0, 0, 0},
            {76, 73, 78, 0, 0, 0}, {76, 73, 78, 71, 0, 0},
            {76, 73, 85, 0, 0, 0}, {76, 79, 0, 0, 0, 0},
            {76, 79, 78, 71, 0, 0}, {76, 79, 85, 0, 0, 0},
            {76, 85, 0, 0, 0, 0}, {76, 85, 65, 78, 0, 0},
            {76, 85, 69, 0, 0, 0}, {76, 85, 78, 0, 0, 0},
            {76, 85, 79, 0, 0, 0}, {77, 0, 0, 0, 0, 0},
            {77, 65, 0, 0, 0, 0}, {77, 65, 73, 0, 0, 0},
            {77, 65, 78, 0, 0, 0}, {77, 65, 78, 71, 0, 0},
            {77, 65, 79, 0, 0, 0}, {77, 69, 0, 0, 0, 0},
            {77, 69, 73, 0, 0, 0}, {77, 69, 78, 0, 0, 0},
            {77, 69, 78, 71, 0, 0}, {77, 73, 0, 0, 0, 0},
            {77, 73, 65, 78, 0, 0}, {77, 73, 65, 79, 0, 0},
            {77, 73, 69, 0, 0, 0}, {77, 73, 78, 0, 0, 0},
            {77, 73, 78, 71, 0, 0}, {77, 73, 85, 0, 0, 0},
            {77, 79, 0, 0, 0, 0}, {77, 79, 85, 0, 0, 0},
            {77, 85, 0, 0, 0, 0}, {78, 0, 0, 0, 0, 0},
            {78, 65, 0, 0, 0, 0}, {78, 65, 73, 0, 0, 0},
            {78, 65, 78, 0, 0, 0}, {78, 65, 78, 71, 0, 0},
            {78, 65, 79, 0, 0, 0}, {78, 69, 0, 0, 0, 0},
            {78, 69, 73, 0, 0, 0}, {78, 69, 78, 0, 0, 0},
            {78, 69, 78, 71, 0, 0}, {78, 73, 0, 0, 0, 0},
            {78, 73, 65, 78, 0, 0}, {78, 73, 65, 78, 71, 0},
            {78, 73, 65, 79, 0, 0}, {78, 73, 69, 0, 0, 0},
            {78, 73, 78, 0, 0, 0}, {78, 73, 78, 71, 0, 0},
            {78, 73, 85, 0, 0, 0}, {78, 79, 78, 71, 0, 0},
            {78, 79, 85, 0, 0, 0}, {78, 85, 0, 0, 0, 0},
            {78, 85, 65, 78, 0, 0}, {78, 85, 69, 0, 0, 0},
            {78, 85, 78, 0, 0, 0}, {78, 85, 79, 0, 0, 0},
            {79, 0, 0, 0, 0, 0}, {79, 85, 0, 0, 0, 0},
            {80, 65, 0, 0, 0, 0}, {80, 65, 73, 0, 0, 0},
            {80, 65, 78, 0, 0, 0}, {80, 65, 78, 71, 0, 0},
            {80, 65, 79, 0, 0, 0}, {80, 69, 73, 0, 0, 0},
            {80, 69, 78, 0, 0, 0}, {80, 69, 78, 71, 0, 0},
            {80, 73, 0, 0, 0, 0}, {80, 73, 65, 78, 0, 0},
            {80, 73, 65, 79, 0, 0}, {80, 73, 69, 0, 0, 0},
            {80, 73, 78, 0, 0, 0}, {80, 73, 78, 71, 0, 0},
            {80, 79, 0, 0, 0, 0}, {80, 79, 85, 0, 0, 0},
            {80, 85, 0, 0, 0, 0}, {81, 73, 0, 0, 0, 0},
            {81, 73, 65, 0, 0, 0}, {81, 73, 65, 78, 0, 0},
            {81, 73, 65, 78, 71, 0}, {81, 73, 65, 79, 0, 0},
            {81, 73, 69, 0, 0, 0}, {81, 73, 78, 0, 0, 0},
            {81, 73, 78, 71, 0, 0}, {81, 73, 79, 78, 71, 0},
            {81, 73, 85, 0, 0, 0}, {81, 85, 0, 0, 0, 0},
            {81, 85, 65, 78, 0, 0}, {81, 85, 69, 0, 0, 0},
            {81, 85, 78, 0, 0, 0}, {82, 65, 78, 0, 0, 0},
            {82, 65, 78, 71, 0, 0}, {82, 65, 79, 0, 0, 0},
            {82, 69, 0, 0, 0, 0}, {82, 69, 78, 0, 0, 0},
            {82, 69, 78, 71, 0, 0}, {82, 73, 0, 0, 0, 0},
            {82, 79, 78, 71, 0, 0}, {82, 79, 85, 0, 0, 0},
            {82, 85, 0, 0, 0, 0}, {82, 85, 65, 0, 0, 0},
            {82, 85, 65, 78, 0, 0}, {82, 85, 73, 0, 0, 0},
            {82, 85, 78, 0, 0, 0}, {82, 85, 79, 0, 0, 0},
            {83, 65, 0, 0, 0, 0}, {83, 65, 73, 0, 0, 0},
            {83, 65, 78, 0, 0, 0}, {83, 65, 78, 71, 0, 0},
            {83, 65, 79, 0, 0, 0}, {83, 69, 0, 0, 0, 0},
            {83, 69, 78, 0, 0, 0}, {83, 69, 78, 71, 0, 0},
            {83, 72, 65, 0, 0, 0}, {83, 72, 65, 73, 0, 0},
            {83, 72, 65, 78, 0, 0}, {83, 72, 65, 78, 71, 0},
            {83, 72, 65, 79, 0, 0}, {83, 72, 69, 0, 0, 0},
            {83, 72, 69, 78, 0, 0}, {88, 73, 78, 0, 0, 0},
            {83, 72, 69, 78, 0, 0}, {83, 72, 69, 78, 71, 0},
            {83, 72, 73, 0, 0, 0}, {83, 72, 79, 85, 0, 0},
            {83, 72, 85, 0, 0, 0}, {83, 72, 85, 65, 0, 0},
            {83, 72, 85, 65, 73, 0}, {83, 72, 85, 65, 78, 0},
            {83, 72, 85, 65, 78, 71}, {83, 72, 85, 73, 0, 0},
            {83, 72, 85, 78, 0, 0}, {83, 72, 85, 79, 0, 0},
            {83, 73, 0, 0, 0, 0}, {83, 79, 78, 71, 0, 0},
            {83, 79, 85, 0, 0, 0}, {83, 85, 0, 0, 0, 0},
            {83, 85, 65, 78, 0, 0}, {83, 85, 73, 0, 0, 0},
            {83, 85, 78, 0, 0, 0}, {83, 85, 79, 0, 0, 0},
            {84, 65, 0, 0, 0, 0}, {84, 65, 73, 0, 0, 0},
            {84, 65, 78, 0, 0, 0}, {84, 65, 78, 71, 0, 0},
            {84, 65, 79, 0, 0, 0}, {84, 69, 0, 0, 0, 0},
            {84, 69, 78, 71, 0, 0}, {84, 73, 0, 0, 0, 0},
            {84, 73, 65, 78, 0, 0}, {84, 73, 65, 79, 0, 0},
            {84, 73, 69, 0, 0, 0}, {84, 73, 78, 71, 0, 0},
            {84, 79, 78, 71, 0, 0}, {84, 79, 85, 0, 0, 0},
            {84, 85, 0, 0, 0, 0}, {84, 85, 65, 78, 0, 0},
            {84, 85, 73, 0, 0, 0}, {84, 85, 78, 0, 0, 0},
            {84, 85, 79, 0, 0, 0}, {87, 65, 0, 0, 0, 0},
            {87, 65, 73, 0, 0, 0}, {87, 65, 78, 0, 0, 0},
            {87, 65, 78, 71, 0, 0}, {87, 69, 73, 0, 0, 0},
            {87, 69, 78, 0, 0, 0}, {87, 69, 78, 71, 0, 0},
            {87, 79, 0, 0, 0, 0}, {87, 85, 0, 0, 0, 0},
            {88, 73, 0, 0, 0, 0}, {88, 73, 65, 0, 0, 0},
            {88, 73, 65, 78, 0, 0}, {88, 73, 65, 78, 71, 0},
            {88, 73, 65, 79, 0, 0}, {88, 73, 69, 0, 0, 0},
            {88, 73, 78, 0, 0, 0}, {88, 73, 78, 71, 0, 0},
            {88, 73, 79, 78, 71, 0}, {88, 73, 85, 0, 0, 0},
            {88, 85, 0, 0, 0, 0}, {88, 85, 65, 78, 0, 0},
            {88, 85, 69, 0, 0, 0}, {88, 85, 78, 0, 0, 0},
            {89, 65, 0, 0, 0, 0}, {89, 65, 78, 0, 0, 0},
            {89, 65, 78, 71, 0, 0}, {89, 65, 79, 0, 0, 0},
            {89, 69, 0, 0, 0, 0}, {89, 73, 0, 0, 0, 0},
            {89, 73, 78, 0, 0, 0}, {89, 73, 78, 71, 0, 0},
            {89, 79, 0, 0, 0, 0}, {89, 79, 78, 71, 0, 0},
            {89, 79, 85, 0, 0, 0}, {89, 85, 0, 0, 0, 0},
            {89, 85, 65, 78, 0, 0}, {89, 85, 69, 0, 0, 0},
            {89, 85, 78, 0, 0, 0}, {74, 85, 78, 0, 0, 0},
            {89, 85, 78, 0, 0, 0}, {90, 65, 0, 0, 0, 0},
            {90, 65, 73, 0, 0, 0}, {90, 65, 78, 0, 0, 0},
            {90, 65, 78, 71, 0, 0}, {90, 65, 79, 0, 0, 0},
            {90, 69, 0, 0, 0, 0}, {90, 69, 73, 0, 0, 0},
            {90, 69, 78, 0, 0, 0}, {90, 69, 78, 71, 0, 0},
            {90, 72, 65, 0, 0, 0}, {90, 72, 65, 73, 0, 0},
            {90, 72, 65, 78, 0, 0}, {90, 72, 65, 78, 71, 0},
            {67, 72, 65, 78, 71, 0}, {90, 72, 65, 78, 71, 0},
            {90, 72, 65, 79, 0, 0}, {90, 72, 69, 0, 0, 0},
            {90, 72, 69, 78, 0, 0}, {90, 72, 69, 78, 71, 0},
            {90, 72, 73, 0, 0, 0}, {83, 72, 73, 0, 0, 0},
            {90, 72, 73, 0, 0, 0}, {90, 72, 79, 78, 71, 0},
            {90, 72, 79, 85, 0, 0}, {90, 72, 85, 0, 0, 0},
            {90, 72, 85, 65, 0, 0}, {90, 72, 85, 65, 73, 0},
            {90, 72, 85, 65, 78, 0}, {90, 72, 85, 65, 78, 71},
            {90, 72, 85, 73, 0, 0}, {90, 72, 85, 78, 0, 0},
            {90, 72, 85, 79, 0, 0}, {90, 73, 0, 0, 0, 0},
            {90, 79, 78, 71, 0, 0}, {90, 79, 85, 0, 0, 0},
            {90, 85, 0, 0, 0, 0}, {90, 85, 65, 78, 0, 0},
            {90, 85, 73, 0, 0, 0}, {90, 85, 78, 0, 0, 0},
            {90, 85, 79, 0, 0, 0}, {0, 0, 0, 0, 0, 0},
            {83, 72, 65, 78, 0, 0}, {0, 0, 0, 0, 0, 0},};

    /**
     * First and last Chinese character with known Pinyin according to zh collation
     */
    private static final String FIRST_PINYIN_UNIHAN = "\u963F";
    private static final String LAST_PINYIN_UNIHAN = "\u9FFF";

    private static final Collator COLLATOR = Collator.getInstance(Locale.CHINA);

    private static HanziToPinyin sInstance;
    private final boolean mHasChinaCollator;

    public static class Token {
        /**
         * Separator between target string for each source char
         */
        public static final String SEPARATOR = " ";

        public static final int LATIN = 1;
        public static final int PINYIN = 2;
        public static final int UNKNOWN = 3;

        public Token() {
        }

        public Token(int type, String source, String target) {
            this.type = type;
            this.source = source;
            this.target = target;
        }

        /**
         * Type of this token, ASCII, PINYIN or UNKNOWN.
         */
        public int type;
        /**
         * Original string before translation.
         */
        public String source;
        /**
         * Translated string of source. For Han, target is corresponding Pinyin. Otherwise target is
         * original string in source.
         */
        public String target;
    }

    protected HanziToPinyin(boolean hasChinaCollator) {
        mHasChinaCollator = hasChinaCollator;
    }

    public static HanziToPinyin getInstance() {
        synchronized (HanziToPinyin.class) {
            if (sInstance != null) {
                return sInstance;
            }
            // Check if zh_CN collation data is available
            final Locale locale[] = Collator.getAvailableLocales();
            for (int i = 0; i < locale.length; i++) {
                if (locale[i].equals(Locale.CHINA) || locale[i].getLanguage().contains("zh")) {
                    // Do self validation just once.
                    if (DEBUG) {
                        Log.d(TAG, "Self validation. Result: " + doSelfValidation());
                    }
                    sInstance = new HanziToPinyin(true);
                    return sInstance;
                }
            }
            if (sInstance == null){//这个判断是用于处理国产ROM的兼容性问题
                if (Locale.CHINA.equals(Locale.getDefault())){
                    sInstance = new HanziToPinyin(true);
                    return sInstance;
                }
            }
            Log.w(TAG, "There is no Chinese collator, HanziToPinyin is disabled");
            sInstance = new HanziToPinyin(false);
            return sInstance;
        }
    }

    /**
     * Validate if our internal table has some wrong value.
     *
     * @return true when the table looks correct.
     */
    private static boolean doSelfValidation() {
        char lastChar = UNIHANS[0];
        String lastString = Character.toString(lastChar);
        for (char c : UNIHANS) {
            if (lastChar == c) {
                continue;
            }
            final String curString = Character.toString(c);
            int cmp = COLLATOR.compare(lastString, curString);
            if (cmp >= 0) {
                Log.e(TAG, "Internal error in Unihan table. " + "The last string \"" + lastString
                        + "\" is greater than current string \"" + curString + "\".");
                return false;
            }
            lastString = curString;
        }
        return true;
    }

    private Token getToken(char character) {
        Token token = new Token();
        final String letter = Character.toString(character);
        token.source = letter;
        int offset = -1;
        int cmp;
        if (character < 256) {
            token.type = Token.LATIN;
            token.target = letter;
            return token;
        } else {
            cmp = COLLATOR.compare(letter, FIRST_PINYIN_UNIHAN);
            if (cmp < 0) {
                token.type = Token.UNKNOWN;
                token.target = letter;
                return token;
            } else if (cmp == 0) {
                token.type = Token.PINYIN;
                offset = 0;
            } else {
                cmp = COLLATOR.compare(letter, LAST_PINYIN_UNIHAN);
                if (cmp > 0) {
                    token.type = Token.UNKNOWN;
                    token.target = letter;
                    return token;
                } else if (cmp == 0) {
                    token.type = Token.PINYIN;
                    offset = UNIHANS.length - 1;
                }
            }
        }

        token.type = Token.PINYIN;
        if (offset < 0) {
            int begin = 0;
            int end = UNIHANS.length - 1;
            while (begin <= end) {
                offset = (begin + end) / 2;
                final String unihan = Character.toString(UNIHANS[offset]);
                cmp = COLLATOR.compare(letter, unihan);
                if (cmp == 0) {
                    break;
                } else if (cmp > 0) {
                    begin = offset + 1;
                } else {
                    end = offset - 1;
                }
            }
        }
        if (cmp < 0) {
            offset--;
        }
        StringBuilder pinyin = new StringBuilder();
        for (int j = 0; j < PINYINS[offset].length && PINYINS[offset][j] != 0; j++) {
            pinyin.append((char) PINYINS[offset][j]);
        }
        token.target = pinyin.toString();
        if (TextUtils.isEmpty(token.target)) {
            token.type = Token.UNKNOWN;
            token.target = token.source;
        }
        return token;
    }

    /**
     * Convert the input to a array of tokens. The sequence of ASCII or Unknown characters without
     * space will be put into a Token, One Hanzi character which has pinyin will be treated as a
     * Token. If these is no China collator, the empty token array is returned.
     */
    public ArrayList<Token> get(final String input) {
        ArrayList<Token> tokens = new ArrayList<Token>();
        if (!mHasChinaCollator || TextUtils.isEmpty(input)) {
            // return empty tokens.
            return tokens;
        }
        final int inputLength = input.length();
        final StringBuilder sb = new StringBuilder();
        int tokenType = Token.LATIN;
        // Go through the input, create a new token when
        // a. Token type changed
        // b. Get the Pinyin of current charater.
        // c. current character is space.
        for (int i = 0; i < inputLength; i++) {
            final char character = input.charAt(i);
            if (character == ' ') {
                if (sb.length() > 0) {
                    addToken(sb, tokens, tokenType);
                }
            } else if (character < 256) {
                if (tokenType != Token.LATIN && sb.length() > 0) {
                    addToken(sb, tokens, tokenType);
                }
                tokenType = Token.LATIN;
                sb.append(character);
            } else {
                Token t = getToken(character);
                if (t.type == Token.PINYIN) {
                    if (sb.length() > 0) {
                        addToken(sb, tokens, tokenType);
                    }
                    tokens.add(t);
                    tokenType = Token.PINYIN;
                } else {
                    if (tokenType != t.type && sb.length() > 0) {
                        addToken(sb, tokens, tokenType);
                    }
                    tokenType = t.type;
                    sb.append(character);
                }
            }
        }
        if (sb.length() > 0) {
            addToken(sb, tokens, tokenType);
        }
        return tokens;
    }

    private void addToken(
            final StringBuilder sb, final ArrayList<Token> tokens, final int tokenType) {
        String str = sb.toString();
        tokens.add(new Token(tokenType, str, str));
        sb.setLength(0);
    }

    public String toPinyinString(String string) {
        if (string == null) {
            return null;
        }
        StringBuilder sb = new StringBuilder();
        ArrayList<Token> tokens = get(string);
        for (Token token : tokens) {
            sb.append(token.target);
        }
        return sb.toString().toLowerCase();
    }
}