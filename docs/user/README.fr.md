[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | **Français** | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md)

# À propos

Outil de portage automatique d'exécutables vers Linux et Windows.

La compilation du relinker seul fonctionne sur macOS, y compris sur les puces Apple Silicon.

[**Convertisseur web**](https://boykopovar.github.io/AnyPS5/) - les fichiers restent sur votre appareil.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Comprend un [relinker](../../core/relinker) qui convertit l'exécutable au format natif du système cible, ainsi que des implémentations des [bibliothèques prx système](../../core/libs/prx) adaptées à l'édition de liens dynamique. Aucune émulation ni processus d'exécution séparé.

[**Discord**](https://discord.gg/x9naWVzC6A), [Utilisation](USAGE.md), [Instructions de compilation](../dev/BUILD.md), [Architecture](../dev/ARCHITECTURE.md), [Dette technique du projet](../dev/TechnicalDebt.md), [conventions de style de code](../dev/CONVENTIONS.md), [contribuer](../../CONTRIBUTING.md)

## État

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shader forms decoded, counted from the code](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Historique de la progression](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Bibliothèques système : pourcentage des fonctions connues du projet à ce jour (déclarées dans [core/libs/prx](../../core/libs/prx)), et non de toutes les fonctions système de la PS5. Le total augmente à mesure que de nouvelles fonctions sont déclarées.</sub>

Les états non pris en charge ou inattendus lèvent une exception C++, généralement `std::runtime_error`. Une exception qui n'est interceptée nulle part affiche son `what()` sur stderr et termine le processus.

Le [recompilateur de shaders](../../core/shader/recompiler/Recompiler.cpp) produit avec succès du SPIR-V (validé avec [Spirv-Tools](../../3rdparty/SPIRV-Tools) lorsque la compilation utilise `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Compatibilité

L'état de chaque jeu et les rapports de test se trouvent dans les discussions [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), avec un fil par titre.

## Configuration des contrôles

Les manettes reconnues par SDL sont prises en charge, y compris les sticks analogiques et les gâchettes. Les contrôles au clavier et à la souris se configurent avec un fichier `anyps5-input.ini`. Consultez la [configuration des contrôles](INPUT_MAPPING.md) pour les périphériques pris en charge et le format de configuration.

## Avertissement

Ce projet est destiné à des fins d'interopérabilité, de recherche, de préservation et de compatibilité. Il n'inclut, ne distribue ni ne nécessite aucun logiciel protégé par le droit d'auteur, firmware, clé cryptographique ou bibliothèque propriétaire. Il incombe aux utilisateurs de s'assurer que les binaires utilisés avec ce projet sont obtenus et utilisés conformément aux lois applicables et à leurs conditions de licence respectives.

AnyPS5 est un projet indépendant, qui n'est ni affilié à Sony Interactive Entertainment, ni autorisé ou approuvé par elle. « PlayStation » et « PS5 » sont des marques commerciales de Sony Interactive Entertainment Inc.

## Licence

Ce projet est distribué sous la licence publique générale GNU (GNU General Public License), version 2 uniquement.
