import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import path from "path";

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      "@": path.resolve(__dirname, "./src"),
    },
  },
  // Tauri 需要固定端口
  server: {
    port: 1420,
    strictPort: true,
  },
  // 环境变量前缀
  envPrefix: ["VITE_", "TAURI_"],
  build: {
    // Tauri 支持 ES2021
    target: ["es2021", "chrome100", "safari13"],
    // 不压缩，便于调试
    minify: !process.env.TAURI_DEBUG ? "esbuild" : false,
    // 生成 sourcemap
    sourcemap: !!process.env.TAURI_DEBUG,
  },
});
