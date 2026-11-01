[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | **Italiano** | [简体中文](README.zh-CN.md) | [Русский](README.ru.md)

# Informazioni

Strumento per il porting automatico di eseguibili su Linux e Windows.

La compilazione del solo relinker funziona su macOS, inclusi i chip Apple Silicon.

[**Convertitore web**](https://boykopovar.github.io/AnyPS5/) - i file rimangono sul tuo dispositivo.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Include un [relinker](../../core/relinker) che converte l'eseguibile nel formato nativo del sistema di destinazione e implementazioni delle [librerie prx di sistema](../../core/libs/prx) adatte al linking dinamico. Nessuna emulazione o processo di runtime separato.

[**Discord**](https://discord.gg/BHFztBPUe), [Utilizzo](USAGE.md), [Istruzioni di compilazione](../dev/BUILD.md), [Architettura](../dev/ARCHITECTURE.md), [Debito tecnico del progetto](../dev/TechnicalDebt.md), [convenzioni sullo stile del codice](../dev/CONVENTIONS.md), [come contribuire](../../CONTRIBUTING.md)

## Stato

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shaders](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Cronologia dei progressi](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Librerie di sistema: percentuale delle funzioni note al progetto fino ad ora (dichiarate in [core/libs/prx](../../core/libs/prx)), non di tutte le funzioni di sistema di PS5. Il totale cresce man mano che vengono dichiarate più funzioni.</sub>

Gli stati non supportati o imprevisti lanciano rigorosamente `std::runtime_error`. Il messaggio `what()` viene stampato su stderr e il processo termina.

Il [recompilatore di shader](../../core/shader/recompiler/Recompiler.cpp) genera con successo codice SPIR-V (validato tramite [Spirv-Tools](../../3rdparty/SPIRV-Tools) se compilato con `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Compatibilità

Lo stato di ciascun gioco e i report dei test si trovano nelle discussioni di [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), un thread per ciascun titolo.

## Mappatura dei comandi

I controller con mappatura SDL sono supportati, comprese levette analogiche e grilletti. I controlli con tastiera e mouse possono essere configurati tramite un file `anyps5-input.ini`. Consulta la [mappatura dei comandi](INPUT_MAPPING.md) per i dispositivi supportati e il formato di configurazione.

## Clausola di esclusione della responsabilità

Questo progetto è destinato a scopi di interoperabilità, ricerca, conservazione e compatibilità. Non include, distribuisce né richiede software protetto da copyright, firmware, chiavi crittografiche o librerie proprietarie. Gli utenti sono responsabili di garantire che tutti i file binari utilizzati con questo progetto siano ottenuti e utilizzati in conformità con le leggi applicabili e i rispettivi termini di licenza.

AnyPS5 è un progetto indipendente e non è affiliato, autorizzato o approvato da Sony Interactive Entertainment. "PlayStation" e "PS5" sono marchi registrati di Sony Interactive Entertainment Inc.

## Licenza

Questo progetto è concesso in licenza esclusivamente sotto la GNU General Public License versione 2.
