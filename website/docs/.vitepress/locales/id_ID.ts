import { createRequire } from 'module'
import { defineConfig } from 'vitepress'

const require = createRequire(import.meta.url)
const pkg = require('vitepress/package.json')

export default defineConfig({
  lang: 'id-ID',
  description: 'Solusi root kernel-based untuk perangkat Android GKI.',

  themeConfig: {
    nav: nav(),

    lastUpdatedText: 'Update Terakhir',

    sidebar: {
      '/id_ID/guide/': sidebarGuide()
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/tiann/KernelSU' }
    ],

    footer: {
        message: 'Rilis Dibawah Lisensi GPL3.',
        copyright: 'Copyright © 2022-Sekarang pengembang KernelSU '
    },

    editLink: {
        pattern: 'https://github.com/tiann/KernelSU/edit/main/website/docs/:path',
        text: 'Edit Halaman ini di GitHub'
    }
  }
})

function nav() {
  return [
    { text: 'Petunjuk', link: '/id_ID/guide/what-is-kernelsu' },
  ]
}

function sidebarGuide() {
  return [
    {
        text: 'Petunjuk',
        items: [
          { text: 'Apa itu KernelSU?', link: '/id_ID/guide/what-is-kernelsu' },
          { text: 'Instalasi', link: '/id_ID/guide/installation' },
          { text: 'Bagaimana cara buildnya?', link: '/id_ID/guide/how-to-build' },
          { text: 'Integrasi untuk perangkat non-GKI', link: '/id_ID/guide/how-to-integrate-for-non-gki'},
          { text: 'Perangkat yang didukung secara tidak resmi', link: '/id_ID/guide/unofficially-support-devices.md' },
          { text: 'Petunjuk module', link: '/id_ID/guide/module.md' },
          { text: 'Antisipasi dari bootloop', link: '/id_ID/guide/rescue-from-bootloop.md' },
          { text: 'FAQ', link: '/id_ID/guide/faq' },
        ]
    }
  ]
}
