import { defineConfig, SiteConfig } from 'vitepress'
import locales from './locales'
import { readdir, writeFile } from 'fs/promises'
import { resolve } from 'path'

export default defineConfig( {
    title: 'KernelSU',
    locales: locales.locales,
    buildEnd: async (config: SiteConfig) => {
        const templateDir = resolve(config.outDir, 'templates')
        const files = await readdir(templateDir);
        const templateList = resolve(templateDir, "index.json")
        await writeFile(templateList, JSON.stringify(files))
    }
})