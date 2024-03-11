import { createRequire } from 'module'
import { defineConfig } from 'vitepress'

const require = createRequire(import.meta.url)
const pkg = require('vitepress/package.json')

export default defineConfig({
  lang: 'zh-CN',
  description: '一个基于内核，为安卓 GKI 准备的 root 方案。',

  themeConfig: {
    nav: nav(),

    lastUpdatedText: '最后更新',

    sidebar: {
      '/zh_CN/guide/': sidebarGuide()
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/tiann/KernelSU' }
    ],

    footer: {
        message: '在 GPL3 许可证下发布。',
        copyright: 'Copyright © 2022-现在 KernelSU 开发者'
    },

    editLink: {
        pattern: 'https://github.com/tiann/KernelSU/edit/main/website/docs/:path',
        text: '在 GitHub 中编辑本页'
    }
  }
})

function nav() {
  return [
    { text: '指南', link: '/zh_CN/guide/what-is-kernelsu' },
  ]
}

function sidebarGuide() {
  return [
    {
        text: 'Guide',
        items: [
          { text: '什么是 KernelSU?', link: '/zh_CN/guide/what-is-kernelsu' },
          { text: 'KernelSU 模块与 Magisk 的差异', link: '/zh_CN/guide/difference-with-magisk' },
          { text: '安装', link: '/zh_CN/guide/installation' },
          { text: '如何构建?', link: '/zh_CN/guide/how-to-build' },
          { text: '如何为非GKI设备集成 KernelSU', link: '/zh_CN/guide/how-to-integrate-for-non-gki'},
          { text: '非官方支持设备', link: '/zh_CN/guide/unofficially-support-devices.md' },
          { text: '模块开发指南', link: '/zh_CN/guide/module.md' },
          { text: '模块 Web 界面', link: '/zh_CN/guide/module-webui.md' },
          { text: 'App Profile', link: '/zh_CN/guide/app-profile.md' },
          { text: '救砖', link: '/zh_CN/guide/rescue-from-bootloop.md' },
          { text: '常见问题', link: '/zh_CN/guide/faq' },
          { text: '隐藏功能', link: '/zh_CN/guide/hidden-features' },
        ]
    }
  ]
}
