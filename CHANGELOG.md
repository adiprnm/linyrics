# Changelog

## 1.0.0

Rilis pertama. Jendela lirik melayang ala MiniLyrics untuk Linamp.

### Fitur
- **Mengikuti Linamp secara otomatis.** Membaca snapshot
  `$XDG_RUNTIME_DIR/linamp/nowplaying.json` dan menginterpolasi posisi playback
  dengan `CLOCK_MONOTONIC`, jadi sapuan karaoke tetap rapat dengan suara
  walaupun snapshot ditulis 10×/detik. Tidak butuh D-Bus/MPRIS.
- **Empat sumber lirik, berurutan:** `<lagu>.lrc` di sebelah file audio → cache
  `~/.cache/linyrics/lyrics/` → LRCLIB (`/get` lalu kaskade `/search`) →
  tidak ada. Lirik yang diunduh dan yang dipilih manual ikut disimpan ke cache.
- **Pembersihan tag sebelum query.** `(Official Music Video)`, `feat. …`, dan
  judul berbentuk `Artis - Judul` dirapikan dulu supaya LRCLIB ketemu.
- **Parser LRC lengkap:** `[ar:] [ti:] [al:] [offset:]`, banyak timestamp per
  baris (chorus berulang), enhanced LRC `<mm:ss.xx>` untuk sinkronisasi per kata,
  dan fallback teks biasa (tanpa karaoke).
- **Tampilan gaya MiniLyrics:** Arial-class bold dengan outline hitam yang
  di-dilate sekali per glyph (satu pass, bukan delapan), baris aktif paling
  terang + sapuan karaoke, progress bar opsional, scroll halus.
- **Background `none` / `dim` / `dark`.** `none` = full transparan; outline
  otomatis diperkuat supaya tetap terbaca di wallpaper terang.
- **Font:** Arial Bold/Tahoma (Windows), Liberation Sans Bold/Arimo (Linux),
  fallback Noto Sans CJK yang dimuat hanya saat ada aksara CJK.
- **Kontrol:** menu klik kanan (reload, buka `.lrc`, delay, ukuran teks,
  background, settings), `M` + panah + Enter untuk navigasi menu, drag & drop
  `.lrc`, koreksi delay `←/→`, dan `--toggle`/`--quit` untuk keybinding.
- **Satu instance.** Menjalankan `linyrics` lagi akan men-toggle window, bukan
  membuka proses kedua.
- **`tools/lrc-tap`** — generator `.lrc` tap-along (mpv + stdlib Python) untuk
  lagu yang tidak ada di LRCLIB, plus jalur otomatis yang menyalin lirik sinkron
  dari LRCLIB kalau sudah tersedia.

### Catatan rilis
- Butuh **Linamp** yang menulis `nowplaying.json` (linamp ≥ 2.1.0). Linamp versi
  lama tidak mengirim posisi, jadi lirik akan tampil tapi tidak berjalan —
  Linyrics menampilkan peringatan kuning untuk kasus ini.
- Linux: AppImage x86_64 (glibc 2.39+, sama seperti Linamp).
- Windows: zip berisi `linyrics.exe` + DLL (SDL3, libcurl). Belum code-signed.
- macOS: dmg arm64, ad-hoc signed; klik kanan → Open kalau Gatekeeper menolak.
