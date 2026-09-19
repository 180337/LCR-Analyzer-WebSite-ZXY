<script setup lang="ts">
import { onMounted } from 'vue'
import AppSidebar from './components/AppSidebar.vue'
import AppHeader from './components/AppHeader.vue'
import { useScanStore } from './store/scan'

const scan = useScanStore()
const staticDeploy = import.meta.env.VITE_STATIC_DEPLOY === '1'

onMounted(() => {
  // GitHub Pages 静态版不请求不存在的 /api；CSV、BLE 与 WASM 拟合均为浏览器本地链路。
  if (!staticDeploy) scan.loadScans().catch(() => {})
})
</script>

<template>
  <div class="app-shell">
    <AppSidebar />
    <div class="main">
      <AppHeader />
      <main class="content">
        <router-view />
      </main>
    </div>
  </div>
</template>
