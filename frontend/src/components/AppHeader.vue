<script setup lang="ts">
import { computed } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { BookOpen } from '@lucide/vue'
import { useAppStore } from '../store/app'

const route = useRoute()
const router = useRouter()
const app = useAppStore()
const staticDeploy = import.meta.env.VITE_STATIC_DEPLOY === '1'
const title = computed(() => (route.meta.title as string) || '')
const sub = computed(() => (route.meta.sub as string) || '')
</script>

<template>
  <header class="topbar">
    <div class="title-block">
      <div class="title">{{ title }}</div>
      <div class="subtitle">{{ sub }}</div>
    </div>
    <div class="spacer" />
    <div class="row tight">
      <button
        class="btn sm docs-btn"
        :class="{ active: route.name === 'docs' }"
        type="button"
        @click="router.push('/docs')"
      >
        <BookOpen />使用文档
      </button>
      <span v-if="staticDeploy" class="badge good" title="GitHub Pages 静态版：CSV / Web Bluetooth / C++ WASM 均在浏览器本地运行">
        <span class="dot good"></span>
        STATIC · BLE/WASM
      </span>
      <span v-else class="badge" :class="app.deviceOnline ? 'good' : ''">
        <span class="dot" :class="app.deviceOnline ? 'good' : 'idle'"></span>
        {{ app.device }} · {{ app.deviceOnline ? 'ONLINE' : 'OFFLINE' }}
      </span>
    </div>
  </header>
</template>

<style scoped>
.docs-btn { white-space: nowrap; }
.docs-btn.active { border-color: var(--accent); color: var(--accent); }
</style>
