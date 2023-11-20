import { createRequire } from 'module'
import { defineConfig } from 'vitepress'

const require = createRequire(import.meta.url)
const pkg = require('vitepress/package.json')

export default defineConfig({
  lang: 'zh-TW',
  description: '一個以核心為基礎，適用於 Android GKI 的 Root 解決方案。',

  themeConfig: {
    nav: nav(),

    lastUpdatedText: '上次更新',

    sidebar: {
      '/zh_TW/guide/': sidebarGuide()
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/tiann/KernelSU' }
    ],

    footer: {
        message: '係依據 GPL3 授權發行。',
        copyright: 'Copyright © 2022-目前 KernelSU 開發人員'
    },

    editLink: {
        pattern: 'https://github.com/tiann/KernelSU/edit/main/website/docs/:path',
        text: '在 GitHub 中編輯本頁面'
    }
  }
})

function nav() {
  return [
    { text: '指南', link: '/zh_TW/guide/what-is-kernelsu' },
  ]
}

function sidebarGuide() {
  return [
    {
        text: 'Guide',
        items: [
          { text: '什麼是 KernelSU？', link: '/zh_TW/guide/what-is-kernelsu' },
          { text: '安裝', link: '/zh_TW/guide/installation' },
          { text: '如何建置？', link: '/zh_TW/guide/how-to-build' },
          { text: '如何為非 GKI 核心整合 KernelSU', link: '/zh_TW/guide/how-to-integrate-for-non-gki'},
          { text: '非官方支援裝置', link: '/zh_TW/guide/unofficially-support-devices.md' },
          { text: '模組指南', link: '/zh_TW/guide/module.md' },
          { text: '搶救開機迴圈', link: '/zh_TW/guide/rescue-from-bootloop.md' },
          { text: '常見問題', link: '/zh_TW/guide/faq' },
          { text: '隱藏功能', link: '/zh_TW/guide/hidden-features' },
        ]
    }
  ]
}
