[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md) | [العربية](README.ar.md) | [Ελληνικά](README.el.md) | **Türkçe** | [עברית](README.he.md) | [Deutsch](README.de.md) | [Română](README.ro.md)

# Hakkında

Çalıştırılabilir dosyaları Linux ve Windows'a otomatik olarak taşıyan bir araç.

Yalnızca relinker'dan oluşan derleme macOS'ta da çalışır, Apple silicon dahil.

[**Web dönüştürücü**](https://boykopovar.github.io/AnyPS5/) - dosyalar cihazınızda kalır.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Projede, çalıştırılabilir dosyayı hedef sistemin kendi formatına dönüştüren bir [relinker](../../core/relinker) ve [sistem prx kütüphanelerinin](../../core/libs/prx) dinamik bağlamaya uygun implementasyonları bulunur. Emülasyon ya da ayrı bir çalışma zamanı süreci kullanılmaz.

[**Discord**](https://discord.gg/x9naWVzC6A), [Kullanım](USAGE.md), [Derleme talimatları](../dev/BUILD.md), [Mimari](../dev/ARCHITECTURE.md), [Projenin teknik borcu](../dev/TechnicalDebt.md), [kod stili kuralları](../dev/CONVENTIONS.md), [katkıda bulunma](../../CONTRIBUTING.md)

## Durum

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![RDNA2 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![gfx1013 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders-gfx1013.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[İlerleme geçmişi](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Sistem kütüphaneleri: Oran, PS5'teki tüm sistem fonksiyonlarına göre değil, projenin şu ana kadar bildiği fonksiyonlara ([core/libs/prx](../../core/libs/prx) içinde deklare edilenler) göre hesaplanır. Daha fazla fonksiyon deklare edildikçe toplam sayı artar.</sub>

Desteklenmeyen ya da beklenmeyen bir durumda C++ istisnası (genellikle `std::runtime_error`) fırlatılır. Hiçbir yerde yakalanmayan bir istisna `what()` mesajını stderr'e yazdırır ve programı sonlandırır.

[Shader recompiler](../../core/shader/recompiler/Recompiler.cpp) başarıyla SPIR-V üretir (`ANYPS5_ENABLE_SPIRV_TOOLS` ile derlendiğinde [Spirv-Tools](../../3rdparty/SPIRV-Tools) ile doğrulanır).

## Uyumluluk

Oyunların tek tek durumu ve test raporları [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up) tartışmalarında yer alır; her oyun için ayrı bir konu açılır.

## Kontrol eşleme

SDL eşlemesi bulunan oyun kontrolcüleri, analog çubuklar ve tetik tuşları dahil, desteklenir. Klavye ve fare kontrolleri `anyps5-input.ini` dosyasıyla yapılandırılabilir. Desteklenen cihazlar ve yapılandırma formatı için [kontrol eşleme](INPUT_MAPPING.md) sayfasına bakın.

## Yasal Uyarı

Bu proje birlikte çalışabilirlik, araştırma, arşivleme ve uyumluluk amacıyla hazırlanmıştır. Telif hakkıyla korunan yazılım, bellenim (firmware), şifreleme anahtarı ya da kapalı kaynaklı kütüphane içermez, dağıtmaz ve gerektirmez. Bu projeyle kullanılan ikili dosyaların (binary) yürürlükteki yasalara ve ilgili lisans şartlarına uygun biçimde edinilmesi ve kullanılması kullanıcıların kendi sorumluluğundadır.

AnyPS5 bağımsız bir projedir. Sony Interactive Entertainment ile bağlantılı değildir; Sony tarafından yetkilendirilmemiş veya onaylanmamıştır. "PlayStation" ve "PS5", Sony Interactive Entertainment Inc. şirketinin ticari markalarıdır.

## Lisans

Bu proje, GNU Genel Kamu Lisansı'nın yalnızca 2. sürümü kapsamında lisanslanmıştır.
