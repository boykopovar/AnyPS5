[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | **Türkçe**

# Hakkında

Çalıştırılabilir dosyaları (executable) otomatik olarak Linux ve Windows'a portlamak (uyarlamak) için geliştirilmiş bir araçtır.

Yalnızca yeniden bağlayıcı (relinker-only) derlemesi, Apple Silicon dahil olmak üzere macOS üzerinde çalışır.

[**Web dönüştürücü**](https://boykopovar.github.io/AnyPS5/) - dosyalar cihazınızda kalır.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Çalıştırılabilir dosyayı hedef sistemin yerel formatına dönüştüren bir [yeniden bağlayıcı (relinker)](../../core/relinker) ve dinamik bağlamaya uygun [sistem prx kütüphanesi](../../core/libs/prx) uygulamalarını içerir. Öykünme (emülasyon) veya ayrı bir çalışma zamanı (runtime) süreci kullanmaz.

[**Discord**](https://discord.gg/x9naWVzC6A), [Kullanım](USAGE.md), [Derleme talimatları](../dev/BUILD.md), [Mimari](../dev/ARCHITECTURE.md), [Projenin teknik borcu](../dev/TechnicalDebt.md), [kod stili kuralları](../dev/CONVENTIONS.md), [katkıda bulunma](../../CONTRIBUTING.md)

## Durum

[![kütüphaneler](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![koddan sayılan çözümlenmiş gölgelendirici biçimleri](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![7 günde uygulananlar](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![ilerleme haritası](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[İlerleme geçmişi](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Sistem kütüphaneleri: projenin şu ana kadar bildiği ([core/libs/prx](../../core/libs/prx) içinde tanımlanan) işlevlerin yüzdesidir; tüm PS5 sistem işlevlerinin yüzdesi değildir. Daha fazla işlev tanımlandıkça toplam sayı artar.</sub>

Desteklenmeyen veya beklenmeyen durumlar genellikle `std::runtime_error` olmak üzere bir C++ istisnası fırlatır. Hiçbir şeyin yakalamadığı bir istisna, `what()` çıktısını `stderr`'e yazdırır ve işlemi sonlandırır.

[Gölgelendirici yeniden derleyicisi](../../core/shader/recompiler/Recompiler.cpp) başarılı bir şekilde SPIR-V üretir (`ANYPS5_ENABLE_SPIRV_TOOLS` ile derlendiğinde [Spirv-Tools](../../3rdparty/SPIRV-Tools) aracılığıyla doğrulanır).

## Uyumluluk

Oyun bazlı durumlar ve test raporları, her oyun için ayrı bir başlık olmak üzere [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up) tartışmalarında yer alır.

## Giriş eşlemesi

Analog çubuklar ve tetikler dahil olmak üzere SDL eşlemeli oyun kumandaları desteklenir. Klavye ve fare kontrolleri bir `anyps5-input.ini` dosyasıyla yapılandırılabilir. Desteklenen cihazlar ve yapılandırma biçimi için [giriş eşlemesi](INPUT_MAPPING.md) belgesine bakın.

## Yasal Uyarı

Bu proje birlikte çalışabilirlik, araştırma, koruma ve uyumluluk amaçlarına yöneliktir. Telif hakkıyla korunan yazılımlar, aygıt yazılımları, kriptografik anahtarlar veya tescilli kütüphaneler içermez, dağıtmaz veya gerektirmez. Kullanıcılar, bu projeyle birlikte kullanılan tüm ikili dosyaların yürürlükteki yasalara ve ilgili lisans koşullarına uygun olarak edinilmesini ve kullanılmasını sağlamaktan sorumludur.

AnyPS5 bağımsız bir projedir ve Sony Interactive Entertainment ile bağlantılı değildir, onun tarafından yetkilendirilmemiş veya onaylanmamıştır. "PlayStation" ve "PS5", Sony Interactive Entertainment Inc.'in ticari markalarıdır.

## Lisans

Bu proje yalnızca GNU Genel Kamu Lisansı sürüm 2 kapsamında lisanslanmıştır.
