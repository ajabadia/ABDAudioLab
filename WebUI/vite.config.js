import { defineConfig } from 'vite';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.dirname(fileURLToPath(import.meta.url));
const sharedDir = path.resolve(root, '../../ABDShared');

export default defineConfig({
  root,
  base: './',
  publicDir: path.resolve(sharedDir, 'Assets'),
  build: {
    copyPublicDir: false, // Zero-copy architecture: assets served from single source of truth
    outDir: 'dist',
    emptyOutDir: true
  },
  server: {
    port: 5173,
    open: true,
    fs: {
      allow: [
        root,
        sharedDir,
        path.resolve(root, '..')
      ]
    }
  },
  resolve: {
    alias: {
      '@abdsynths/shared/styles': path.resolve(sharedDir, 'Assets/styles'),
      '@abdsynths/shared/components': path.resolve(sharedDir, 'Components'),
      '@abdsynths/shared/Components': path.resolve(sharedDir, 'Components'),
      '@abdsynths/midi-keyb/keyboard.css': path.resolve(sharedDir, 'Code/MidiKeyboard/src/keyboard.css'),
      '@abdsynths/midi-keyb': path.resolve(sharedDir, 'Code/MidiKeyboard/src/keyboard.js'),
      '@abdsynths/shared': sharedDir,
      '@components': path.resolve(sharedDir, 'Components'),
      '@assets': path.resolve(sharedDir, 'Assets'),
      '@styles': path.resolve(sharedDir, 'Assets/styles')
    }
  }
});
