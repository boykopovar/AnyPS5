[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md) | [العربية](README.ar.md) | **Ελληνικά** | [Türkçe](README.tr.md) | [עברית](README.he.md)

# Σχετικά

Εργαλείο για την αυτόματη μεταφορά (porting) εκτελέσιμων αρχείων σε Linux και Windows.

Το build που περιλαμβάνει μόνο τον relinker εκτελείται και σε macOS, συμπεριλαμβανομένων των επεξεργαστών Apple silicon.

[**Μετατροπέας Web**](https://boykopovar.github.io/AnyPS5/) – τα αρχεία παραμένουν στη συσκευή σας.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Περιλαμβάνει έναν [relinker](../../core/relinker) που μετατρέπει το εκτελέσιμο αρχείο στη εγγενή (native) μορφή του συστήματος-στόχου, καθώς και υλοποιήσεις των [βιβλιοθηκών συστήματος prx](../../core/libs/prx) κατάλληλες για δυναμική σύνδεση (dynamic linking). Χωρίς εξομοίωση (emulation) ή ξεχωριστή διεργασία κατά τον χρόνο εκτέλεσης.

[**Discord**](https://discord.gg/x9naWVzC6A), [Χρήση](USAGE.md), [Οδηγίες μεταγλώττισης (Build)](../dev/BUILD.md), [Αρχιτεκτονική](../dev/ARCHITECTURE.md), [Τεχνικό χρέος του έργου](../dev/TechnicalDebt.md), [Κανόνες στυλ κώδικα](../dev/CONVENTIONS.md), [Πώς να συνεισφέρετε](../../CONTRIBUTING.md)

## Κατάσταση

[![Libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![RDNA2 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![gfx1013 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders-gfx1013.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Ιστορικό προόδου](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Βιβλιοθήκες συστήματος: ποσοστό των συναρτήσεων που είναι γνωστές στο έργο μέχρι στιγμής (δηλωμένες στο [core/libs/prx](../../core/libs/prx)), και όχι του συνόλου των συναρτήσεων συστήματος του PS5. Το σύνολο αυξάνεται καθώς δηλώνονται περισσότερες συναρτήσεις.</sub>

Οι μη υποστηριζόμενες ή απροσδόκητες καταστάσεις προκαλούν μια εξαίρεση (exception) της C++, συνήθως std::runtime_error. Μια εξαίρεση που δεν συλλαμβάνεται εκτυπώνει το what() της στο stderr και τερματίζει τη διεργασία.

Ο [αναμεταγλωττιστής shaders (shader recompiler)](../../core/shader/recompiler/Recompiler.cpp) παράγει επιτυχώς SPIR-V (επικυρωμένο μέσω [Spirv-Tools](../../3rdparty/SPIRV-Tools)) όταν χτίζεται με το ANYPS5_ENABLE_SPIRV_TOOLS.

## Συμβατότητα

Η κατάσταση κάθε παιχνιδιού και οι αναφορές δοκιμών βρίσκονται στις συζητήσεις του [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), ένα νήμα ανά τίτλο.

## Αντιστοίχιση εισόδου (Input mapping)

Υποστηρίζονται χειριστήρια παιχνιδιών με αντιστοίχιση SDL, συμπεριλαμβανομένων των αναλογικών μοχλών και των σκανδαλών. Ο έλεγχος με πληκτρολόγιο και ποντίκι μπορεί να ρυθμιστεί μέσω ενός αρχείου `anyps5-input.ini`. Δείτε την [αντιστοίχιση εισόδου](INPUT_MAPPING.md) για τις υποστηριζόμενες συσκευές και τη μορφή ρύθμισης.

## Αποποίηση ευθυνών

Αυτό το έργο προορίζεται για σκοπούς διαλειτουργικότητας, έρευνας, διατήρησης και συμβατότητας. Δεν περιλαμβάνει, δεν διανέμει και δεν απαιτεί λογισμικό που προστατεύεται από πνευματικά δικαιώματα, υλικολογισμικό (firmware), κρυπτογραφικά κλειδιά ή ιδιόκτητες βιβλιοθήκες. Οι χρήστες είναι υπεύθυνοι να διασφαλίζουν ότι οποιοδήποτε εκτελέσιμο αρχείο χρησιμοποιείται με αυτό το έργο έχει αποκτηθεί και χρησιμοποιείται σύμφωνα με τους ισχύοντες νόμους και τους αντίστοιχους όρους άδειας χρήσης.

Το AnyPS5 είναι ένα ανεξάρτητο έργο και δεν συνδέεται, δεν εξουσιοδοτείται και δεν υποστηρίζεται από τη Sony Interactive Entertainment. Τα «PlayStation» και «PS5» είναι εμπορικά σήματα της Sony Interactive Entertainment Inc.

## Άδεια χρήσης

Αυτό το έργο διανέμεται υπό την άδεια GNU General Public License έκδοση 2 μόνο.
