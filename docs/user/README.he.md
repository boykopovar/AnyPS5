[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md) | [العربية](README.ar.md) | [Ελληνικά](README.el.md) | [Türkçe](README.tr.md) | **עברית** | [Deutsch](README.de.md)

# אודות

כלי להמרה אוטומטית של קובצי הרצה ל-Linux ול-Windows.

בניית ה-relinker בלבד רצה על macOS, כולל Apple silicon.

[**ממיר הרשת**](https://boykopovar.github.io/AnyPS5/) - הקבצים נשארים במכשיר שלך.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

כולל [relinker](../../core/relinker) שממיר את קובץ ההרצה לפורמט המקורי של מערכת היעד, ומימושים של [ספריות ה-prx של המערכת](../../core/libs/prx) המתאימים לקישור דינמי. אין אמולציה ואין תהליך runtime נפרד.

[**Discord**](https://discord.gg/x9naWVzC6A), [שימוש](USAGE.md), [הוראות בנייה](../dev/BUILD.md), [ארכיטקטורה](../dev/ARCHITECTURE.md), [החוב הטכני של הפרויקט](../dev/TechnicalDebt.md), [מוסכמות סגנון הקוד](../dev/CONVENTIONS.md), [תרומה לפרויקט](../../CONTRIBUTING.md)

## מצב

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![RDNA2 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![gfx1013 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders-gfx1013.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[היסטוריית ההתקדמות](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* ספריות מערכת: אחוז הפונקציות המוכרות לפרויקט עד כה (המוצהרות ב-[core/libs/prx](../../core/libs/prx)), ולא מכלל פונקציות המערכת של PS5. הסך הכולל גדל ככל שמוצהרות פונקציות נוספות.</sub>

מצבים שאינם נתמכים או שאינם צפויים זורקים חריגת C++, בדרך כלל `std::runtime_error`. חריגה שאיש אינו תופס מדפיסה את ה-`what()` שלה ל-stderr ומסיימת את התהליך.

[מהדר ה-shaders מחדש](../../core/shader/recompiler/Recompiler.cpp) מפיק SPIR-V בהצלחה (מאומת באמצעות [Spirv-Tools](../../3rdparty/SPIRV-Tools) בבנייה עם `ANYPS5_ENABLE_SPIRV_TOOLS`).

## תאימות

מצב כל משחק ודוחות הבדיקה נמצאים בדיונים של [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), שרשור אחד לכל כותר.

## מיפוי קלט

בקרי משחק הממופים דרך SDL נתמכים, כולל ג'ויסטיקים אנלוגיים והדקים. אפשר להגדיר שליטה במקלדת ובעכבר באמצעות קובץ `anyps5-input.ini`. ראו [מיפוי קלט](INPUT_MAPPING.md) לרשימת ההתקנים הנתמכים ולפורמט ההגדרות.

## הבהרה

פרויקט זה מיועד למטרות של יכולת פעולה הדדית, מחקר, שימור ותאימות. הוא אינו כולל, מפיץ או דורש תוכנה, קושחה, מפתחות הצפנה או ספריות קנייניות המוגנים בזכויות יוצרים. המשתמשים אחראים לוודא שכל קובץ בינארי שבו הם משתמשים עם הפרויקט הושג ונעשה בו שימוש בהתאם לחוקים החלים ולתנאי הרישיון שלו.

AnyPS5 הוא פרויקט עצמאי ואינו קשור ל-Sony Interactive Entertainment, ואינו מורשה או נתמך על ידה. "PlayStation" ו-"PS5" הם סימנים מסחריים של Sony Interactive Entertainment Inc.

## רישיון

פרויקט זה מורשה תחת GNU General Public License גרסה 2 בלבד.
