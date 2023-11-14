import { createRequire } from 'module'
import { defineConfig } from 'vitepress'

const require = createRequire(import.meta.url)
const pkg = require('vitepress/package.json')

export default defineConfig({
  lang: 'ru-RU',
  description: 'Решение на основе ядра root для устройств Android GKI.',

  themeConfig: {
    nav: nav(),

    lastUpdatedText: 'последнее обновление',

    sidebar: {
      '/ru_RU/guide/': sidebarGuide()
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/tiann/KernelSU' }
    ],

    footer: {
        message: 'Выпускается под лицензией GPL3.',
        copyright: 'Авторские права © 2022-текущее Разработчики KernelSU'
    },

    editLink: {
        pattern: 'https://github.com/tiann/KernelSU/edit/main/website/docs/:path',
        text: 'Редактировать эту страницу на GitHub'
    }
  }
})

function nav() {
  return [
    { text: 'Руководство', link: '/ru_RU/guide/what-is-kernelsu' },
  ]
}

function sidebarGuide() {
  return [
    {
        text: 'Руководство',
        items: [
          { text: 'Что такое KernelSU?', link: '/ru_RU/guide/what-is-kernelsu' },
          { text: 'Установка', link: '/ru_RU/guide/installation' },
          { text: 'Как собрать?', link: '/ru_RU/guide/how-to-build' },
          { text: 'Реализация в устройствах, не относящихся к GKI', link: '/ru_RU/guide/how-to-integrate-for-non-gki'},
          { text: 'Неофициально поддерживаемые устройства', link: '/ru_RU/guide/unofficially-support-devices.md' },
          { text: 'Руководство по разработке модулей', link: '/ru_RU/guide/module.md' },
          { text: 'Профиль приложений', link: '/ru_RU/guide/app-profile.md' },
          { text: 'Выход из циклической загрузки', link: '/ru_RU/guide/rescue-from-bootloop.md' },
          { text: 'FAQ', link: '/ru_RU/guide/faq' },
          { text: 'Скрытые возможности', link: '/ru_RU/guide/hidden-features' },
        ]
    }
  ]
}
