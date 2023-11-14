import { createRequire } from 'module'
import { defineConfig } from 'vitepress'

const require = createRequire(import.meta.url)
const pkg = require('vitepress/package.json')

export default defineConfig({
  lang: 'ja-JP',
  description: 'Android GKI デバイス向けのカーネルベースの root ソリューション',

  themeConfig: {
    nav: nav(),

    lastUpdatedText: '最終更新',

    sidebar: {
      '/ja_JP/guide/': sidebarGuide()
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/tiann/KernelSU' }
    ],

    footer: {
      message: 'GPL3 ライセンスでリリースされています。',
      copyright: 'Copyright © 2022-現在 KernelSU 開発者'
    },

    editLink: {
      pattern: 'https://github.com/tiann/KernelSU/edit/main/website/docs/:path',
      text: 'GitHub でこのページを編集'
    }
  }
})

function nav() {
  return [
    { text: 'ガイド', link: '/ja_JP/guide/what-is-kernelsu' },
  ]
}

function sidebarGuide() {
  return [
    {
      text: 'ガイド',
      items: [
        { text: 'KernelSU とは?', link: '/ja_JP/guide/what-is-kernelsu' },
        { text: 'インストール', link: '/ja_JP/guide/installation' },
        { text: 'ビルドするには?', link: '/guide/how-to-build' },
        { text: '非 GKI デバイスでの実装', link: '/guide/how-to-integrate-for-non-gki' },
        { text: '非公式の対応デバイス', link: '/ja_JP/guide/unofficially-support-devices.md' },
        { text: 'モジュールのガイド', link: '/ja_JP/guide/module.md' },
        { text: 'ブートループからの復旧', link: '/ja_JP/guide/rescue-from-bootloop.md' },
        { text: 'よくある質問', link: '/ja_JP/guide/faq' },
        { text: '隠し機能', link: '/ja_JP/guide/hidden-features' },
      ]
    }
  ]
}
