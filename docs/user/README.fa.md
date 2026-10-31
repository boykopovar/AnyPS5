[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | **فارسی**

# درباره پروژه

ابزاری برای انتقال خودکار فایل‌های اجرایی به Linux و Windows.

حالت ساختِ فقط Relinker روی macOS، از جمله دستگاه‌های مجهز به Apple silicon، اجرا می‌شود.

[**مبدل وب**](https://boykopovar.github.io/AnyPS5/) — فایل‌ها روی دستگاه شما باقی می‌مانند.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

این پروژه شامل یک [relinker](../../core/relinker) است که فایل اجرایی را به قالب بومی سیستم مقصد تبدیل می‌کند و پیاده‌سازی‌هایی از [کتابخانه‌های سیستمی PRX](../../core/libs/prx) را برای پیونددهی پویا ارائه می‌دهد. در این فرایند از شبیه‌سازی یا فرایند اجرای جداگانه استفاده نمی‌شود.

[**Discord**](https://discord.gg/BHFztBPUe)، [راهنمای استفاده](USAGE.md)، [راهنمای ساخت](../dev/BUILD.md)، [معماری](../dev/ARCHITECTURE.md)، [بدهی فنی پروژه](../dev/TechnicalDebt.md)، [قواعد سبک کدنویسی](../dev/CONVENTIONS.md)، [راهنمای مشارکت](../../CONTRIBUTING.md)

## وضعیت

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shaders](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[تاریخچه پیشرفت](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* کتابخانه‌های سیستمی: درصد توابعی که پروژه تا این لحظه شناسایی کرده و در [core/libs/prx](../../core/libs/prx) اعلان کرده است؛ این درصد به معنی پوشش تمام توابع سیستمی PS5 نیست. با اضافه‌شدن اعلان‌های جدید، تعداد کل توابع نیز افزایش پیدا می‌کند.</sub>

حالت‌های پشتیبانی‌نشده یا غیرمنتظره باعث پرتاب یک استثنای C++ می‌شوند که معمولاً از نوع `std::runtime_error` است. اگر هیچ بخشی استثنا را دریافت نکند، مقدار `what()` آن در `stderr` چاپ می‌شود و فرایند خاتمه می‌یابد.

[بازکامپایلر شیدر](../../core/shader/recompiler/Recompiler.cpp) با موفقیت خروجی SPIR-V تولید می‌کند. این خروجی هنگام ساخت پروژه با گزینه `ANYPS5_ENABLE_SPIRV_TOOLS` از طریق [Spirv-Tools](../../3rdparty/SPIRV-Tools) اعتبارسنجی می‌شود.

## سازگاری

وضعیت هر بازی و گزارش‌های آزمون آن در بخش گفتگوهای [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up) نگهداری می‌شود؛ برای هر عنوان یک گفت‌وگوی جداگانه وجود دارد.

## نگاشت ورودی

دسته‌های بازی با نگاشت SDL پشتیبانی می‌شوند؛ از جمله استیک‌های آنالوگ و ماشه‌ها (تریگرها). کنترل‌های صفحه‌کلید و ماوس را می‌توان با فایل `anyps5-input.ini` پیکربندی کرد. برای مشاهده دستگاه‌های پشتیبانی‌شده و قالب فایل پیکربندی، به [راهنمای نگاشت ورودی](INPUT_MAPPING.md) مراجعه کنید.

## سلب مسئولیت

این پروژه با اهداف تعامل‌پذیری، پژوهش، حفظ و سازگاری توسعه داده شده است. این پروژه هیچ نرم‌افزار مشمول حق نشر، میان‌افزار (firmware)، کلید رمزنگاری یا کتابخانه اختصاصی را شامل نمی‌شود، توزیع نمی‌کند و به استفاده از آن‌ها نیاز ندارد. مسئولیت اطمینان از اینکه فایل‌های باینری مورد استفاده با این پروژه مطابق قوانین مربوط و شرایط مجوزهای مربوطه تهیه و استفاده شده‌اند، بر عهده کاربران است.

AnyPS5 پروژه‌ای مستقل است و به Sony Interactive Entertainment وابستگی ندارد، از آن مجوز دریافت نکرده و مورد تأیید آن نیست. «PlayStation» و «PS5» علامت‌های تجاری Sony Interactive Entertainment Inc. هستند.

## مجوز

این پروژه صرفاً تحت نسخه ۲ مجوز GNU General Public License (GPL-2.0-only) منتشر می‌شود.
