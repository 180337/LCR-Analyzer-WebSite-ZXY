import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// Backend the dev server proxies to. Override with LCR_BACKEND=http://host:port
const backend = process.env.LCR_BACKEND || 'http://localhost:8000'

export default defineConfig({
  // GitHub Pages 项目站点部署在 /LCR-Analyzer-WebSite/app/；本地开发仍使用 /。
  // 由部署工作流传入 LCR_PUBLIC_BASE，避免把仓库名硬编码进日常开发配置。
  base: process.env.LCR_PUBLIC_BASE || '/',
  plugins: [vue()],
  worker: { format: 'es' },
  server: {
    port: 5173,
    proxy: {
      '/api': backend,
      '/ws': { target: backend, ws: true },
    },
  },
})
