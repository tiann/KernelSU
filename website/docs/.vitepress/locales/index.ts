import { defineConfig } from 'vitepress'
import en from './en'
import zh_CN from './zh_CN'
import zh_TW from './zh_TW'
import vi_VN from './vi_VN'
import id_ID from './id_ID'
import ja_JP from './ja_JP'
import ru_RU from './ru_RU'
import pt_BR from './pt_BR'

export default defineConfig({
  locales: {
    root: {
      label: 'English',
      lang: en.lang,
      themeConfig: en.themeConfig,
      description: en.description
    },
    zh_CN: {
      label: '简体中文',
      lang: zh_CN.lang,
      themeConfig: zh_CN.themeConfig,
      description: zh_CN.description
    },
    zh_TW: {
      label: '繁體中文',
      lang: zh_TW.lang,
      themeConfig: zh_TW.themeConfig,
      description: zh_TW.description
    },
    ja_JP: {
      label: '日本語',
      lang: ja_JP.lang,
      themeConfig: ja_JP.themeConfig,
      description: ja_JP.description
    },
    vi_VN: {
      label: 'Tiếng Việt',
      lang: vi_VN.lang,
      themeConfig: vi_VN.themeConfig,
      description: vi_VN.description
    },
    id_ID: {
      label: 'Bahasa',
      lang: id_ID.lang,
      themeConfig: id_ID.themeConfig,
      description: id_ID.description
    },
    ru_RU: {
      label: 'Русский',
      lang: ru_RU.lang,
      themeConfig: ru_RU.themeConfig,
      description: ru_RU.description
    },
    pt_BR: {
      label: 'Português (Brasil)',
      lang: pt_BR.lang,
      themeConfig: pt_BR.themeConfig,
      description: pt_BR.description
    }
  }
})
