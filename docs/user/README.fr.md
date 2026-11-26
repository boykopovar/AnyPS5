[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | **Français** | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md) | [العربية](README.ar.md) | [Ελληνικά](README.el.md) | [Türkçe](README.tr.md) | [עברית](README.he.md) | [Deutsch](README.de.md)

# À propos

Outil de portage automatique d'exécutables vers Linux et Windows.

La compilation en mode relinker seul fonctionne sur macOS, y compris sur Apple silicon.

[**Convertisseur web**](https://boykopovar.github.io/AnyPS5/) - les fichiers restent sur votre appareil.

Inclut un [relinker](../../core/relinker) qui convertit l'exécutable au format natif du système cible ainsi que des implémentations des [bibliothèques prx système](../../core/libs/prx) adaptées à l'édition de liens dynamique. Aucune émulation ni processus d'exécution séparé.

[**Discord**](https://discord.gg/x9naWVzC6A), [Utilisation](USAGE.md), [Instructions de compilation](../dev/BUILD.md), [Architecture](../dev/ARCHITECTURE.md), [Dette technique du projet](../dev/TechnicalDebt.md), [conventions de style de code](../dev/CONVENTIONS.md), [contribuer](../../CONTRIBUTING.md)

## État

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![RDNA2 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![gfx1013 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders-gfx1013.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Historique de progression](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Bibliothèques système : pourcentage des fonctions connues du projet jusqu'à présent (déclarées dans [core/libs/prx](../../core/libs/prx)), et non de toutes les fonctions système de la PS5. Le total augmente à mesure que de nouvelles fonctions sont déclarées.</sub>

Les états non pris en charge ou inattendus lèvent une exception C++, généralement `std::runtime_error`. Une exception non interceptée affiche son `what()` sur `stderr` et termine le processus.

Le [recompilateur de shaders](../../core/shader/recompiler/Recompiler.cpp) produit avec succès du SPIR-V (validé via [Spirv-Tools](../../3rdparty/SPIRV-Tools) lors d'une compilation avec `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Compatibilité

L'état par jeu et les rapports de test se trouvent dans les discussions [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), un fil par titre.

## Mappage des entrées

Les manettes mappées par SDL sont prises en charge, y compris les sticks analogiques et les gâchettes. Les contrôles clavier et souris peuvent être configurés avec un fichier `anyps5-input.ini`. Consultez [mappage des entrées](INPUT_MAPPING.md) pour connaître les périphériques pris en charge et le format de configuration.

## Avertissement

Ce projet est destiné à des fins d'interopérabilité, de recherche, de préservation et de compatibilité. Il n'inclut, ne distribue ni ne requiert aucun logiciel protégé par le droit d'auteur, firmware, clé cryptographique ou bibliothèque propriétaire. Les utilisateurs sont responsables de s'assurer que tout binaire utilisé avec ce projet est obtenu et utilisé conformément aux lois applicables et aux conditions de licence respectives.

AnyPS5 est un projet indépendant et n'est ni affilié à, ni autorisé ou approuvé par Sony Interactive Entertainment. « PlayStation » et « PS5 » sont des marques déposées de Sony Interactive Entertainment Inc.

## Licence

Ce projet est sous licence GNU General Public License version 2 uniquement.
