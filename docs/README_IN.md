
[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [Polski](README_PL.md) | [Portuguese-Brazil](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_iw.md) |  **हिंदी**

<div style="display: flex; align-items: center;">
    <img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="">
    <div style="margin-left: 20px;">
        <span style="font-size: large; "><b>KernelSU</b></span>
        <br>
        <span style="font-size: medium; "><i>Android उपकरणों के लिए कर्नेल-आधारित रूट समाधान।</i></span>   
    </div>
</div>

## विशेषताएँ

1. कर्नेल-आधारित `su` और रूट एक्सेस प्रबंधन।
2. Overlayfs पर आधारित मॉड्यूल प्रणाली।
3. [App Profile](https://kernelsu.org/guide/app-profile.html): Root शक्ति को पिंजरे में बंद कर दो।

## अनुकूलता अवस्था

KernelSU आधिकारिक तौर पर Android GKI 2.0 डिवाइस (कर्नेल 5.10+) का समर्थन करता है। पुराने कर्नेल (4.14+) भी संगत हैं, लेकिन कर्नेल को मैन्युअल रूप से बनाना होगा।

इसके साथ, WSA, ChromeOS और कंटेनर-आधारित Android सभी समर्थित हैं।

वर्तमान में, केवल `arm64-v8a` और `x86_64` समर्थित हैं।

## प्रयोग

- [स्थापना निर्देश](https://kernelsu.org/guide/installation.html)
- [कैसे बनाना है ?](https://kernelsu.org/guide/how-to-build.html)
- [आधिकारिक वेबसाइट](https://kernelsu.org/)

## अनुवाद करना

KernelSU का अनुवाद करने या मौजूदा अनुवादों को बेहतर बनाने में सहायता के लिए, कृपया इसका उपयोग करें [Weblate](https://hosted.weblate.org/engage/kernelsu/).

## बहस

- Telegram: [@KernelSU](https://t.me/KernelSU)

## लाइसेंस

- `Kernel` निर्देशिका के अंतर्गत फ़ाइलें हैं [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
- `Kernel` निर्देशिका को छोड़कर अन्य सभी भाग हैं [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html)

## आभार सूची

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): KernelSU विचार।
- [Magisk](https://github.com/topjohnwu/Magisk): शक्तिशाली root उपकरण।
- [genuine](https://github.com/brevent/genuine/): apk v2 हस्ताक्षर सत्यापन।
- [Diamorphine](https://github.com/m0nad/Diamorphine): कुछ रूटकिट कौशल।
