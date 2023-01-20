

import { createRequire } from 'module'
import { defineConfig } from 'vitepress'
import locales from './locales'

const require = createRequire(import.meta.url)
const pkg = require('vitepress/package.json')

export default defineConfig( {
    title: 'KernelSU',
    locales: locales.locales
})