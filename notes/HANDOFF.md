# AnyPS5 — handoff

Documento di consegna. Chi riprende il lavoro dovrebbe leggere **solo questo**; il diario
dettagliato sta in `AnyPS5-STATO.md`, i testi delle PR in `PR-DESCRIPTIONS.md`.

---

## 1. Il progetto in due righe

`boykopovar/AnyPS5` **non è un emulatore**. È un *relinker statico*: converte un eseguibile PS5
in un binario nativo Linux/Windows, e fornisce **reimplementazioni in C++ delle librerie di
sistema** (`core/libs/prx`, HLE). Niente firmware PS5, niente chiavi: serve solo il dump
decrittato del gioco. GPL-2.0-only.

Il lavoro di questa sessione si concentra sul **traduttore `--to-intel`**, che sostituisce le
istruzioni x86 esclusive AMD (che il PS5 usa perché monta uno Zen 2) con sequenze eseguibili su
CPU Intel.

---

## 2. Dove sta tutto

| Cosa | Dove |
|---|---|
| Fork con il lavoro | <https://github.com/giannireale/AnyPS5> — `main` = `9fd2496`, 28 commit sopra l'upstream `75a8668` |
| Clone locale | `/home/user/AnyPS5` (branch `main`) |
| Branch per le PR | 11 branch `pr/*`, ricostruiti sopra l'upstream, uno per argomento |
| Branch di lavoro | 14 branch `feat/*` e `fix/*`, tutti **già contenuti in `main`**: ridondanti, cancellabili |
| Testi PR pronti | `notes/PR-DESCRIPTIONS.md` |
| Diario completo | `notes/STATO.md` |
| Censimento degli altri progetti | `notes/PORTING-SURVEY.md` |
| Modelli SHA-NI validati sul silicio | `/home/user/bench/sha-ni-reference-models.cpp` |
| Strumenti di misura | `/home/user/bench/` (solo sorgenti: `scan.cpp`, `ajmwrap.cpp`, `stubs.cpp`) |

I `.patch` sono stati rimossi: ogni commit e' sul fork, `git format-patch` li rigenera.

**Nessuna PR è stata aperta** verso l'upstream: scelta esplicita dell'utente, che vuole prima
provare di persona.

---

## 3. Cosa è stato fatto (20 commit)

### Traduttore `--to-intel` — copertura istruzioni
- `f7f12a4` INSERTQ forma register
- `c364270` MONITORX, MWAITX, MCOMMIT (in place) e CLZERO (stub)
- `faf2d83` `775ff3f` `fad796f` `44de58d` **tutte e sette le SHA-NI**, forma registro e memoria

### Traduttore — usabilità su codice reale (il blocco vero)
- `2a72195` rilocazione degli operandi RIP-relative assorbiti nello stub
- `e38ce96` assorbimento di salti, `ret`, trap e salti indiretti
- `8e16e9a` SHA-NI RIP-relative + `JCXZ`/`LOOP`
- `69a1834` la scansione non muore sulla coda del segmento
- `48786d5` si scandiscono le **sezioni** eseguibili, non l'intero segmento
- `35d19c0` i salti che entrano nel sito vengono rediretti dentro lo stub

### Librerie di sistema
- `da5237d` `libSceAjm` costruito come wrapper su `libSceAjm.native` (12 delle 13 funzioni)
- `9fd2496` `libSceNgs2`: ciclo di vita di sistema, rack e voci (15 delle 25)

### Strumenti
- `a7f5246` `tools/compat_report.py`: dato il registro degli import di un gioco, dice quante
  funzioni sono implementate, quante sono stub e quante il progetto non conosce

### Altro
- `dfb19a9` icona nel PE (`--icon`), solo stdlib
- `f45c558` stub `--to-intel` nei guest module (Linux + Windows)
- `fee8322` `f16e93c` 10 opcode MIMG nel recompiler shader
- `2e5fd44` correzione della **misura** della copertura shader (non della copertura)
- `a6bd2c6` encoder JPEG baseline: 4:2:2 e grayscale veri
- `5f5e972` coordinate baricentriche: rifiuto esplicito invece di modulo non valido
- `8f942de` `3e9fcf4` test che **esegue** un binario rilinkato

**10 voci di `docs/dev/TechnicalDebt.md` chiuse.**

---

## 4. Come ricostruire e testare (senza CMake)

Nella sandbox **non ci sono `cmake` né `ninja`**: tutto è stato costruito a mano con `g++ 14`.
I comandi qui sotto sono quelli realmente usati.

```sh
cd /home/user/AnyPS5

# elenco sorgenti estratto dal CMakeLists (si aggiorna da solo)
python3 -c "
import re
src=open('core/relinker/CMakeLists.txt').read()
m=re.search(r'add_executable\(relinker(.*?)\n\)',src,re.S).group(1)
open('/home/user/bench/rs.txt','w').write('\n'.join('core/relinker/'+f for f in re.findall(r'\\\$\{CMAKE_CURRENT_SOURCE_DIR\}/(\S+)',m)))
m2=re.search(r'add_executable\(amd64_only_converter_tests(.*?)\n\s*\)',src,re.S).group(1)
open('/home/user/bench/ts.txt','w').write('\n'.join('core/relinker/'+f for f in re.findall(r'\\\$\{CMAKE_CURRENT_SOURCE_DIR\}/(\S+)',m2)))"

INC="-Icore/relinker/cli/include -Icore/relinker/domain/include -Icore/relinker/elfpatcher/include \
     -Icore/relinker/io/include -Icore/relinker/codegen/include -Icore/relinker/relinker/include"

g++ -std=c++20 -O1 -o /home/user/bench/relinker $(cat /home/user/bench/rs.txt) $INC
g++ -std=c++20 -O1 -o /home/user/bench/amd64_tests $(cat /home/user/bench/ts.txt) \
    -Icore/relinker/domain/include -Icore/relinker/codegen/include \
    -Icore/relinker/elfpatcher/include -Icore/relinker/io/include
/home/user/bench/amd64_tests

cd core/relinker/relinker/tests
for t in test_*.py; do python3 $t /home/user/bench/relinker; done
```

Altri test:

```sh
# JPEG (serve il submodule stb solo per decodificare)
git submodule update --init --depth 1 3rdparty/stb
g++ -std=c++20 -O2 -o /tmp/jt core/Decoder/Jpeg/tests/Jpeg.cpp core/Decoder/Jpeg/src/Jpeg.cpp \
    core/Decoder/Jpeg/src/JpegEncoder.cpp -Icore/Decoder/Jpeg/include -isystem 3rdparty/stb && /tmp/jt

# decoder immagini RDNA (autonomo, non serve glslang)
R=core/shader/recompiler
g++ -std=c++20 -O1 -o /tmp/rd $R/tests/RdnaImageDecoderTests.cpp \
    $R/RdnaDecoder/src/RdnaImageOpDecoder.cpp $R/RdnaDecoder/src/RdnaOpcode.cpp \
    -I $R/RdnaDecoder/include -I $R && /tmp/rd

# il recompiler compila tutto senza glslang: basta SPIRV-Headers
git submodule update --init --depth 1 3rdparty/SPIRV-Headers
```

**Misura su binari reali** (è lo strumento che ha fatto emergere i bug più grossi):

```sh
g++ -std=c++20 -O1 -o /home/user/bench/scan /home/user/bench/scan.cpp \
  core/relinker/codegen/src/Amd64OnlyConverter.cpp core/relinker/codegen/src/InstructionScanner.cpp \
  core/relinker/codegen/src/x86/*.cpp core/relinker/io/src/BufferUtils.cpp \
  -Icore/relinker/domain/include -Icore/relinker/codegen/include -Icore/relinker/io/include

for f in /usr/bin/* /usr/lib/x86_64-linux-gnu/lib*.so.*; do /home/user/bench/scan "$f"; done
```

---

## 5. Stato di verifica, patch per patch

Questa tabella è la parte più importante del documento: dice **di cosa fidarsi**.

| Patch | Come è verificata | Forza |
|---|---|---|
| SHA-NI (7 istruzioni) | confronto con **l'istruzione nativa** su input casuali (la CPU ha `sha_ni`) | massima |
| INSERTQ, CLZERO | stub eseguito via `mmap`, confronto con modello + controllo registri/flag | alta |
| Stub Linux end-to-end | **binario rilinkato eseguito**, 5 scenari, exit code 42 | massima |
| Icona PE | albero risorse ispezionato + riletto con `pefile` | alta |
| JPEG 4:2:2/grayscale | header verificati + decodifica con **Pillow** (indipendente da stb) | alta |
| Guest module stub | pipeline reale, `.guest.prx` ispezionato su entrambi i target | alta |
| MIMG decoder | test con mutation check | media |
| Scansione/sezioni | **1257/1259 binari reali**, 405 MiB | alta |
| Baricentriche | **solo compilazione** + contratto di serializzazione | **bassa — dichiarata** |

Tutte le patch non banali hanno un **mutation test**: rompendo deliberatamente la logica, il test
fallisce. Quelle marcate "massima" usano un oracolo indipendente (hardware o altro software).

---

## 6. Cosa resta, e perché

### Risolvibile con lavoro
- **MIMG**: `IMAGE_MSAA_LOAD`, load/store `_PCK`, `ATOMIC_INC/DEC` (semantica wrap, serve un loop
  CAS), `BVH64_INTERSECT_RAY`.
- **Ricompilazione shader nel relinker** invece che a draw time: il pezzo architetturalmente più
  pesante, da concordare con l'autore upstream.

### Non risolvibile onestamente
- **`RDPRU`**: legge contatori di performance AMD. Qualunque valore sarebbe inventato.
- **`node`/`nodejs`** (2/1259): V8 mette dati dentro `.text`. Nessuna scansione lineare può
  distinguerli senza metadati; fallire con un errore chiaro è la risposta corretta.
- **Salto che entra a metà istruzione**, o con displacement a 8 bit che non raggiunge lo stub:
  il primo non è riscrivibile, il secondo richiederebbe di allungare l'istruzione sorgente.

### Fuori dalla portata di questo ambiente
- **prx al 73%** e **shader al 52%**: servono reverse engineering e una GPU. Il vero collo di
  bottiglia per far partire un gioco non è il traduttore. **Attenzione a leggere i badge**: le
  percentuali per libreria contano le directory `libSceX` e `libSceX.native` separatamente, e
  spesso l'implementazione vera sta nella `.native`. Esempio: `libSceAjm` risulta 0/13 ma
  `libSceAjm.native` ha 33 funzioni e decodifica ATRAC9 con LibAtrac9; `libSceAvPlayer.native`
  ha 631 righe. Genuinamente a zero sono `libSceAudiodec` e `libSceAudiodec.native` (AAC/MP3).

---

## 7. Trappole già pagate (non ripeterle)

1. **`/tmp` viene azzerato tra un comando e l'altro** nella sandbox: tenere gli strumenti in
   `/home/user/bench/`.
2. **Dopo un cherry-pick, verificare `git log -1` prima di committare**: una volta `main` locale
   è finito sull'upstream e un commit è nato sulla base sbagliata. Campanello: la lista dei test
   passata da 8 a 6 elementi.
3. **Un confronto fra due implementazioni può passare a vuoto**: tre codifiche ModRM di test
   usavano `xmm7` invece di `xmm3`, quindi stub e hardware lavoravano su un registro non
   osservato. L'ha scoperto il confronto con il *modello*, non con l'hardware: tenere due oracoli.
4. **`call` vs `jmp` nei banchi di prova**: il relinker entra negli stub con `jmp`, un harness che
   usa `call` sposta `rsp` di 8 byte e falsa gli operandi rsp-relative.
5. **`sub` scrive i flag**, `lea` no: usare `lea` per spostare `rsp` quando si verifica la
   preservazione dei flag.
6. **`CONVENTIONS.md` vieta i commenti nel codice** (eccetto marcatori di debito tecnico, `#endif`
   e fine namespace). Conventional Commits obbligatori.
7. **Le fixture ELF minime vanno controllate**: `e_entry` fuori dalla regione mappata dà un
   segfault che sembra un bug del relinker e non lo è.

---

## 8. Come valutare un gioco vero

```sh
relinker --registry --skip-sce-module --to-intel eboot.bin out.elf   # 1. converte e scrive il registro
python3 tools/compat_report.py out.registry.json --list              # 2. dice cosa manca
```

Il primo comando risponde a "il traduttore ce la fa?", il secondo a "le librerie ci sono?".
Nessuno dei due risponde a "il gioco parte": per quello servono GPU, audio e una prova sul campo.

## 9. Prossimi passi consigliati

1. **Provare il relinker su un eseguibile PS5 vero.** È l'unico test che manca e l'unico che
   nessuno in questa sessione poteva fare. Se fallisce, l'errore indica istruzione, indirizzo e
   motivo.
2. **Aprire le PR** partendo da `pr/insertq-register-form` e `pr/amd-system-instructions`, poi
   `pr/guest-intel-stubs` (maggiore impatto pratico) e `pr/sha-ni`. Per ultima quella su
   `progress.py`, spiegando che corregge la misura e non aggiunge copertura.
3. Se si continua a programmare: sul traduttore restano solo casi rari e documentati;
   il prossimo blocco vero è negli shader (`IMAGE_MSAA_LOAD`, load/store `_PCK`) o nelle prx.

---

## 10. Sicurezza

Il Personal Access Token usato per fork e push **è stato incollato in chat** e aveva scope molto
più ampi del necessario (`admin:org`, `admin:enterprise`, `delete_repo`, `workflow`, …).
**Va revocato**: <https://github.com/settings/tokens>. Nella sandbox non è stato salvato
(`.git/config` verificato pulito, nessun remote autenticato). Per operazioni future basta un
token classic con il solo scope `repo`.
