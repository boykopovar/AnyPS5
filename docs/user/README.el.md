[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | **Ελληνικά**

# Σχετικά

Εργαλείο για την αυτόματη μεταφορά εκτελέσιμων αρχείων σε Linux και Windows.

Το build μόνο του relinker τρέχει σε macOS, και σε Apple silicon.

[**Μετατροπέας web**](https://boykopovar.github.io/AnyPS5/) - τα αρχεία μένουν στη συσκευή σας.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Περιλαμβάνει έναν [relinker](../../core/relinker) που μετατρέπει το εκτελέσιμο στην εγγενή μορφή του συστήματος προορισμού, και υλοποιήσεις των [βιβλιοθηκών prx συστήματος](../../core/libs/prx) κατάλληλες για δυναμική σύνδεση. Χωρίς εξομοίωση και χωρίς ξεχωριστή διεργασία runtime.

[**Discord**](https://discord.gg/BHFztBPUe), [Χρήση](USAGE.md), [Οδηγίες build](../dev/BUILD.md), [Αρχιτεκτονική](../dev/ARCHITECTURE.md), [Τεχνικό χρέος του έργου](../dev/TechnicalDebt.md), [συμβάσεις στυλ κώδικα](../dev/CONVENTIONS.md), [συνεισφορά](../../CONTRIBUTING.md)

## Κατάσταση

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shader forms decoded, counted from the code](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Ιστορικό προόδου](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Βιβλιοθήκες συστήματος: ποσοστό επί των συναρτήσεων που γνωρίζει ως τώρα το έργο (δηλωμένων στο [core/libs/prx](../../core/libs/prx)), όχι επί όλων των συναρτήσεων συστήματος του PS5. Το σύνολο μεγαλώνει όσο δηλώνονται περισσότερες συναρτήσεις.</sub>

Οι μη υποστηριζόμενες ή απρόσμενες καταστάσεις εγείρουν εξαίρεση C++, συνήθως `std::runtime_error`. Μια εξαίρεση που δεν τη συλλαμβάνει τίποτα τυπώνει το `what()` της στο stderr και τερματίζει τη διεργασία.

Ο [shader recompiler](../../core/shader/recompiler/Recompiler.cpp) παράγει επιτυχώς SPIR-V (που επικυρώνεται με το [Spirv-Tools](../../3rdparty/SPIRV-Tools) όταν το build γίνεται με `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Συμβατότητα

Η κατάσταση κάθε παιχνιδιού και οι αναφορές δοκιμών βρίσκονται στις συζητήσεις [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), ένα νήμα ανά τίτλο.

## Αντιστοίχιση εισόδου

Υποστηρίζονται χειριστήρια παιχνιδιών που αντιστοιχίζονται μέσω SDL, μαζί με αναλογικούς μοχλούς και σκανδάλες. Οι χειρισμοί πληκτρολογίου και ποντικιού ρυθμίζονται με ένα αρχείο `anyps5-input.ini`. Δείτε την [αντιστοίχιση εισόδου](INPUT_MAPPING.md) για τις υποστηριζόμενες συσκευές και τη μορφή του αρχείου ρυθμίσεων.

## Αποποίηση ευθύνης

Αυτό το έργο προορίζεται για σκοπούς διαλειτουργικότητας, έρευνας, διατήρησης και συμβατότητας. Δεν περιλαμβάνει, δεν διανέμει και δεν απαιτεί λογισμικό που προστατεύεται από πνευματικά δικαιώματα, firmware, κρυπτογραφικά κλειδιά ή ιδιόκτητες βιβλιοθήκες. Οι χρήστες είναι υπεύθυνοι να διασφαλίζουν ότι κάθε δυαδικό αρχείο που χρησιμοποιείται με αυτό το έργο έχει αποκτηθεί και χρησιμοποιείται σύμφωνα με την ισχύουσα νομοθεσία και τους αντίστοιχους όρους άδειας χρήσης.

Το AnyPS5 είναι ανεξάρτητο έργο και δεν συνδέεται με τη Sony Interactive Entertainment, ούτε έχει την εξουσιοδότηση ή την έγκρισή της. Τα «PlayStation» και «PS5» είναι εμπορικά σήματα της Sony Interactive Entertainment Inc.

## Άδεια χρήσης

Αυτό το έργο διατίθεται με την GNU General Public License, αποκλειστικά στην έκδοση 2.
