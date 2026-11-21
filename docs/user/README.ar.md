[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md) | **العربية**

# نبذة

أداة لنقل الملفات التنفيذية تلقائيًا إلى Linux وWindows.

بناء relinker فقط يعمل على macOS، بما في ذلك Apple silicon.

[**محوّل الويب**](https://boykopovar.github.io/AnyPS5/) - تبقى الملفات على جهازك.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

يتضمن [relinker](../../core/relinker) يحوّل الملف التنفيذي إلى الصيغة الأصلية للنظام الهدف، وتطبيقات لمكتبات [system prx](../../core/libs/prx) المناسبة للربط الديناميكي. لا محاكاة ولا عملية runtime منفصلة.

[**Discord**](https://discord.gg/x9naWVzC6A)، [الاستخدام](USAGE.md)، [تعليمات البناء](../dev/BUILD.md)، [البنية](../dev/ARCHITECTURE.md)، [الدين التقني للمشروع](../dev/TechnicalDebt.md)، [اصطلاحات نمط الكود](../dev/CONVENTIONS.md)، [المساهمة](../../CONTRIBUTING.md)

## الحالة

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![RDNA2 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![gfx1013 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders-gfx1013.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[سجل التقدّم](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* مكتبات النظام: نسبة الدوال المعروفة للمشروع حتى الآن (المعلنة في [core/libs/prx](../../core/libs/prx))، لا نسبة كل دوال نظام PS5. يزداد الإجمالي كلما أُعلن عن المزيد من الدوال.</sub>

الحالات غير المدعومة أو غير المتوقعة تُطلق استثناءً في C++، عادةً `std::runtime_error`. والاستثناء الذي لا يلتقطه أحد يطبع `what()` الخاص به إلى stderr وينهي العملية.

[مُعيد تجميع الشيدر](../../core/shader/recompiler/Recompiler.cpp) ينتج SPIR-V بنجاح (يُتحقق منه عبر [Spirv-Tools](../../3rdparty/SPIRV-Tools) عند البناء مع `ANYPS5_ENABLE_SPIRV_TOOLS`).

## التوافق

حالة كل لعبة وتقارير الاختبار موجودة في نقاشات [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up)، موضوع واحد لكل عنوان.

## تعيين الإدخال

تُدعم أذرع التحكم المعيّنة عبر SDL، بما في ذلك العصي التناظرية والزنادات. يمكن تهيئة التحكم بلوحة المفاتيح والفأرة عبر ملف `anyps5-input.ini`. راجع [تعيين الإدخال](INPUT_MAPPING.md) لمعرفة الأجهزة المدعومة وصيغة التهيئة.

## إخلاء المسؤولية

هذا المشروع مخصص لأغراض التشغيل البيني والبحث والحفظ والتوافق. لا يتضمن برمجيات أو firmware أو مفاتيح تشفير أو مكتبات احتكارية محمية بحقوق النشر، ولا يوزّعها أو يتطلبها. المستخدمون مسؤولون عن ضمان أن أي ملفات ثنائية تُستخدم مع هذا المشروع قد حُصل عليها واستُخدمت وفقًا للقوانين السارية وشروط تراخيصها.

AnyPS5 مشروع مستقل، وغير تابع لـ Sony Interactive Entertainment ولا مصرّح به أو معتمد منها. "PlayStation" و"PS5" علامتان تجاريتان لشركة Sony Interactive Entertainment Inc.

## الترخيص

هذا المشروع مرخّص بموجب GNU General Public License الإصدار 2 فقط.
