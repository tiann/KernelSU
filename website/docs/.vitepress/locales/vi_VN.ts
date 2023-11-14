import { createRequire } from 'module'
import { defineConfig } from 'vitepress'

const require = createRequire(import.meta.url)
const pkg = require('vitepress/package.json')

export default defineConfig({
  lang: 'vi-VN',
  description: 'Một giải pháp root trực tiếp trên kernel dành cho các thiết bị hỗ trợ GKI.',

  themeConfig: {
    nav: nav(),

    lastUpdatedText: 'cập nhật lần cuối',

    sidebar: {
      '/vi_VN/guide/': sidebarGuide()
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/tiann/KernelSU' }
    ],

    footer: {
        message: 'Phát hành dưới giấy phép GPL3.',
        copyright: 'Bản Quyền © 2022-nay KernelSU Developers'
    },

    editLink: {
        pattern: 'https://github.com/tiann/KernelSU/edit/main/website/docs/:path',
        text: 'Chỉnh sửa trang này trên GitHub'
    }
  }
})

function nav() {
  return [
    { text: 'Hướng Dẫn', link: '/vi_VN/guide/what-is-kernelsu' },
  ]
}

function sidebarGuide() {
  return [
    {
        text: 'Hướng Dẫn',
        items: [
          { text: 'KernelSU là gì?', link: '/vi_VN/guide/what-is-kernelsu' },
          { text: 'Cách cài đặt', link: '/vi_VN/guide/installation' },
          { text: 'Cách để build?', link: '/vi_VN/guide/how-to-build' },
          { text: 'Tích hợp vào thiết bị không sử dụng GKI', link: '/vi_VN/guide/how-to-integrate-for-non-gki'},
          { text: 'Thiết bị hỗ trợ không chính thức', link: '/vi_VN/guide/unofficially-support-devices.md' },
          { text: 'FAQ - Câu hỏi thường gặp', link: '/vi_VN/guide/faq' },
        ]
    }
  ]
}
