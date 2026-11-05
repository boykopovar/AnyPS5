[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | **فارسی**

# درباره پروژه

ابزاری برای انتقال خودکار فایل‌های اجرایی به Linux و Windows.

نسخه‌ای که فقط شامل `relinker` است، روی macOS، از جمله دستگاه‌های مجهز به Apple silicon، اجرا می‌شود.

[**مبدل تحت وب**](https://boykopovar.github.io/AnyPS5/) — فایل‌ها هنگام تبدیل از دستگاه شما خارج نمی‌شوند.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

این پروژه یک [relinker](../../core/relinker) دارد که فایل اجرایی را به قالب بومی سیستم‌عامل مقصد تبدیل می‌کند و پیاده‌سازی‌هایی از [کتابخانه‌های سیستمی PRX](../../core/libs/prx) را برای پیونددهی پویا فراهم می‌کند. برای این کار به شبیه‌سازی یا فرایند اجرایی جداگانه‌ای نیاز نیست.

[**Discord**](https://discord.gg/BHFztBPUe)، [راهنمای استفاده](USAGE.md)، [راهنمای ساخت](../dev/BUILD.md)، [معماری پروژه](../dev/ARCHITECTURE.md)، [بدهی فنی پروژه](../dev/TechnicalDebt.md)، [قواعد سبک کدنویسی](../dev/CONVENTIONS.md)، [راهنمای مشارکت](../../CONTRIBUTING.md)

## وضعیت

[![کتابخانه‌ها](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![فرم‌های شیدرِ رمزگشایی‌شده، شمارش‌شده بر اساس کد](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![پیاده‌سازی‌شده در ۷ روز](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![نقشهٔ پیشرفت](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[تاریخچهٔ پیشرفت](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* درصد کتابخانه‌های سیستمی، توابع شناخته‌شده در پروژه را نشان می‌دهد که در [core/libs/prx](../../core/libs/prx) تعریف شده‌اند؛ این عدد شامل همهٔ توابع سیستمی PS5 نیست. با تعریف توابع بیشتر، تعداد کل نیز افزایش می‌یابد.</sub>

در وضعیت‌های پشتیبانی‌نشده یا غیرمنتظره، یک استثنای C++ (معمولاً از نوع `std::runtime_error`) پرتاب می‌شود. اگر هیچ بخشی آن را دریافت نکند، متن آن (`what()`) در `stderr` چاپ می‌شود و فرایند پایان می‌یابد.

[بازکامپایلر شیدر](../../core/shader/recompiler/Recompiler.cpp) با موفقیت خروجی SPIR-V تولید می‌کند؛ این خروجی هنگام ساخت پروژه با گزینهٔ `ANYPS5_ENABLE_SPIRV_TOOLS` از طریق [Spirv-Tools](../../3rdparty/SPIRV-Tools) اعتبارسنجی می‌شود.

## سازگاری

وضعیت هر بازی و گزارش آزمون‌های آن در گفت‌وگوهای بخش [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up) نگهداری می‌شود؛ برای هر عنوان، یک گفت‌وگوی جداگانه وجود دارد.

## نگاشت ورودی

کنترلرهای بازی با نگاشت‌های SDL پشتیبانی می‌شوند؛ از جمله استیک‌های آنالوگ و تریگرها. ورودی‌های صفحه‌کلید و ماوس را می‌توان با فایل `anyps5-input.ini` پیکربندی کرد. برای مشاهدهٔ دستگاه‌های پشتیبانی‌شده و قالب فایل پیکربندی، به [راهنمای نگاشت ورودی](INPUT_MAPPING.md) مراجعه کنید.

## سلب مسئولیت

این پروژه با هدف تعامل‌پذیری، پژوهش، حفظ و نگه‌داری نرم‌افزار و افزایش سازگاری توسعه یافته است. این پروژه هیچ نرم‌افزار دارای حق نشر، میان‌افزار (firmware)، کلید رمزنگاری یا کتابخانهٔ اختصاصی را شامل نمی‌شود، توزیع نمی‌کند و استفاده از آن‌ها را نیز الزامی نمی‌داند. کاربران مسئول‌اند اطمینان حاصل کنند که فایل‌های باینری مورد استفاده در کنار این پروژه، مطابق قوانین قابل‌اعمال و شرایط مجوز مربوط تهیه و استفاده شده‌اند.

AnyPS5 پروژه‌ای مستقل است؛ به Sony Interactive Entertainment وابستگی ندارد، از آن مجوز نگرفته و مورد تأیید آن نیست. نام‌های «PlayStation» و «PS5» علامت‌های تجاری Sony Interactive Entertainment Inc. هستند.

## مجوز

این پروژه منحصراً تحت نسخهٔ ۲ مجوز عمومی همگانی گنو (GNU General Public License، شناسهٔ `GPL-2.0-only`) منتشر شده است.
