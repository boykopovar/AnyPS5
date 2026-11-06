# AnyPS5 — stato del lavoro

Ultimo aggiornamento: 2026-09-29 (22 commit su main)

## Cos'è il progetto (da non fraintendere)

`boykopovar/AnyPS5` **non è un emulatore**. È un *relinker* statico che converte gli eseguibili PS5
in binari nativi Linux/Windows, più implementazioni delle librerie di sistema `prx` per il linking
dinamico. Nessuna emulazione, nessun processo runtime separato. Licenza GPL-2.0-only.

Struttura utile:

| Percorso | Contenuto |
|---|---|
| `core/relinker/` | relinker: CLI, parsing ELF, patcher ELF/PE, codegen x86 |
| `core/relinker/codegen/src/x86/` | conversione istruzioni AMD-only (`--to-intel`) |
| `core/libs/prx/` | implementazioni librerie di sistema (562 file `.cpp` circa) |
| `core/shader/recompiler/` | ricompilatore shader → SPIR-V |
| `docs/dev/TechnicalDebt.md` | **lista dei lavori aperti, punto di partenza migliore** |
| `docs/dev/CONVENTIONS.md` | convenzioni di stile (vedi sotto) |

## Lavoro completato — 1. INSERTQ register form

Voce del debito tecnico chiusa: la **forma register di INSERTQ** (`F2 0F 79 /r`) non veniva
abbassata da `--to-intel`; la lowering lanciava
`CodegenException("INSERTQ register form has no Intel lowering")` e il relink falliva.
INSERTQ è SSE4a, esclusiva AMD: qualunque titolo che la usa era non portabile su CPU Intel.

### Implementazione

`core/relinker/codegen/src/x86/Sse4aLowering.cpp` — nuova `_emitInsertqRegisterForm`, stub
out-of-line in sole istruzioni SSE2, 3 registri scratch salvati sotto la red zone:

```
movdqa s0, src ; psrldq s0,9 ; pand s0,[63]      -> index  (bit 77:72 di src)
movdqa s1, src ; psrldq s1,8 ; pand/pxor/paddq   -> 64-len (bit 69:64, len 0 = 64)
pcmpeqd s2,s2 ; psrlq s2,s1 ; psllq s2,s0        -> maschera allineata all'indice
movdqa s1,src ; psllq s1,s0 ; pand s1,s2         -> valore in posizione
pandn s2,dst ; por s2,s1 ; movsd dst,s2          -> merge
```

Punti non ovvi:

- `len == 0` significa 64: risolto con il trucco `xor 63` + `paddq 1` + `pand 63`, lo stesso
  già usato dal path EXTRQ esistente.
- `psrlq`/`psllq` **non attraversano le corsie a 64 bit** (uno shift ≥ 64 azzera): per portare
  giù i byte 8 e 9 di `src` serve `psrldq` (`66 0F 73 /3 ib`), che shifta l'intero registro a byte.
- Il `movsd` finale (`F2 0F 10 /r`) scrive solo il quadword basso e **preserva quello alto**
  della destinazione — meglio del path immediato con `pshufb`, che invece lo azzera.
- Bonus incluso: deduplica delle costanti nel `BodyBuilder`; lo stub della forma register di
  EXTRQ scende da 224 a 160 byte.

Altri file toccati:

- `Amd64OnlyInstructionMatcher.cpp` — la forma register di INSERTQ non è più `Unsupported`
  ma `Trampoline`, sia in `Match` sia in `MatchSequence`.
- `Amd64OnlyConverterTests.cpp` — asserzione obsoleta aggiornata + **nuovo test che esegue
  davvero lo stub generato** (`mmap` con `PROT_EXEC` + asm inline) su 8 combinazioni
  length/index, confrontandolo con un modello di riferimento; verifica anche che gli scratch
  non vengano sporcati e che il quadword alto sopravviva, con operandi distinti e con `xmm2,xmm2`.
- `docs/dev/TechnicalDebt.md` — rimosso INSERTQ dalla riga delle istruzioni non abbassate.

### Dove sta il codice

- Commit `f7f12a4` — `feat(relinker): lower the INSERTQ register form for --to-intel`
- Fork: <https://github.com/giannireale/AnyPS5> — sia `main` sia `feat/insertq-register-form`
  puntano a questo commit
- Upstream `boykopovar/AnyPS5`: **nessun permesso di push** (`{"push": false, "pull": true}`),
  l'unica strada è una Pull Request — non ancora aperta
- Patch standalone: `/home/user/insertq-register-form.patch` (applicabile con `git am`)
- Clone locale: `/home/user/AnyPS5` (branch `feat/insertq-register-form`, ormai non più shallow)

## Lavoro completato — 2. Icona nell'eseguibile PE (`--icon`)

Seconda voce del debito tecnico chiusa: il relinker non inseriva alcuna icona nel PE generato,
e il vincolo era farlo **senza nuove dipendenze, solo stdlib**.

### Implementazione

Nuovo `WindowsResourceBuilder` (`elfpatcher/{include,src}/windows/WindowsResourceBuilder.*`):

- parsing dell'`ICONDIR` di un file `.ico` (validazione stretta: reserved, type == 1, count > 0,
  offset/size dentro il file → altrimenti `RelinkerException`)
- ogni immagine diventa una risorsa **RT_ICON** (tipo 3), id `1..n`, lingua 1033
- viene sintetizzata una **GRPICONDIR** come **RT_GROUP_ICON** (tipo 14) id 1: è la struttura che
  Windows e Explorer cercano per scegliere la dimensione giusta
- albero risorse a 3 livelli (tipo → nome → lingua), `IMAGE_RESOURCE_DATA_ENTRY` con **RVA assoluti**,
  payload allineati a 4, il tutto in una nuova sezione `.rsrc` agganciata alla **data directory 2**
- i payload delle immagini sono copiati verbatim, quindi funzionano sia DIB sia PNG (icone Vista+)

Integrazione: `WindowsPePatcher(bool windowsGui, std::vector<std::uint8_t> icon)`, CLI `--icon <path.ico>`
(errore esplicito se usata senza `--windows`), lettura del file in `main.cpp` via `Io::FileReader`
— l'I/O resta ai bordi, il patcher riceve byte.

### Verifica

- Nuovo `relinker/tests/test_windows_icon.py`, registrato in CMake come test `windows_icon`:
  gira la **pipeline reale** ELF→PE e poi naviga l'albero risorse del PE prodotto, confrontando
  ogni payload RT_ICON con l'immagine sorgente e ogni voce GRPICONDIR (dimensioni, lunghezza, id).
  Copre anche i casi negativi: file cursore (`type == 2`) rifiutato, `--icon` senza `--windows` rifiutato,
  e assenza di `.rsrc`/directory 2 vuota quando l'opzione non è usata.
- Verifica indipendente con **pefile**: l'albero viene letto correttamente come
  `RT_ICON` id 1/2/3 + `RT_GROUP_ICON` id 1, lingua 1033, subsystem 2 con `--windows-gui`.
- Le 5 suite python preesistenti restano verdi.

Commit `dfb19a9`, branch `feat/pe-icon`, anche su `main` del fork. Patch: `/home/user/pe-icon.patch`.

Comando per ricostruire il relinker senza CMake (le sorgenti si estraggono da `core/relinker/CMakeLists.txt`):

```sh
python3 - <<'EOF'
import re
src=open('core/relinker/CMakeLists.txt').read()
m=re.search(r'add_executable\(relinker(.*?)\n\)',src,re.S).group(1)
open('/tmp/relinker_sources.txt','w').write('\n'.join('core/relinker/'+f for f in re.findall(r'\$\{CMAKE_CURRENT_SOURCE_DIR\}/(\S+)',m)))
EOF
g++ -std=c++20 -O1 -o /tmp/relinker $(cat /tmp/relinker_sources.txt) \
  -Icore/relinker/cli/include -Icore/relinker/domain/include -Icore/relinker/elfpatcher/include \
  -Icore/relinker/io/include -Icore/relinker/codegen/include -Icore/relinker/relinker/include
cd core/relinker/relinker/tests && for t in test_*.py; do python3 $t /tmp/relinker; done
```

## Lavoro completato — 3. Copertura MIMG (shader recompiler)

Nove opcode MIMG in più, scelti dove il recompiler era già strutturalmente pronto.

### Scoperta importante sulla metrica

Il badge "shaders" **sottostima MIMG**: `tools/progress.py` conta i nomi degli enum in
`RdnaOpcode.hpp` contro `tools/rdna_isa.txt`, ma il decoder collassa tutte le varianti
`IMAGE_SAMPLE_*` in un unico `RdnaOpcode::ImageSample` più un bitmask di flag. Quindi
`IMAGE_SAMPLE_C`, `_B`, `_LZ`... risultano "todo" pur essendo decodificate da sempre.
La copertura reale di MIMG è più alta del 17,7% mostrato.

### Cosa è stato aggiunto

- **Gather**: `IMAGE_GATHER4` (0x40) e `IMAGE_GATHER4_O` (0x50) — nuovi enum `ImageGather4`,
  `ImageGather4O`; riusano l'emitter flag-driven esistente
- **Atomiche con segno**: `IMAGE_ATOMIC_SUB` (0x12), `SMIN` (0x14), `SMAX` (0x16) →
  nuovi IrOpcode `ImageAtomicISub32/SMin32/SMax32` → `OpAtomicISub`, `OpAtomicSMin`, `OpAtomicSMax`
- **Sample shadow con offset**: `IMAGE_SAMPLE_C_D_O` (0x3a), `C_L_O` (0x3c), `C_B_O` (0x3d),
  `C_LZ_O` (0x3f) — sono PCF su shadow map con offset, comunissime; pure combinazioni di flag
  già supportate, invisibili al badge ma copertura reale
- **Correzione**: `EmitGatherOp` ora **rifiuta** un gather a LOD implicito fuori da un pixel
  shader invece di emettere un `OpImageGather` non valido (prima il caso non si presentava
  perché ogni gather aveva il flag LZ)

Scartati di proposito, perché richiederebbero approssimazioni silenziose (vietate dalla policy):
`GATHER4_L`/`_B` (servirebbe `SPV_AMD_texture_gather_bias_lod`), `ATOMIC_INC`/`DEC`
(semantica wrap senza equivalente SPIR-V diretto), `CMPSWAP` (due dword di dato),
tutte le varianti `_CL` (lod clamp, già rifiutato da `validateFlags`).

Effetto sul badge: shaders **585 → 590 / 1166**, MIMG 23 → 28.

### Verifica

- Nuovo `core/shader/recompiler/tests/RdnaImageDecoderTests.cpp`, registrato in CMake come test
  `rdna_image_decoder` (target autonomo: dipende solo da `RdnaImageOpDecoder.cpp` + `RdnaOpcode.cpp`,
  quindi **non richiede glslang**, che nella sandbox non è disponibile).
  Verifica opcode, flag di indirizzo, numero di componenti e di dword, più i casi negativi
  (dmask multi-bit sul gather, opcode riservato 0x13, `ATOMIC_INC` non implementato, varianti `_CL`).
- **Mutation test**: rimuovendo la riga 0x50 dalla tabella il test fallisce — le asserzioni mordono.
- I 7 file toccati compilano con `g++ -fsyntax-only` (serve solo il submodule SPIRV-Headers,
  inizializzabile con `git submodule update --init --depth 1 3rdparty/SPIRV-Headers`).

Commit `fee8322`, branch `feat/mimg-coverage` (poi `f16e93c` e `2e5fd44` su `feat/mimg-atomics`), anche su `main` del fork. Patch: `/home/user/mimg-coverage.patch`.

Comando test decoder:

```sh
R=core/shader/recompiler
g++ -std=c++20 -O1 -o /tmp/rdna_image_tests $R/tests/RdnaImageDecoderTests.cpp \
  $R/RdnaDecoder/src/RdnaImageOpDecoder.cpp $R/RdnaDecoder/src/RdnaOpcode.cpp \
  -I $R/RdnaDecoder/include -I $R && /tmp/rdna_image_tests
```

### Mappa degli encoding MIMG (utile per continuare)

Blocchi da 8, ordine interno: base, `cl`, `d`, `d_cl`, `l`, `b`, `b_cl`, `lz`.
`0x20` sample, `0x28` sample_c, `0x30` sample_o, `0x38` sample_c_o,
`0x40` gather4, `0x48` gather4_c, `0x50` gather4_o, `0x58` gather4_c_o.
Atomiche: `0x0f` swap, `0x10` cmpswap, `0x11` add, `0x12` sub, `0x14` smin, `0x15` umin,
`0x16` smax, `0x17` umax, `0x18` and, `0x19` or, `0x1a` xor, `0x1b` inc, `0x1c` dec.

## Lavoro completato — 4. IMAGE_ATOMIC_CMPSWAP

Compare-and-swap atomica su immagine, mappata **esattamente** su `OpAtomicCompareExchange`.

Il punto delicato è il DMASK: l'ISA impone `0x3` per la cmpswap a 32 bit, perché valore e
comparatore stanno in due registri dati consecutivi (`VDATA[0]` = nuovo valore, `VDATA[1]` = comparatore).
Il decoder ora applica quella regola: `0x3` obbligatorio per la cmpswap, singolo bit per tutte
le altre atomiche a 32 bit, forma a 64 bit (`0xf`) ancora rifiutata.

Nuovo `IrOpcode::ImageAtomicCompareSwap32` con un operando in più rispetto alle altre atomiche
(`ImageResource, ImageAddress, U32, U32, U1`), nuovo `EmitCompareSwapOp` nel backend SPIR-V.
Commit `f16e93c`.

## Lavoro completato — 5. Correzione della metrica shader

`tools/progress.py` derivava la copertura dal solo enum `RdnaOpcode`. Ma il decoder immagini
**collassa tutte le varianti `IMAGE_SAMPLE_*` in `RdnaOpcode::ImageSample` più un bitmask di flag**,
quindi encoding decodificati da tempo risultavano mancanti.

Ora lo script legge anche la tabella `imageOpcodes` di `RdnaImageOpDecoder.cpp` e accredita le voci
con nome esplicito **i cui flag il decoder accetta davvero**: le varianti `_CL` (lod clamp), `_CD`
(coarse derivative) e `_A` (adjust) restano nei todo perché `validateFlags` continua a rifiutarle.

Nessun cambiamento di comportamento del decoder: è solo la misura che diventa onesta.
**MIMG 29 → 47 su 130**, shaders 591 → 609/1166. Commit `2e5fd44`.

I 18 opcode riconosciuti sono tutte varianti sample già funzionanti: `IMAGE_SAMPLE_C`, `_B`, `_L`,
`_D`, `_O`, `_C_LZ`, `_C_D_O`, `_C_L_O`, `_C_B_O`, `_C_LZ_O`, `_B_O`, `_D_O`, `_L_O`, `_LZ_O`, ...

## Lavoro completato — 6. Istruzioni di sistema AMD per `--to-intel`

Chiusa quasi tutta la riga del debito tecnico su MONITORX/MWAITX/CLZERO/RDPRU/MCOMMIT.
Resta solo RDPRU.

Nuovo `codegen/{include,src}/x86/SystemInstructionLowering.*`:

| Istruzione | Lowering | Perché è corretto |
|---|---|---|
| MONITORX (`0F 01 FA`) | NOP a 3 byte, in place | arma solo un monitor d'indirizzo, nessun effetto architetturale osservabile |
| MWAITX (`0F 01 FB`) | `PAUSE` + NOP, in place | **MWAIT può risvegliarsi per qualsiasi motivo**: tornare subito è legale, la coppia degrada a spin |
| MCOMMIT (`F3 0F 01 FA`) | `MFENCE` + `CLC`, in place (4 byte esatti) | CLC segnala il successo che il chiamante testa su CF |
| CLZERO (`0F 01 FC`) | stub | serve mascherare l'indirizzo, vedi sotto |
| RDPRU | resta `Unsupported` | legge MPERF/APERF: nessun equivalente Intel, restituire zero sarebbe una bugia silenziosa |

Il punto chiave è MWAITX: non è un'approssimazione ma una **implementazione legale**, perché
l'architettura ammette risvegli spuri. Per questo non serve un marcatore di debito tecnico.

Stub CLZERO (93 byte), non tocca né registri né flag:

```
lea rsp,[rsp-0x90]      ; salta la red zone
pushfq ; push rax
and rax, -64            ; inizio della cache line
8 × mov qword [rax+disp8], 0
pop rax ; popfq
lea rsp,[rsp+0x90]
```

### Verifica

- `matcherSubstitutions` aggiornato per tutte e cinque le istruzioni; i test che usavano MONITORX
  come esempio di "non supportata" ora usano RDPRU.
- **Nuovo `clzeroExecution()`**: esegue davvero lo stub su un buffer `alignas(64)` di 192 byte
  riempito di 0xAA, con `rax` a un indirizzo non allineato (buffer+70), e verifica che siano
  azzerati esattamente i 64 byte della linea, che `rax` torni intatto e che **CF sopravviva**
  (impostato con `stc` prima della call).
  Nota sul test: il setup usa `lea` invece di `sub` per spostare `rsp`, perché `sub` scrive i flag
  e falsava la verifica — ci ho sbattuto contro al primo giro.
- Suite `amd64_only_converter` verde, più le 6 suite python del relinker.

Commit `c364270`, branch `feat/amd-system-instructions`, anche su `main`.
Patch: `/home/user/amd-system-instructions.patch`.

## Lavoro completato — 7. Stub `--to-intel` nei guest module

Era la voce più *funzionale* rimasta: `GuestModuleBuilder` rifiutava qualunque lowering che
richiedesse uno stub, quindi **`--to-intel` falliva su qualsiasi gioco con `sce_module` che usi**
la forma register di EXTRQ/INSERTQ, CLZERO o un sito SSE4a corto. Con le patch precedenti il
numero di lowering che producono stub è cresciuto, quindi il blocco pesava sempre di più.

Modifiche:

- estratto `AppendTrampoline` da `LinuxElfPatcher` in `elfpatcher/general/TrampolineWriter.*`
  (il patcher ora delega, comportamento identico)
- `GuestImage` porta i propri `Trampolines`; `GuestModuleBuilder` li conserva invece di lanciare
- `LinuxGuestModuleWriter`: stub in testa all'extra block, che è già `PT_LOAD` con flag 7 (RWX)
- `WindowsGuestModuleWriter`: riusa `WindowsTrampolineBuilder`, lo stesso che genera `.amdstub`
  per l'eseguibile principale — zero codice nuovo sul lato PE

### Verifica

Nuovo `relinker/tests/test_guest_intel_stub.py` (test ctest `guest_intel_stub`), integrazione vera
su **entrambi i target**: crea un `sce_module/` con un guest ELF contenente `66 0F 79 CA`
(EXTRQ register form), lancia il relinker con `--to-intel` e poi ispeziona il `.guest.prx` prodotto.

- Linux: il sito è sostituito da `E9`, il displacement viene risolto e si verifica che l'indirizzo
  di destinazione **cada in un segmento eseguibile**, che lo stub inizi con il salto della red zone
  e contenga il jmp di ritorno.
- Windows: il PE guest contiene la sezione `.amdstub`.

Dettagli scoperti scrivendo il test (utili se lo si estende):
- il guest reader pretende `DT_HASH`, serve una tabella hash minima (`nbucket=1, nchain=2`)
- lo scanner decodifica *tutto* il segmento eseguibile: la fixture deve terminare con un byte
  che decodifichi (ho messo `0x90` in fondo), altrimenti "instruction truncated"
- l'eseguibile principale va scelto per target: la fixture di `test_linux_load_alignment` per Linux
  (servono 5 slot di program header), quella di `test_optional_plt` per Windows

Commit `f45c558`, branch `feat/guest-intel-stubs`, anche su `main`.
Patch: `/home/user/guest-intel-stubs.patch`.

## Lavoro completato — 8/9/10. SHA-NI completa (7 istruzioni su 7)

### Metodo: differential testing contro il silicio

**La CPU della sandbox ha `sha_ni`**, quindi ogni stub è confrontato con l'istruzione vera sugli
stessi input casuali. Niente vettori NIST: è una verifica più forte, perché isola la singola
istruzione invece di guardare solo il digest finale.

`/home/user/sha_ni_model_check.cpp` contiene i modelli di riferimento delle sette istruzioni,
validati sul silicio (200.000 confronti). Il modello di SHA1NEXTE era sbagliato al primo tentativo
(valore E nel dword **alto** `127:96`): l'hardware l'ha isolato subito.

### Le sette lowering

| Istruzione | Encoding | Strategia | Stub |
|---|---|---|---|
| SHA1NEXTE | `0F 38 C8` | `pslld`/`psrld`/`por` per rol30, maschera lane alta, `paddd` | 112 B |
| SHA1MSG1 | `0F 38 C9` | `pslldq`/`psrldq` per allineare le lane, `pxor` | 83 B |
| SHA1MSG2 | `0F 38 CA` | rol1 su tutte le lane, poi seconda passata per W19 che dipende da W16 | 149 B |
| SHA256MSG1 | `0F 38 CC` | sigma0 = rol25 ^ rol14 ^ shr3 | 196 B |
| SHA256MSG2 | `0F 38 CD` | sigma1 su lane azzerate: **nessuna maschera serve**, sigma(0)=0 | 257 B |
| SHA1RNDS4 | `0F 3A CC /r ib` | 4 round scalari su GPR spillati in un frame sotto la red zone | 396-460 B |
| SHA256RNDS2 | `0F 38 CB` | 2 round su GPR, `xmm0` implicito salvato nel frame | 603 B |

I due round sono codice scalare: `StubBodyBuilder` è stato esteso con un emettitore GPR
(`AdjustStack`, `StoreQword`/`LoadDword`, `GprBinary`, `GprNot`, `GprRotate`, `GprAddImmediate`,
`PushFlags`/`PopFlags`). Il frame è 0x100 byte, quindi salta la red zone di 128; i flag sono
salvati perché l'aritmetica dei round li scrive.

### Verifica

- banco di prova `/tmp/shadev.cpp` (ricreabile): 512 input per istruzione, stub vs hardware
- suite ufficiale: `shaNiExecution()` per le quattro message schedule, `shaRoundExecution()` per
  i due round (4 varianti di immediato + RNDS2), confronto **sia con i modelli sia con l'hardware**
  (`__builtin_cpu_supports("sha")` a runtime, encoding `.byte` così compila senza `-msha`), più
  verifica che i flag sopravvivano (`stc` prima della call)
- mutation test superati: `rol30`→`rol29` in RNDS4 e `ror11`→`ror12` in RNDS2 fanno fallire

Commit `faf2d83`, `775ff3f`, `fad796f` sul branch `feat/sha-ni` e su `main`.
Patch unica: `/home/user/sha-ni-complete.patch`.

Resta fuori solo la forma **RIP-relative** (vedi patch 11) e **RDPRU**.

## Lavoro completato — 11. Forma con operando in memoria delle SHA-NI

Lo stub carica l'operando con `movdqu` in un registro scratch e poi **riusa invariata** la forma
registro: tutte e sette le lowering restano identiche.

- i byte ModRM/SIB/displacement originali vengono ricopiati nella `movdqu`, quindi base e indice
  restano validi (lo stub si raggiunge con un `jmp` e non ha ancora toccato i GPR)
- operando **rsp-relative**: ri-codificato con `mod=10` e displacement + dimensione del frame (0x90)
- operando **RIP-relative**: rifiutato con errore esplicito, perché lo stub sta a un altro indirizzo
  e il displacement non e' ricalcolabile a questo livello (servirebbe il supporto del patcher)
- limite noto e benigno: `movdqu` accetta indirizzi non allineati che l'istruzione nativa
  potrebbe rifiutare — la lowering e' piu' permissiva, mai meno

### Due errori di test da ricordare (nessuno dei due nel codice prodotto)

1. **`call` contro `jmp`**: il banco di prova invoca lo stub con `call`, che spinge 8 byte di
   indirizzo di ritorno; il relinker ci arriva con `jmp`. Per gli operandi rsp-relative questo
   sposta l'indirizzo di 8 byte. Nel test il payload viene duplicato 8 byte piu' in basso.
2. **Confronto vacuo**: le prime codifiche ModRM che avevo scritto (`0x3F`, `0x7F`, `0x4C`) avevano
   nel campo reg xmm7/xmm1 invece di xmm3. Stub e hardware operavano entrambi su un registro che
   non osservavo, quindi il confronto passava senza verificare niente. **L'ha scoperto il confronto
   con il modello di riferimento**, non quello con l'hardware: e' la ragione per cui in suite ci
   sono entrambi.

Mutation test: togliendo la compensazione del displacement sullo stack la suite fallisce.

Commit `44de58d`, branch `feat/sha-ni` e `main`. Patch: `/home/user/sha-ni-memory-form.patch`.

## Lavoro completato — 12. Encoder JPEG: 4:2:2 e grayscale veri

`libSceJpegEnc` rispondeva a una richiesta 4:2:2 con un 4:2:0 e a una grayscale con un JPEG a
**tre** componenti a croma neutra: stb sa scrivere solo quello. Erano risposte sbagliate date in
silenzio, quindi contro la policy del progetto.

Nuovo `core/Decoder/Jpeg/src/JpegEncoder.cpp`: encoder JPEG baseline completo, **solo stdlib**
(tabelle di quantizzazione Annex K scalate per qualita', tabelle di Huffman standard, DCT
separabile, bitstream con byte stuffing, loop MCU con fattori di campionamento variabili,
replica dei bordi per gli MCU parziali). La decodifica continua a passare da stb.

`Decoder::Jpeg::Encode` prende ora un `Sampling` (`Full`, `Ycc422`, `Ycc420`); `libSceJpegEnc`
gli passa quello richiesto dal chiamante, sceglie 1 canale per la grayscale e **non taglia piu'
la qualita' a 90** (era un aggiramento del writer stb).

### Verifica

- `decoder_jpeg` (test gia' esistente, esteso): controlla i fattori di campionamento nel SOF0
  (`0x11`, `0x21`, `0x22`), che la grayscale abbia **una** componente, e che il roundtrip
  encode/decode degradi in modo monotono (full <= 422 <= 420) con dimensioni decrescenti
- verifica indipendente con **Pillow**: tutte e quattro le varianti si aprono, mode `L` per la
  grayscale, PSNR 42.3 / 38.3 / 29.5 / 28.6 dB
- mutation test: rimettendo il bug originale (4:2:2 codificato come 4:2:0) la suite fallisce

Per eseguire i test in locale serve il submodule stb (solo per la decodifica):
`git submodule update --init --depth 1 3rdparty/stb`

```sh
g++ -std=c++20 -O2 -o /tmp/jpeg_tests core/Decoder/Jpeg/tests/Jpeg.cpp \
  core/Decoder/Jpeg/src/Jpeg.cpp core/Decoder/Jpeg/src/JpegEncoder.cpp \
  -Icore/Decoder/Jpeg/include -isystem 3rdparty/stb && /tmp/jpeg_tests
```

Commit `a6bd2c6`, branch `feat/jpeg-sampling` e `main`. Patch: `/home/user/jpeg-sampling.patch`.

## Lavoro completato — 13. Coordinate baricentriche: rifiuto esplicito

Voce della sezione **"stub silenziosi"** del debito. Il flag `fragmentShaderBarycentricEnabled`
arrivava al traduttore (che genera gli input baricentrici) ma **non al backend SPIR-V**:
`SpirvTargetOptions` non aveva il campo. Risultato: il backend emetteva
`CapabilityFragmentBarycentricKHR` + `SPV_KHR_fragment_shader_barycentric` **senza controllare
il device**, al contrario di ogni altra capability opzionale del progetto (BDA, descriptor
indexing, tessellation controllano tutte).

Su un device senza quell'estensione il driver rifiuta il modulo: crash o schermo nero a runtime,
invece di un errore di ricompilazione.

Modifiche: campo aggiunto a `SpirvTargetOptions` e a `SpirvEmitterState`, popolato in
`Recompiler.cpp`, e in `SpirvModuleSetup` il ramo baricentrico ora lancia
`std::runtime_error` se manca l'enable, la capability o l'estensione.

### Verifica — piu' debole delle altre, e va detto

- i 99 sorgenti del recompiler compilano (glslang non serve: basta il submodule SPIRV-Headers)
- `BdaContracts.cpp` esteso: verifica che il flag sopravviva al round-trip di serializzazione
  della richiesta
- **non ho potuto eseguire la nuova diagnostica**: servirebbe costruire un `IrProgram` pixel con
  input baricentrico e un `BindingAllocationResult`, oppure una GPU. A differenza delle patch
  precedenti qui non c'e' un oracolo: la logica e' semplice e modellata su `SpirvBdaRead.cpp`,
  ma resta da provare sul campo.

Commit `5f5e972`, branch `fix/barycentric-target` e `main`. Patch: `/home/user/barycentric-target.patch`.

## Lavoro completato — 14. Esecuzione reale di un eseguibile rilinkato

Ultima voce del relinker nel debito: *"il piazzamento degli stub Linux e' coperto solo da un test
sintetico"*. Chiusa nel modo piu' forte possibile: **il binario rilinkato viene eseguito**.

Scoperta abilitante: un ELF prodotto dal relinker **gira su questa macchina**. La fixture di
`test_linux_load_alignment` aveva `e_entry` fuori dalla regione mappata (il `PT_LOAD` mappa il file
0x4000 a vaddr 0), quindi il primo tentativo segfaultava; sistemato l'entry, il binario parte.

`test_linux_stub_execution.py` costruisce un eseguibile PIE minimo il cui **exit code nasce dalla
forma register di EXTRQ**, lo rilinka con `--to-intel` e lo lancia: se esce 42, allora il salto
nello stub, il corpo dello stub, l'istruzione assorbita (il sito e' piu' corto di un jmp) e il
salto di ritorno funzionano tutti davvero. Fuori da Linux x86-64 il test ripiega sull'ispezione.

Mutation test: con uno shift sbagliato nella lowering EXTRQ il binario esce 0 e il test fallisce.

Commit `8f942de`, branch `feat/linux-stub-execution` e `pr/linux-stub-execution`.

### Due incidenti da ricordare

Durante la creazione dei branch `pr/*` il `main` locale e' finito **sull'upstream**, e un commit
e' nato sulla base sbagliata. Nessuna perdita (il lavoro era gia' sul fork), ma la lezione e':
dopo i cherry-pick verificare sempre `git log --oneline -1` **prima** di committare. Il campanello
d'allarme e' stato la lista dei test python passata da 8 a 6 elementi.

## Lavoro completato — 16. Operandi RIP-relative assorbiti nello stub (usabilita')

Il blocco pratico piu' grosso del traduttore `--to-intel`. Un'istruzione AMD-only piu' corta di
5 byte non ha spazio per un `jmp rel32`, quindi il converter assorbe nello stub le istruzioni
seguenti — ma **rifiutava qualunque istruzione con operando RIP-relative**, che nel codice SSE
compilato e' ovunque (tipicamente il caricamento di una costante). Risultato: relink fallito su
una classe enorme di siti reali.

Ora `TrampolineSite` porta l'indirizzo assoluto di ogni operando RIP-relative assorbito
(`Fixups`), e **entrambi** i writer di trampolini — ELF e PE — ricalcolano il displacement
dall'indirizzo in cui lo stub finisce davvero. I salti continuano a fermare l'assorbimento:
il loro target richiederebbe lo stesso trattamento nella direzione opposta.

Catena: decoder (`RipRelativeDispOffset`, gia' esistente) -> converter (calcola il target
assoluto) -> matcher (espone `TrailingOffset`, dove inizia il trailing nello stub) -> writer
(riscrive i 4 byte).

### Verifica

`test_linux_stub_execution.py` ha ora un secondo scenario: EXTRQ (4 byte) seguito da
`paddd xmm0, [rip+addend]` (8 byte, RIP-relative). Il binario rilinkato **viene eseguito** e
restituisce 42, cioe' il campo estratto piu' l'addendo letto dalla memoria *dall'interno dello
stub*. Mutation test: disattivando l'applicazione dei fixup il binario segfaulta (-11).

Commit `2a72195`, branch `feat/rip-relative-absorption`; il branch di revisione
`pr/rip-relative-absorption` coincide con `main` perche' la patch tocca componenti introdotti
da quasi tutti i commit precedenti.

### Cosa fa ancora fallire il relink (stato del traduttore)

1. **RDPRU** — nessun equivalente Intel, per scelta
2. **SHA-NI con operando RIP-relative** — il displacement andrebbe rilocato dal patcher
3. **Sito corto seguito da un salto** — il target del salto andrebbe rilocato
4. **Salto che entra dentro un'istruzione AMD-only**
5. Istruzione AMD-only a fine segmento senza istruzioni successive

I punti 2 e 3 sono ora gli unici davvero comuni, e hanno entrambi la stessa forma: rilocare un
displacement. L'infrastruttura dei `Fixups` appena aggiunta e' la base su cui risolverli.

## Lavoro completato — 17. Assorbimento di salti e ritorni (usabilita', punto 3)

Completa il lavoro precedente. Un sito corto pretendeva che le istruzioni successive fossero
codice sequenziale puro; restavano quindi non rilinkabili forme comunissime:
AMD-only seguita da un `ret`, da un `jmp` corto che scavalca un blocco, o da una chiamata indiretta.

- **salto relativo diretto** (`jmp`/`jcc`/`call`): riscritto nella forma a 32 bit dentro lo stub e
  rilocato con la **stessa lista di fixup** degli operandi RIP-relative
- **`ret`, trap, salti indiretti**: spostati invariati, non portano displacement verso il codice
  che hanno lasciato (prima venivano rifiutati per nulla)
- **`jcc` condizionale**: corretto su entrambi i rami — preso raggiunge il target originale, non
  preso cade nel salto di ritorno, che e' esattamente l'istruzione che lo seguiva
- **`JCXZ`/`LOOP`**: continuano a fallire, non hanno forma a 32 bit

### Verifica

Terzo scenario nel test di esecuzione: `EXTRQ` seguito da `jmp short` che scavalca un
`mov edi, 99`. Il binario rilinkato **gira e restituisce 42**: se la rilocazione del salto fosse
sbagliata finirebbe nel codice morto o in mezzo al nulla. Mutation test: lasciando `0xEB`
(forma corta) invece di allargare a `0xE9`, il binario segfaulta.

Nella suite C++ il vecchio `requireFailure` su "EXTRQ corto seguito da un return" e' diventato
un'asserzione positiva: il `ret` finisce nello stub e non serve alcun fixup.

Commit `e38ce96`, branch `feat/branch-absorption`.

### Stato del traduttore `--to-intel`

Fanno ancora fallire il relink, in ordine di frequenza attesa:

1. **SHA-NI con operando RIP-relative** (il displacement va rilocato dal patcher)
2. `JCXZ`/`LOOP` dopo un sito corto
3. Salto che entra **dentro** un'istruzione AMD-only
4. `RDPRU` (per scelta)
5. Istruzione AMD-only a fine segmento

Tutto il resto del codice x86-64 reale che segue un sito corto ora e' gestito.

## Lavoro completato — 18. Ultimi casi di fallimento del traduttore

### Punto 1 — SHA-NI con operando RIP-relative: RISOLTO

Lo stub mantiene la forma RIP-relative e **dichiara** il displacement che gli serve; il converter
lo trasforma in un target assoluto e i writer lo ricalcolano. Catena nuova:
`LoweredBody.RipFixups` (offset nel corpo, displacement originale, fine istruzione) ->
`Amd64OnlyMatch` -> `TrampolineSite.Fixups`.

Bug trovato strada facendo nel decoder SHA-NI: per `mod=00, rm=101` **non contava i 4 byte di
displacement**, quindi l'operando veniva troncato. Il test di esecuzione l'ha smascherato subito
(segfault), un test di sola ispezione no.

### Punto 2 — JCXZ/LOOP dopo un sito corto: RISOLTO

Non hanno forma a 32 bit, e riscriverli con `test`+`jz` **cambierebbe i flag**, che quelle
istruzioni non devono toccare. Soluzione senza effetti collaterali: l'istruzione originale resta,
e sceglie solo tra un salto corto che scavalca il salto largo e il salto largo stesso.

```
E3 02      ; jrcxz +2   (oppure E0/E1/E2 per LOOP*)
EB 05      ; jmp +5     ramo non preso
E9 rel32   ; ramo preso, target rilocato
```

### Punto 3 — Salto che entra dentro un sito: NON risolto, diagnostica migliorata

La soluzione vera sarebbe riscrivere il **salto sorgente** perche' punti nello stub: richiede una
nuova classe di fixup applicata a un'istruzione lontana dal sito, con il rischio che un `rel8` non
arrivi. Non l'ho fatta: l'avrei consegnata senza poterla provare su codice reale.
Ora almeno l'errore dice chi e' il colpevole:

```
Branch at 0x10 enters the AMD-only site at 0x12 (target 0x14), so the site cannot be replaced by a jump
```

### Punto 4 — RDPRU: resta fuori per scelta, con spiegazione nell'errore

### Punto 5 — Istruzione a fine segmento: messaggio che spiega il perche'

Si potrebbe assorbire *all'indietro* l'istruzione precedente (simmetrico a quello in avanti), ma
il caso si presenta solo se il segmento finisce esattamente li': non vale il rischio.

### Verifica

Il test di esecuzione ha ora **quattro scenari**, tutti eseguiti sull'host: operando RIP-relative
assorbito, salto assorbito e allargato, SHA-NI con operando RIP-relative, JRCXZ. Ognuno restituisce
42; ognuno, se la rilocazione e' sbagliata, restituisce un valore diverso o segfaulta.

Commit `8e16e9a`, branch `feat/relocation-completeness`.

## Lavoro completato — 19. Prova sul campo: 1259 binari reali

Per rispondere a "e' utilizzabile?" ho smesso di usare fixture mie e ho dato in pasto al
converter **tutti i binari ELF del sistema** (`/usr/bin`, `/usr/lib`), cioe' 519 MiB di codice
x86-64 compilato da compilatori veri.

### Risultato iniziale: 478 fallimenti su 1259 (38%)

Tutti con lo stesso errore, e tutti **negli ultimi byte del segmento eseguibile**: la scansione
lineare arriva sui byte di allineamento che seguono l'ultima funzione, quelli non formano
un'istruzione intera, e il relink falliva.

Correzione (`69a1834`): la scansione si ferma se il decoder fallisce negli ultimi 15 byte **e**
gli stessi byte decodificano una volta riempiti di padding — cioe' esattamente il caso
dell'istruzione tagliata dalla fine del segmento. Un opcode sconosciuto continua a far fallire
il relink ovunque si trovi (lo verifica la suite C++, che aveva colto una prima versione della
correzione troppo permissiva).

**Dopo: 3 fallimenti su 1259 (0,2%)** — `node`, `nodejs`, `libLLVM`, tutti su una tabella dati
dentro il segmento eseguibile, dove la scansione lineare esce di passo. Limite noto e strutturale
del linear sweep, non un bug del decoder.

### Prova di copertura: libcrypto.so.3

3,6 MiB di codice OpenSSL che usa SHA-NI sul serio:

| | |
|---|---|
| istruzioni SHA-NI trovate dal converter | **512** |
| istruzioni SHA-NI contate da objdump | **512** |
| stub prodotti | 415 (97 contengono una catena di piu' istruzioni) |
| dimensione degli stub | 154 KiB su 3,6 MiB di codice (~4%) |

Il conteggio coincide con un disassemblatore indipendente: **nessun sito perso**.

### Cosa questo prova e cosa no

Prova: il decoder regge codice compilato reale su larga scala, il traduttore trova tutti i siti
e gli stub generati girano (i quattro scenari di esecuzione).
Non prova: **nessun eseguibile PS5 vero e' mai passato per questa pipeline** — non ne ho uno.
E un relink riuscito non significa un gioco che parte: quello dipende dalle prx.

## Lavoro completato — 20. Scansione per sezione: 1257/1259

Con la scansione limitata alla coda del segmento restavano 3 fallimenti. La causa vera era piu'
seria di quanto sembrasse: **un `PT_LOAD` eseguibile spesso mappa anche i dati in sola lettura**
(tabella dei simboli, rilocazioni, hash) *prima* del codice. In `libLLVM` l'errore era a `0x8a1d0`,
cioe' molto prima di `.init` a `0xcf46d8`: stavamo decodificando la tabella dei simboli come se
fossero istruzioni.

Non era solo robustezza: una sequenza di byte dentro una tabella **poteva essere scambiata per
un'istruzione AMD-only e patchata**, corrompendo i dati.

`ElfReader::ReadCodeSegments` ora, quando l'immagine ha gli header di sezione, restituisce solo le
sezioni marcate eseguibili; se non ci sono (guest module strippati) resta il comportamento
precedente.

| | prima | dopo |
|---|---|---|
| binari analizzati senza errori | 1256/1259 | **1257/1259** |
| byte dati in pasto al decoder | 519 MiB | **405 MiB** |

Il caso residuo sono `node`/`nodejs` (lo stesso binario): V8 mette lo **snapshot embedded dentro
`.text`**, cioe' dati in una sezione eseguibile. Nessuna scansione lineare puo' distinguerli senza
metadati; la risposta giusta resta fallire con un errore chiaro.

Commit `48786d5`, branch `feat/section-aware-scan`.

## Lavoro completato — 21. Salti che entrano nel sito: risolti

Era l'ultimo blocco strutturale del traduttore, e l'avevo rimandato due volte perche' "non
verificabile". Con l'esecuzione dei binari rilinkati lo e' diventato.

Quando un salto punta **all'inizio di un'istruzione che lo stub conserva** (una di quelle
assorbite perche' il sito era piu' corto di 5 byte), il sito registra dove quell'istruzione
finisce nel corpo dello stub, e **entrambi** i writer riscrivono il displacement del salto
entrante perche' ci arrivi.

Restano due casi che falliscono, ciascuno con una diagnostica che nomina il salto:
- target **in mezzo** a un'istruzione: nessuna riscrittura e' possibile
- salto con displacement a **8 bit**: non raggiunge lo stub, e allargarlo cambierebbe la
  lunghezza dell'istruzione sorgente

### Verifica, e come ho sbagliato il test due volte

Primo tentativo: il salto tornava *indietro* su un'istruzione precedente, creando un ciclo
infinito, e il target cadeva a meta' istruzione. Secondo problema: con un `nop` come istruzione
assorbita il test **passava anche senza la correzione**, perche' saltare sul padding dava lo
stesso risultato.

Scenario definitivo, che discrimina: il salto entra su un `paddd xmm0, xmm2` assorbito, che
aggiunge 2 al valore estratto. Con la riscrittura esce **42**, senza esce **40** (o segfault,
come ha mostrato il mutation test).

Commit `35d19c0`, branch `feat/incoming-branch-redirect`.

## Lavoro completato — 22. `tools/compat_report.py`

Risponde alla domanda "questo gioco e' portabile?" incrociando due informazioni che il progetto
aveva gia' ma non collegava: il **registro degli import** che il relinker scrive con `--registry`
(NID + libreria per ogni funzione chiamata) e le **funzioni dichiarate** in `core/libs/prx`.

Il NID si calcola dal nome con SHA-1 piu' un suffisso fisso: ho portato in Python l'algoritmo di
`core/libs/nid` e **verificato che coincida sulle 2789 funzioni dichiarate, zero differenze**.

Output su un registro sintetico:

```
imported functions: 92
  implemented: 65
  silent stub: 25
  unknown to the project: 2
  ready: 70.7%

per library:
  libSceAgc.prx        12 to write
  libSceAgcDriver.prx   8 to write
```

Con `--list` stampa i NID mancanti uno per uno. Commit `a7f5246`, branch `feat/compat-report`.

## Branch pronti per le pull request

I branch `feat/*` e `fix/*` sono **impilati** su `main` del fork: una PR da uno di essi
trascinerebbe anche i commit precedenti. Per questo esistono i branch **`pr/*`**, ricostruiti con
cherry-pick sopra l'upstream `75a8668`, uno per argomento:

| PR | Branch | Base | Commit |
|---|---|---|---|
| 1 | `pr/insertq-register-form` | upstream | 1 |
| 2 | `pr/amd-system-instructions` | dopo la 1 | 2 |
| 3 | `pr/sha-ni` | dopo la 2 | 6 |
| 4 | `pr/guest-intel-stubs` | upstream | 1 |
| 5 | `pr/pe-icon` | upstream | 1 |
| 6 | `pr/jpeg-sampling` | upstream | 1 |
| 7 | `pr/mimg-coverage` | upstream | 1 |
| 8 | `pr/mimg-atomics` | dopo la 7 | 3 |
| 9 | `pr/barycentric-target` | upstream | 1 |
| 10 | `pr/linux-stub-execution` | dopo la 4 | 2 |
| 11 | `pr/rip-relative-absorption` | = `main`, stack completo | 16 |

Tre sono impilati per dipendenze **di codice reali** (SHA-NI usa lo `StubBodyBuilder` estratto
dalla patch sulle istruzioni di sistema, che a sua volta tocca il matcher modificato da INSERTQ).
I conflitti di cherry-pick erano quasi tutti su `docs/dev/TechnicalDebt.md`, risolti prendendo la
riga finale voluta da ciascun commit.

**Ogni branch `pr/*` e' stato ricompilato e testato da solo sull'upstream**, non solo impilato:
relinker + suite C++ + 7 suite python per quelli del relinker, decoder RDNA per i MIMG,
`decoder_jpeg` per il JPEG, compilazione del recompiler per il baricentrico.

Testi delle PR pronti in inglese: `/home/user/PR-DESCRIPTIONS.md` (titolo, corpo e nota su come
ciascuna e' stata verificata, inclusa l'ammissione esplicita per la PR 9).

**Non ho aperto le PR**: l'utente aveva escluso le PR verso l'upstream.

## Ambiente della sandbox

- **`cmake` e `ninja` NON sono installati**, `g++ 14.2` e `python3` sì. La build ufficiale via
  CMake non è replicabile qui.
- I submodule in `3rdparty/` non sono stati scaricati: SDL2, Vulkan-Headers, SPIRV-Tools,
  glslang, freetype, stb, LibAtrac9. Servono per `core/libs` e per il recompiler shader,
  **non** per il relinker (che usa solo la stdlib C++20).
- Il clone iniziale era `--depth 20`; per pushare è servito `git fetch --unshallow`, altrimenti
  GitHub rifiuta con *shallow update not allowed*.

Comando che compila ed esegue la suite `amd64_only_converter` senza CMake (da `/home/user/AnyPS5`):

```sh
g++ -std=c++20 -O1 -o /tmp/amd64_tests \
  core/relinker/codegen/tests/Amd64OnlyConverterTests.cpp \
  core/relinker/codegen/src/Amd64OnlyConverter.cpp \
  core/relinker/codegen/src/InstructionScanner.cpp \
  core/relinker/codegen/src/x86/X64InstructionDecoder.cpp \
  core/relinker/codegen/src/x86/X64InstructionRewriter.cpp \
  core/relinker/codegen/src/x86/DecodedInstruction.cpp \
  core/relinker/codegen/src/x86/Amd64OnlyInstructionMatcher.cpp \
  core/relinker/codegen/src/x86/Sse4aOperands.cpp \
  core/relinker/codegen/src/x86/Sse4aLowering.cpp \
  core/relinker/elfpatcher/src/linux/LinuxElfPatcher.cpp \
  core/relinker/elfpatcher/src/general/SegmentFilter.cpp \
  core/relinker/elfpatcher/src/general/EntryStubBuilder.cpp \
  core/relinker/elfpatcher/src/general/ProgramHeaderLayoutBuilder.cpp \
  core/relinker/elfpatcher/src/general/SectionHeaderTableBuilder.cpp \
  core/relinker/io/src/ByteWriter.cpp core/relinker/io/src/BufferUtils.cpp \
  -Icore/relinker/domain/include -Icore/relinker/codegen/include \
  -Icore/relinker/elfpatcher/include -Icore/relinker/io/include
/tmp/amd64_tests   # atteso: "AMD64-only converter tests passed"
```

Esito: **verde** sia prima sia dopo la patch. Il test di esecuzione richiede host
Linux x86-64 (è dietro `#if defined(__linux__) && defined(__x86_64__)`).

## Convenzioni del progetto (rispettarle nelle prossime patch)

- PascalCase per classi e metodi pubblici, camelCase per membri privati e argomenti,
  interfacce con prefisso `I`, template `TKey`/`TValue`.
- **I commenti nel codice sono vietati**, salvo marcare debito tecnico (solo da umano),
  fine `#endif` e fine namespace.
- Conventional Commits.
- Ogni stato inatteso deve lanciare `std::runtime_error`: niente stub silenziosi.

## Prossimi candidati (dal debito tecnico)

1. **Ricompilazione shader spostata nella fase di relink** — oggi avviene appena prima del
   trasferimento a Vulkan; è il lavoro architetturalmente più pesante.
2. MIMG: restano `IMAGE_MSAA_LOAD`, le load/store `_PCK`, `ATOMIC_INC/DEC` (semantica wrap,
   servirebbe un loop CAS), le atomiche float, `BVH64_INTERSECT_RAY` e le varianti gather con
   LOD/bias esplicito (servirebbe `SPV_AMD_texture_gather_bias_lod`).
3. `--to-intel`: resta RDPRU (nessun equivalente Intel) e le SHA-NI con operando RIP-relative
   (servirebbe far rilocare il displacement al patcher, che oggi non lo prevede).
4. Il path *length-changing* di `X64InstructionRewriter` non è usato dal converter: non
   aggiusta operandi RIP-relative VEX/0F38/0F3A, riferimenti dati→codice, dimensioni dei
   segmenti. Lavoro grosso e delicato.
5. Encoder JPEG: mancano ancora MJPEG e il restart interval (entrambi lanciano esplicitamente).

## Nota di sicurezza

Il Personal Access Token usato per fork e push è stato incollato in chat e aveva scope molto
più ampi del necessario (`admin:org`, `admin:enterprise`, `delete_repo`, `workflow`, ...).
**Va revocato**: <https://github.com/settings/tokens>. Nella sandbox non è stato salvato
(nessuna credenziale in `.git/config`, nessun remote autenticato). Per operazioni future
servirà un token nuovo: classic con il solo scope `repo`, oppure fine-grained limitato al
repo già forkato.
