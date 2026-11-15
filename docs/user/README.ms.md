[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md) | **Bahasa Melayu**

# Tentang

Alat untuk porting fail executable secara automatik ke Linux dan Windows.

Binaan khas relinker berjalan pada macOS, termasuk Apple silicon.

[**Penukar web**](https://boykopovar.github.io/AnyPS5/) - fail kekal pada peranti anda.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Termasuk [relinker](core/relinker) yang menukar fail executable kepada format asli sistem sasaran, serta implementasi [pustaka prx sistem](core/libs/prx) yang sesuai untuk dynamic linking. Tiada emulasi atau proses runtime berasingan.

[**Discord**](https://discord.gg/x9naWVzC6A), [Penggunaan](docs/user/USAGE.md), [Arahan binaan](docs/dev/BUILD.md), [Seni bina](docs/dev/ARCHITECTURE.md), [Technical debt projek](docs/dev/TechnicalDebt.md), [konvensyen gaya kod](docs/dev/CONVENTIONS.md), [menyumbang](CONTRIBUTING.md)

## Status

[![pustaka](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![bentuk shader rujukan RDNA2 dinyahkod](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![bentuk shader rujukan gfx1013 dinyahkod](https://boykopovar.github.io/AnyPS5/badge-shaders-gfx1013.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![dilaksanakan dalam 7 hari](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![peta kemajuan](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Sejarah kemajuan](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Pustaka sistem: peratusan fungsi yang diketahui oleh projek setakat ini (diisytiharkan dalam [core/libs/prx](core/libs/prx)), bukan keseluruhan fungsi sistem PS5. Jumlah ini bertambah apabila lebih banyak fungsi diisytiharkan.</sub>

Keadaan yang tidak disokong atau tidak dijangka akan throw pengecualian C++, biasanya `std::runtime_error`. Pengecualian yang tidak ditangkap akan mencetak `what()` ke stderr dan menamatkan proses.

[Pengkompil semula shader](core/shader/recompiler/Recompiler.cpp) berjaya menghasilkan SPIR-V (disahkan melalui [Spirv-Tools](3rdparty/SPIRV-Tools) apabila dibina dengan `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Kesepadanan

Status setiap permainan dan laporan ujian terdapat dalam perbincangan [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), satu thread bagi setiap tajuk.

## Pemetaan input

Pengawal permainan yang dipetakan SDL disokong, termasuk stik analog dan butang picu. Kawalan papan kekunci dan tetikus boleh dikonfigurasikan dengan fail `anyps5-input.ini`. Lihat [pemetaan input](docs/user/INPUT_MAPPING.md) untuk peranti yang disokong dan format konfigurasi.

## Penafian

Projek ini bertujuan untuk interoperabiliti, penyelidikan, pemeliharaan dan keserasian. Ia tidak mengandungi, mengedar atau memerlukan perisian berhak cipta, firmware, kunci kriptografi atau pustaka proprietari. Pengguna bertanggungjawab memastikan sebarang binari yang digunakan dengan projek ini diperoleh dan digunakan selaras dengan undang-undang yang terpakai serta terma lesen masing-masing.

AnyPS5 ialah projek bebas dan tidak bergabung dengan, tidak dibenarkan oleh, atau tidak diiktiraf oleh Sony Interactive Entertainment. "PlayStation" dan "PS5" ialah tanda dagangan Sony Interactive Entertainment Inc.

## Lesen

Projek ini dilesenkan di bawah GNU General Public License versi 2 sahaja.
