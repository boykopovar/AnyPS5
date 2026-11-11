[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | **Bahasa Indonesia** | [Polski](README.pl.md)

# Tentang

Alat untuk memindahkan file executable secara otomatis ke Linux dan Windows.

Build khusus relinker berjalan di macOS, termasuk Apple silicon.

[**Konverter web**](https://boykopovar.github.io/AnyPS5/) - file tetap berada di perangkat Anda.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Berisi [relinker](../../core/relinker) yang mengubah executable ke format asli sistem tujuan, serta implementasi [library prx sistem](../../core/libs/prx) yang dapat ditautkan secara dinamis. Tanpa emulasi dan tanpa proses runtime terpisah.

[**Discord**](https://discord.gg/x9naWVzC6A), [Penggunaan](USAGE.md), [Petunjuk build](../dev/BUILD.md), [Arsitektur](../dev/ARCHITECTURE.md), [Technical debt proyek](../dev/TechnicalDebt.md), [konvensi gaya kode](../dev/CONVENTIONS.md), [cara berkontribusi](../../CONTRIBUTING.md)

## Status

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shader forms decoded, counted from the code](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Riwayat progres](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Library sistem: persentase dari fungsi yang sejauh ini dikenal proyek (dideklarasikan di [core/libs/prx](../../core/libs/prx)), bukan dari seluruh fungsi sistem PS5. Totalnya bertambah seiring makin banyak fungsi yang dideklarasikan.</sub>

Keadaan yang tidak didukung atau tidak terduga melempar exception C++, biasanya `std::runtime_error`. Exception yang tidak ditangkap mencetak `what()`-nya ke stderr dan mengakhiri proses.

[Shader recompiler](../../core/shader/recompiler/Recompiler.cpp) berhasil menghasilkan SPIR-V (divalidasi dengan [Spirv-Tools](../../3rdparty/SPIRV-Tools) bila di-build dengan `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Kompatibilitas

Status per game dan laporan pengujian ada di diskusi [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), satu thread per judul.

## Pemetaan input

Controller yang dipetakan oleh SDL didukung, termasuk stik analog dan trigger. Kontrol keyboard dan mouse dapat diatur dengan file `anyps5-input.ini`. Lihat [pemetaan input](INPUT_MAPPING.md) untuk perangkat yang didukung dan format konfigurasinya.

## Penafian

Proyek ini ditujukan untuk interoperabilitas, riset, pelestarian, dan kompatibilitas. Proyek ini tidak menyertakan, mendistribusikan, atau memerlukan perangkat lunak berhak cipta, firmware, kunci kriptografi, maupun library proprietary. Pengguna bertanggung jawab memastikan bahwa setiap binary yang digunakan dengan proyek ini diperoleh dan digunakan sesuai hukum yang berlaku dan ketentuan lisensinya masing-masing.

AnyPS5 adalah proyek independen dan tidak berafiliasi dengan, tidak diizinkan, maupun tidak didukung oleh Sony Interactive Entertainment. "PlayStation" dan "PS5" adalah merek dagang Sony Interactive Entertainment Inc.

## Lisensi

Proyek ini dilisensikan di bawah GNU General Public License versi 2 saja.
