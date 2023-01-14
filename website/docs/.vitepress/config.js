export default {
    title: 'KernelSU',
    description: 'A kernel-based root solution for Android GKI devices',
    themeConfig: {
        nav: [
            { text: 'Guide', link: '/guide/what-is-kernelsu' },
            { text: 'Github', link: 'https://github.com/tiann/KernelSU' }
        ],
        sidebar: [
            {
              text: 'Guide',
              items: [
                { text: 'What is KernelSU?', link: '/guide/what-is-kernelsu' },
                { text: 'Installation', link: '/guide/installation' },
                { text: 'How to build?', link: '/guide/how-to-build' },
                { text: 'FAQ', link: '/guide/faq' },
              ]
            }
        ],
        footer: {
          message: 'Released under the GPL3 License.',
          copyright: 'Copyright © 2022-present KernelSU Developers'
        },
        editLink: {
            pattern: 'https://github.com/tiann/KernelSU_website/edit/main/docs/:path',
            text: 'Edit this page on GitHub'
        }
      }
  }