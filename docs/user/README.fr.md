[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | **Français** | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md) | [العربية](README.ar.md) | [Ελληνικά](README.el.md)

# A propos

Outil pour le portage automatique des exécutables vers Linux et Windows.

Le build "relinker-only" fonctionne sur macOS, incluant les Apple silicon.

[**Convertisseur web**](https://boykopovar.github.io/AnyPS5/) - les fichiers restent sur votre système.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Inclut un [relinker](../../core/relinker) qui convertit l'exécutable au format natif du système cible et des implémentations des [bibliothèques système prx ](../../core/libs/prx) compatibles avec la liaison dynamique. Aucune émulation ni processus d'exécution séparé.

[**Discord**](https://discord.gg/x9naWVzC6A), [Usage](USAGE.md), [Instructions de build](../dev/BUILD.md), [Architecture](../dev/ARCHITECTURE.md), [Dette technique du projet](../dev/TechnicalDebt.md), [conventions de style de code](../dev/CONVENTIONS.md), [contribuer](../../CONTRIBUTING.md)

## Statut

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![RDNA2 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![gfx1013 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders-gfx1013.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Historique de progrès](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Bibliothèques système : pourcentage des fonctions connues du projet à ce jour (déclarées dans [core/libs/prx](core/libs/prx)), et non de toutes les fonctions système de la PS5. Le total augmente à mesure que de nouvelles fonctions sont déclarées.</sub>

Les états non pris en charge ou inattendus lèvent une exception C++, généralement `std::runtime_error`. Une exception non interceptée affiche son résultat `what()` sur la sortie d'erreur standard et met fin au processus.

Le [recompilateur de shaders](../../core/shader/recompiler/Recompiler.cpp) produit avec succès SPIR-V (validé via [Spirv-Tools](../../3rdparty/SPIRV-Tools) lorsqu'il est construit avec `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Compatibilité

L'état et les rapports de test de chaque jeu se trouvent dans les discussions [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), un fil de discussion par titre.

## Mappage des contrôles

Les manettes de jeu compatibles SDL sont prises en charge, notamment les joysticks analogiques et les gâchettes. Les commandes clavier et souris peuvent être configurées à l'aide d'un fichier `anyps5-input.ini`. Consultez la section [Mappage des contrôles](INPUT_MAPPING.md) pour connaître les périphériques compatibles et le format de configuration.

## Clause de non-responsabilité

Ce projet vise à assurer l'interopérabilité, la recherche, la préservation et la compatibilité. Il n'inclut, ne distribue ni ne requiert aucun logiciel, micrologiciel, clé cryptographique ou bibliothèque propriétaire protégé par le droit d'auteur. Il incombe aux utilisateurs de s'assurer que les fichiers binaires utilisés dans le cadre de ce projet sont obtenus et utilisés conformément aux lois applicables et à leurs conditions de licence respectives.

AnyPS5 est un projet indépendant et n'est ni affilié à, ni autorisé par, ni approuvé par Sony Interactive Entertainment. « PlayStation » et « PS5 » sont des marques déposées de Sony Interactive Entertainment Inc.

## Licence

Ce projet est distribué sous licence GNU GPL version 2 uniquement.
