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
@SuppressWarnings("SizeReplaceableByIsEmpty")
public record HanziToPinyin(boolean mHasChinaCollator) {
    private static final String TAG = "HanziToPinyin";

    // Turn on this flag when we want to check internal data structure.
    private static final boolean DEBUG = false;

    /**
     * Unihans array.
     * <p>
     * Each unihans is the first one within same pinyin when collator is zh_CN.
     */
    public static final char[] UNIHANS = {
            '阿', '哎', '安', '肮', '凹', '八',
            '挀', '扳', '邦', '勹', '陂', '奔',
            '伻', '屄', '边', '灬', '憋', '汃',
            '冫', '癶', '峬', '嚓', '偲', '参',
            '仓', '撡', '冊', '嵾', '曽', '曾',
            '層', '叉', '芆', '辿', '伥', '抄',
            '车', '抻', '沈', '沉', '阷', '吃',
            '充', '抽', '出', '欻', '揣', '巛',
            '刅', '吹', '旾', '逴', '呲', '匆',
            '凑', '粗', '汆', '崔', '邨', '搓',
            '咑', '呆', '丹', '当', '刀', '嘚',
            '扥', '灯', '氐', '嗲', '甸', '刁',
            '爹', '丁', '丟', '东', '吺', '厾',
            '耑', '襨', '吨', '多', '妸', '诶',
            '奀', '鞥', '儿', '发', '帆', '匚',
            '飞', '分', '丰', '覅', '仏', '紑',
            '伕', '旮', '侅', '甘', '冈', '皋',
            '戈', '给', '根', '刯', '工', '勾',
            '估', '瓜', '乖', '关', '光', '归',
            '丨', '呙', '哈', '咍', '佄', '夯',
            '茠', '诃', '黒', '拫', '亨', '噷',
            '叿', '齁', '乯', '花', '怀', '犿',
            '巟', '灰', '昏', '吙', '丌', '加',
            '戋', '江', '艽', '阶', '巾', '坕',
            '冂', '丩', '凥', '姢', '噘', '军',
            '咔', '开', '刊', '忼', '尻', '匼',
            '肎', '劥', '空', '抠', '扝', '夸',
            '蒯', '宽', '匡', '亏', '坤', '扩',
            '垃', '来', '兰', '啷', '捞', '肋',
            '勒', '崚', '刕', '俩', '奁', '良',
            '撩', '列', '拎', '刢', '溜', '囖',
            '龙', '瞜', '噜', '娈', '畧', '抡',
            '罗', '呣', '妈', '埋', '嫚', '牤',
            '猫', '么', '呅', '门', '甿', '咪',
            '宀', '喵', '乜', '民', '名', '谬',
            '摸', '哞', '毪', '嗯', '拏', '腉',
            '囡', '囔', '孬', '疒', '娞', '恁',
            '能', '妮', '拈', '嬢', '鸟', '捏',
            '囜', '宁', '妞', '农', '羺', '奴',
            '奻', '疟', '黁', '郍', '喔', '讴',
            '妑', '拍', '眅', '乓', '抛', '呸',
            '喷', '匉', '丕', '囨', '剽', '氕',
            '姘', '乒', '钋', '剖', '仆', '七',
            '掐', '千', '呛', '悄', '癿', '亲',
            '狅', '芎', '丘', '区', '峑', '缺',
            '夋', '呥', '穣', '娆', '惹', '人',
            '扔', '日', '茸', '厹', '邚', '挼',
            '堧', '婑', '瞤', '捼', '仨', '毢',
            '三', '桒', '掻', '閪', '森', '僧',
            '杀', '筛', '山', '伤', '弰', '奢',
            '申', '莘', '敒', '升', '尸', '収',
            '书', '刷', '衰', '闩', '双', '谁',
            '吮', '说', '厶', '忪', '捜', '苏',
            '狻', '夊', '孙', '唆', '他', '囼',
            '坍', '汤', '夲', '忑', '熥', '剔',
            '天', '旫', '帖', '厅', '囲', '偷',
            '凸', '湍', '推', '吞', '乇', '穵',
            '歪', '弯', '尣', '危', '昷', '翁',
            '挝', '乌', '夕', '虲', '仚', '乡',
            '灱', '些', '心', '星', '凶', '休',
            '吁', '吅', '削', '坃', '丫', '恹',
            '央', '幺', '倻', '一', '囙', '应',
            '哟', '佣', '优', '扜', '囦', '曰',
            '晕', '筠', '筼', '帀', '災', '兂',
            '匨', '傮', '则', '贼', '怎', '増',
            '扎', '捚', '沾', '张', '长', '長',
            '佋', '蜇', '贞', '争', '之', '峙',
            '庢', '中', '州', '朱', '抓', '拽',
            '专', '妆', '隹', '宒', '卓', '乲',
            '宗', '邹', '租', '钻', '厜', '尊',
            '昨', '兙', '鿃', '鿄'};

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
            {83, 72, 65, 78, 0, 0}, {0, 0, 0, 0, 0, 0}};

    /**
     * First and last Chinese character with known Pinyin according to zh collation
     */
    private static final String FIRST_PINYIN_UNIHAN = "阿";
    private static final String LAST_PINYIN_UNIHAN = "鿿";

    private static final Collator COLLATOR = Collator.getInstance(Locale.CHINA);

    private static HanziToPinyin sInstance;

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

    public static HanziToPinyin getInstance() {
        synchronized (HanziToPinyin.class) {
            if (sInstance != null) {
                return sInstance;
            }
            // Check if zh_CN collation data is available
            final Locale[] locale = Collator.getAvailableLocales();
            for (Locale value : locale) {
                if (value.equals(Locale.CHINA) || value.getLanguage().contains("zh")) {
                    // Do self validation just once.
                    if (DEBUG) {
                        Log.d(TAG, "Self validation. Result: " + doSelfValidation());
                    }
                    sInstance = new HanziToPinyin(true);
                    return sInstance;
                }
            }
            if (sInstance == null) {//这个判断是用于处理国产ROM的兼容性问题
                if (Locale.CHINA.equals(Locale.getDefault())) {
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
        ArrayList<Token> tokens = new ArrayList<>();
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