[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | **فارسی**

# درباره پروژه

ابزاری برای پورت‌کردن خودکار فایل‌های اجرایی PS5 به Linux و Windows.

امکان ساخت مستقل ابزار `relinker` روی macOS نیز وجود دارد؛ از جمله روی دستگاه‌های مجهز به Apple silicon.

[**مبدل تحت وب**](https://boykopovar.github.io/AnyPS5/) — فایل‌ها برای تبدیل از دستگاه شما خارج نمی‌شوند و روی همان دستگاه باقی می‌مانند.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

این پروژه شامل یک [relinker](../../core/relinker) است که فایل اجرایی را به قالب بومی سیستم‌عامل مقصد تبدیل می‌کند. همچنین پیاده‌سازی‌هایی از [کتابخانه‌های سیستمی PRX](../../core/libs/prx) را برای پیونددهی پویا (dynamic linking) فراهم می‌کند. این فرایند به شبیه‌سازی یا یک فرایند اجرایی مستقل نیاز ندارد.

[**Discord**](https://discord.gg/BHFztBPUe)، [راهنمای استفاده](USAGE.md)، [راهنمای ساخت](../dev/BUILD.md)، [معماری پروژه](../dev/ARCHITECTURE.md)، [بدهی فنی پروژه](../dev/TechnicalDebt.md)، [قواعد سبک کدنویسی](../dev/CONVENTIONS.md)، [راهنمای مشارکت](../../CONTRIBUTING.md)

## وضعیت پیشرفت

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shaders](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[تاریخچه پیشرفت پروژه](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* کتابخانه‌های سیستمی: این درصد، سهم توابعی را نشان می‌دهد که پروژه تاکنون شناسایی و در [core/libs/prx](../../core/libs/prx) اعلان کرده است؛ این عدد نشان‌دهنده میزان پوشش تمام توابع سیستمی PS5 نیست. با شناسایی و اعلان توابع بیشتر، تعداد کل توابع شناخته‌شده نیز افزایش پیدا می‌کند.</sub>

حالت‌های پشتیبانی‌نشده یا پیش‌بینی‌نشده با پرتاب یک استثنای C++ اعلام می‌شوند که معمولاً از نوع `std::runtime_error` است. اگر هیچ بخشی این استثنا را دریافت نکند، متن آن (خروجی `what()`) در `stderr` چاپ می‌شود و فرایند خاتمه می‌یابد.

[بازکامپایلر شیدر](../../core/shader/recompiler/Recompiler.cpp) با موفقیت خروجی SPIR-V تولید می‌کند. این خروجی در صورت ساخت پروژه با گزینه `ANYPS5_ENABLE_SPIRV_TOOLS`، از طریق [Spirv-Tools](../../3rdparty/SPIRV-Tools) اعتبارسنجی می‌شود.

## سازگاری

وضعیت سازگاری هر بازی و گزارش‌های آزمون مربوط به آن در بخش گفتگوهای [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up) نگهداری می‌شود. برای هر عنوان، یک گفت‌وگوی جداگانه در نظر گرفته شده است.

## نگاشت ورودی

کنترلرهای بازی با نگاشت SDL پشتیبانی می‌شوند؛ از جمله استیک‌های آنالوگ و تریگرها. ورودی‌های صفحه‌کلید و ماوس را نیز می‌توان از طریق فایل `anyps5-input.ini` پیکربندی کرد. برای مشاهده دستگاه‌های پشتیبانی‌شده و قالب فایل تنظیمات، به [راهنمای نگاشت ورودی](INPUT_MAPPING.md) مراجعه کنید.

## سلب مسئولیت

این پروژه با اهداف تعامل‌پذیری، پژوهش، حفظ و نگه‌داری نرم‌افزار و سازگاری توسعه داده شده است. این پروژه هیچ نرم‌افزار دارای حق نشر، میان‌افزار (firmware)، کلید رمزنگاری یا کتابخانه اختصاصی را در خود جای نمی‌دهد، توزیع نمی‌کند و استفاده از آن‌ها را نیز الزامی نمی‌سازد. کاربران مسئول‌اند اطمینان حاصل کنند که فایل‌های باینری مورد استفاده در کنار این پروژه، مطابق قوانین قابل‌اعمال و شرایط مجوزهای مربوط تهیه و استفاده شده‌اند.

AnyPS5 پروژه‌ای مستقل است و به Sony Interactive Entertainment وابستگی ندارد؛ از این شرکت مجوز دریافت نکرده و مورد تأیید آن نیز نیست. نام‌های «PlayStation» و «PS5» علامت‌های تجاری Sony Interactive Entertainment Inc. هستند.

## مجوز

این پروژه منحصراً تحت نسخه ۲ مجوز عمومی همگانی GNU، با شناسه `GPL-2.0-only`، منتشر می‌شود.
