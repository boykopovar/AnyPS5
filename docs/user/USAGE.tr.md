# Relinker kullanımı

[**Web dönüştürücü**](https://boykopovar.github.io/AnyPS5/) - dosyalar cihazınızda kalır.

### Tarayıcıda klasör çıktısı

Çevrimiçi dönüştürücü, desteklenen tarayıcılarda Linux çıktısını sıkıştırılmamış bir klasör olarak kaydedebilir. Var olan bir hedef klasörün üzerine yazmaz. Dönüştürme başarısız olursa ya da iptal edilirse, etkin dosyayı durdurur ve yeni oluşturulan çıktı klasörünü silmeye çalışır. Tarayıcı temizliği engellerse, hata mesajı eksik dosyalar içerebilecek klasörü belirtir.

### Tarayıcıda toplu dönüştürme

Birden fazla uygulama ZIP arşivini seçin veya sürükleyip bırakın; hepsi tek bir ZIP paketine dönüştürülür. Dönüştürücü her uygulamayı ayrı inceler ve çalıştırılabilir dosyasını, kütüphanelerini ve `app0/` dosyalarını numaralandırılmış bir klasöre koyar; böylece aynı dosya adına sahip arşivler birbirinden ayrı kalır. Toplu çıktı ZIP modunu kullanır; sıkıştırılmamış klasöre kaydetme ise aynı anda yalnızca bir Linux uygulaması için kullanılabilir. 256 MiB'lık bellekte indirme sınırı paketin tamamı için geçerlidir, bu yüzden daha büyük toplu işlemlerde **Save ZIP to disk** (ZIP'i diske kaydet) seçeneğini kullanın. Toplu işlem iptal edilirse ya da herhangi bir uygulama başarısız olursa dönüştürücü eksik paketi iptal eder; temizlik sırasında bir hata olursa bu, tamamlanmış sonuç olarak sunulmak yerine bildirilir.

## Sürüm indirmesini doğrulama

Dosyayı [resmi AnyPS5 sürümlerinden](https://github.com/boykopovar/AnyPS5/releases) indirin. İlgili sürümün **Assets** (Varlıklar) bölümünde, tam dosya adının yanındaki `sha256:` özetini kopyalayın. Dosya başarıyla indirildiyse, açmadan önce SHA-256 karmasını hesaplayın:

- Windows PowerShell: `Get-FileHash -Algorithm SHA256 -LiteralPath "path\to\downloaded-file"`
- Linux: `sha256sum -- "path/to/downloaded-file"`

- macOS: `shasum -a 256 -- "path/to/downloaded-file"`

Karmayı `sha256:` sonrasındaki kısımla, harf büyüklüğüne bakmadan karşılaştırın. Farklıysa dosyayı atın. Eşleşen bir karma, dosyanın yayımlanan varlıkla aynı olduğunu doğrular.

Bir tarayıcı veya antivirüs indirmeyi engellerse, engelli bırakın. Sürüm bağlantısını, dosya adını, uyarının tam metnini ve uyarıyı gösteren tarayıcıyı ya da antivirüsü bildirin. Dosya mevcutsa boyutunu bayt olarak ve karmasını da ekleyin.

## Girdi ve dönüştürme

Relinker paket dosyalarını çıkarmaz ve şifrelerini çözmez. Çıkarılmış, şifresi çözülmüş bir ELF çalıştırılabilir dosyasına ve onunla gelen modüllere ihtiyaç duyar.

### SELF boyut sınırı

SELF açıcı, toplamda 64 MiB'tan fazla hizalama dolgusu gerektiren bir kapsayıcıyı reddeder. Bunu çıktı arabelleğini ayırmadan önce kontrol eder. Sınır, segment verisini değil, dosyaya bağlı ve boş segmentlerdeki dolguyu sayar. Reddederse girdiniz ve var olan çıktı dosyası olduğu gibi kalır.

### Dosyaları nereye koymalı

Temiz bir ELF çalıştırılabilir dosyası kullanın. Onunla gelen ELF modüllerini girdi çalıştırılabilir dosyasının yanındaki `sce_module/`, `sce_modules/` veya `prx/` klasörüne ya da `--sce-module-path <path>` ile seçilen üst dizinin altına koyun.

- `--sce-module-path` mutlak yolları ve geçerli çalışma dizinine göre göreli yolları kabul eder. `sce_module` dizininin kendisini değil, üst dizini verin.
- Yalnızca modül dizinlerinin doğrudan içindeki dosyalar işlenir.
- SELF/fSELF kapsayıcıları bir uyarıyla atlanır. Dahil edilmeleri gerekiyorsa relink etmeden önce açın.
  - `tools/unwrap_self.py <input.self> <output.elf>` şifresi çözülmüş bir kapsayıcı için bunu yapar. Segmentleri SELF segment kayıtlarının ofsetlerine yerleştirir ve `p_offset % p_align == p_vaddr % p_align` koşulunu geri yükler. Yükleme segmentlerinin hepsi şifreli görünen bir kapsayıcıyı reddeder.
- Yedek kopyalar, hem komut satırında hem de çevrimiçi dönüştürücüde bir mesajla atlanır. Bir ELF modülünün dosya adı aynı dizindeki başka bir ELF modülünün adını uzatıyorsa ve ikisinin SONAME'i aynıysa yedek sayılır. Örnek: bir dump aracının, ELF `libX.prx` dosyasının yanına bıraktığı `libX.prx.esbak` veya `libX.prxbak`. Bir SELF kapsayıcısının yanındaki tek bir yedek ise yine relink edilir.
- `prx/`, `sce_module/` ya da `sce_modules/` ile birlikte bulunabilir.
- Hem `sce_module/` hem `sce_modules/` bulunması bir hatadır.
- Üçünün de bulunmaması, açık bir `--module-dir` vermediğiniz sürece hatadır.

### Linux, macOS ve Windows'ta modül başlatma

Çalıştırılabilir dosya `sceKernelLoadStartModule` fonksiyonunu import ediyorsa, ne çalıştırılabilir dosyanın ne de gerekli bir modülün ihtiyaç duymadığı bir paket modülü açılışta başlatılmaz. Oyun onu yüklediğinde, `sceKernelLoadStartModule` ile gelen argümanlarla başlatılır. Bu import yoksa, paketle gelen tüm modüller açılışta başlar. macOS'ta bir ertelenmiş modülün bu şekilde yüklenmesi denenmemiştir.

### Yerleşim ve komut

```text
source/
    input.elf
    sce_module/
        <bundled ELF modules>
```

```text
relinker [options] <input.elf> <output>
```

### Deneme çalıştırması (dry run)

`relinker --dry-run [options] <input.elf> <output>`, paketle gelen modüller, ek çalıştırılabilir dosyalar, import analizi ve bayt yamaları dahil dönüştürmeyi bellekte çalıştırır.

- Planlanan her çıktıyı listeler: registry dosyaları, gerekli kütüphane dosyaları, yama manifestoları, Windows tanılama dosyaları ve Linux masaüstü meta verileri.
- Hiçbir dizin oluşturmaz, hiçbir dosya yazmaz ve hiçbir izni değiştirmez. Var olan çıktılar korunur.
- Dönüştürme hataları, normal bir çalıştırmadaki gibi `2` döndürür.
- Çıktı yolu yine de gereklidir, çünkü yerleşimi o belirler.
- Hedefe yazma erişimini denetlemez ve henüz var olmayan dosyaların dosya sistemi takma adlarını yoklamaz. Bu denetimleri yalnızca normal bir dönüştürme yapar.
- `--dry-run`, `--autorun` ve `--verify-patches` ile çakışır.

### Standart adı olmayan modül dizinleri

Modül üst dizininde `sce_module`, `sce_modules` veya `prx` dizini yoksa da açık bir `--module-dir` çalışır. Çalıştırılabilir dosyasının yanında paylaşımlı kütüphaneler bulunan bir homebrew paketi için `--module-dir .` kullanın. Boş bir standart dizine gerek yoktur. Açık bir modül dizini verilmediğinde standart dizin gerekliliği devam eder.

### Dönüştürülmemiş gerekli modüller

Çalıştırılabilir dosyanın ihtiyaç duyduğu ama dönüştürülmeyen bir modül, modül dizinlerinin dışında, modül üst dizininin altında aranır. Bir dosya, SELF kapsayıcısıysa ya da adı `.prx`, `.sprx`, `.suprx` veya `.debug_prx` uzantısına kadar eşleşen bir `ET_SCE_DYNAMIC` ELF'iyse eşleşme sayılır. `--windows` ile büyük/küçük harf yok sayılır; guest modül eşleştirmesinde de böyledir.

- Her eşleşme, dönüştürmeden sonra onu dahil edecek `--module-dir` ile birlikte bildirilir.
- Çıktı yine o adı taşıyan bir host kütüphanesine başvurur.
- Okunamayan bir dosya bildirilmez.

### Örnek komutlar

Linux çıktısı:

```sh
relinker source/input.elf app.elf
```

Windows çıktısı:

```sh
relinker --windows source/input.elf app.exe
```

macOS çıktısı (Apple silicon üzerinde Rosetta altında çalışan x86-64 Mach-O):

```sh
relinker --macos source/input.elf eboot
```

Çalıştırılabilir dosya ve modül kütüphaneleri ad-hoc bir kod imzası taşır, bu yüzden `codesign` olmadan çalışırlar. Birini `codesign -s - -f` ile veya kendi kimliğinizle yeniden imzalamak bu imzanın yerini alır. Birini `install_name_tool` veya `strip` ile düzenledikten sonra yeniden imzalayın.

Intel ana bilgisayarlar için `--to-intel`, Rosetta için `--to-rosetta` ekleyin. Apple silicon üzerinde Rosetta altında çalışan bir oyun `--macos --to-rosetta` ile relink edilir.

- `--to-intel`, çalıştırılabilir dosyada ve paketle gelen modüllerde bulunan AMD'ye özgü komutları yeniden yazar. Bir oyunun çalışma zamanında ürettiği AMD'ye özgü komutları çeviremez, bu yüzden bu tür oyunlar AMD ana bilgisayar gerektirir. Bu seçenek olmadan, bu komutu desteklemeyen bir CPU'da ilk bu tür komut çalıştırmayı sonlandırır; Linux ve Windows'ta çökme raporu komutun adını verir.
- Çıktı biçimi, dosya adı ne olursa olsun varsayılan olarak Linux ELF'dir. Tek başına `.exe` Windows'u seçmez.

### Güvenlik uyarısı

Dönüştürülmüş bir oyun sıradan bir programdır. Kullanıcı hesabınızın izinleriyle çalışır ve hesabınızın okuyabildiği ve yazabildiği her şeyi okuyup yazabilir. Konsolun sandbox'ının hiçbiri yoktur. Yalnızca güvendiğiniz oyunları ve modülleri dönüştürüp çalıştırın. Dosyalarınızdan uzak tutmak için bunları ayrı bir kullanıcı olarak, bir konteynerde veya sanal makinede çalıştırın.

Çalıştırılabilir dosyası ayrıca address space layout randomization (ASLR) ile yerleştirilmez:

- **Linux:** `0x400000` adresine ya da `--linux-fixed-base` adresine yüklenir; kullanımdan kaldırılmış `--lazy-binding` onu konumdan bağımsız tutmadığı sürece.
- **Windows:** en düşük ELF yükleme sayfası, bu aralık boşsa tercih edilen `0x400000` adresine ya da `--windows-fixed-base` adresine, `DYNAMIC_BASE` olmadan yüklenir.
- **macOS:** çalıştırılabilir dosya `MH_PIE` olmadan yazılır.

### CPU gereksinimleri

Oyunlar yerel olarak çalışır; bu yüzden ana bilgisayar CPU'sunun, derlenmiş kodun kontrol etmeden kullandığı PS5'in Zen 2 CPU'sundaki uzantılara sahip olması gerekir. Bunlar arasında AVX2, FMA, F16C, BMI1, BMI2, LZCNT ve MOVBE bulunur.

Linux ve Windows'ta dönüştürülmüş bir oyun, açılışta ana bilgisayar CPU'sunun uzantılarını günlüğe yazar. Biri yoksa, herhangi bir oyun kodunu çalıştırmadan durur ve CPU'yu ile eksik uzantıları adlandırır ([liste](../dev/debt/libkernel.md)).

### Çıktı dosyası çakışmaları

Çalıştırılabilir dosya, registry, Windows import tanılama, simge ve masaüstü girdisi çıktı dosyaları, girdi çalıştırılabilir dosyasına ya da dönüştürülen bir paket modülüne başvurmamalıdır. Buna sabit bağlantılar (hard link) ve sembolik bağlantılar da dahildir.

- Bu çakışmalar, herhangi bir çıktı yazılmadan önce hata verir.
- Var olan bir çıktı dosyası, bu girdilerden ayrıysa değiştirilebilir.
- Var olan bir dizin olan çıktı yolu da herhangi bir çıktı yazılmadan önce hata verir.

Linux ve macOS'ta normal bir çıktı dosyasını değiştirmek, dizinine yazma erişimi gerektirir. Geçici dosya oluşturma ya da eski dosyayı değiştirme başarısız olursa hata alırsınız. Önceki dosyanın üzerine hiçbir zaman yerinde yazılmaz.

## Seçenekler

Tüm anahtarlar varsayılan olarak kapalıdır. `unused-filter` varsayılanı `0`'dır; `--rpath` varsayılanı `$ORIGIN/libs`'dir.

| Seçenek | Etki |
|---|---|
| `--help` | Kullanımı stdout'a yazdırır ve `0` koduyla çıkar; girdi ve çıktı yolları gerekmez. |
| `--version` | `git describe` tarafından bildirilen relinker derleme revizyonunu stdout'a yazdırır ve `0` koduyla çıkar; girdi ve çıktı yolları gerekmez. |
| `--windows` | Bir Windows PE çalıştırılabilir dosyası üretir. |
| `--macos` | Bir x86-64 Mach-O çalıştırılabilir dosyası üretir; paketle gelen modüller `app0/` altında, her biri bulunduğu dizinde (`sce_module`, `sce_modules`, `prx` veya bir `--module-dir`) Mach-O kütüphaneleri olarak yazılır. `--windows` ile çakışır. |
| `--windows-diagnostics` | Açılış bağımlılık tanılamalarını, çıktı çalıştırılabilir dosyasının yanındaki `windows-diagnostics-imports.txt` dosyasına yazar. `--windows` gerektirir. |
| `--windows-gui` | Konsol alt sistemi yerine Windows GUI alt sistemini seçer. `--windows` gerektirir. |
| `--windows-fixed-base <address>` | En düşük ELF yükleme sayfasını, tercih edilen `0x400000` yerine bu sabit Windows adresine eşler; ondalık veya onaltılık (`0x`) verilir. `--windows`, `0x20000` ile `0x7fffffff0000` arasında 64 KiB hizalı bir adres ve kullanıcı adres alanına sığan eksiksiz bir imaj gerektirir. Ana çalıştırılabilir dosyada ASLR ya da taban yeniden konumlandırmaları yoktur; dolu bir adres yüklenemez. Paketle gelen modüller normal yerleşimlerini korur. |
| `--trace-imports` | Her fonksiyon import'unu libc.prx çağrı izleyicisinden geçirir, bkz. [sistem kütüphanesi çağrılarını izleme](#sistem-kütüphanesi-çağrılarını-izleme). Windows, Linux ve macOS çıktısı; `--lazy-binding` ile çakışır. |
| `--linux-fixed-base <address>` | Linux çalıştırılabilir dosyasını varsayılan `0x400000` yerine bu sabit adrese yükler; ondalık veya onaltılık (`0x`) verilir: imajın her sanal adresi bu kadar kaydırılır, yani sanal adres 0, konsolun bir çalıştırılabilir dosyayı yerleştirdiği gibi bu adrese yerleştirilir. `0x10000` ile `0x7fffffff0000` arasında, her `PT_LOAD` hizalamasının katı olan 64 KiB hizalı bir adres ve kullanıcı adres alanında biten bir imaj gerektirir. Çalıştırılabilir dosya, varsayılan adreste olduğu gibi ASLR olmadan `ET_EXEC` olarak yazılır. Paketle gelen modüller normal yerleşimlerini korur. `--windows`, `--macos` ve `--lazy-binding` ile çakışır. |
| `--macos-fixed-base <address>` | macOS çalıştırılabilir dosyasının en düşük ELF yükleme sayfasını bu sabit adrese eşler; ondalık veya onaltılık (`0x`) verilir. `--macos`, `0x20000` ile `0x7fffffff0000` arasında 64 KiB hizalı bir adres ve macOS kullanıcı adres alanında biten bir imaj gerektirir. `__PAGEZERO` adresin 64 KiB altında biter ve Mach-O başlığı bu 64 KiB'ı kaplar. Çalıştırılabilir dosyada ASLR yoktur; işlemin eşleyemediği bir adres yüklenemez. Paketle gelen modüller normal yerleşimlerini korur. |
| `--to-intel` | Çalıştırılabilir dosyadaki ve paketle gelen modüllerdeki desteklenen yalnızca-AMD komutlarını dönüştürür. Desteklenmeyen komutlar ya da erişilemeyen dönüştürme stub'ları hataya yol açar. |
| `--to-rosetta` | `--to-intel` gibi dönüştürür ve ayrıca Rosetta'da bulunmayanları da aşağı çeker: ANDN skaler NOT/AND olur; BLSI, BLSMSK ve BLSR skaler LEA/AND/XOR/NEG dizilerine; SHLX, SHRX, SARX ve RORX bayrakları koruyan skaler kaydırma ve döndürmelere; BZHI ve BEXTR skaler kaydır-ve-maskele dizilerine dönüşür; RDSEED, RDRAND olur; CLWB bir NOP olur; XSAVEOPT, XSAVE olur; RDPID ve RDTSCP'nin ECX'i işlemci 0'ı döndürür. `--to-intel`'i ima eder. `--macos`'tan bağımsızdır: tek başına `--macos` bu komutları olduğu gibi bırakır. `--windows` ile çakışır. |
| `--to-avx` | `--to-intel` gibi dönüştürür ve ayrıca AVX'i olan ama AVX2'si olmayan bir CPU'da bulunmayanları da aşağı çeker: ANDN, BLSI, BLSMSK, BLSR, SHLX, SHRX, SARX, RORX, BZHI, BEXTR, MULX, CLWB ve RDPID, `--to-rosetta`'nın aşağı çektiği şekilde; TZCNT ve LZCNT, sıfır kaynak için bir CMOV ile BSF ve BSR olur; MOVBE, MOV ve BSWAP olur. VINSERTI128, VEXTRACTI128, VPERM2I128, VPBLENDD, VBROADCASTI128 ve VPBROADCASTD ile VPBROADCASTQ'nun bellek biçimleri, yerinde AVX karşılıkları VINSERTF128, VEXTRACTF128, VPERM2F128, VBLENDPS, VBROADCASTF128, VBROADCASTSS ve VBROADCASTSD veya VMOVDDUP olur; 256 bitlik şerit-yerel tamsayı komutları (toplamalar, mantık, karşılaştırmalar, paketlemeler, açmalar, bir şerit içindeki karıştırmalar, anlık değerle ya da XMM sayısıyla kaydırmalar) bir stub içinde iki 128 bitlik yarı olarak çalışır; şeritler arası permütasyonlar VPERMQ, VPERMPD, VPERMD ve VPERMPS, 256 bitlik genişleten taşımalar VPMOVZX ve VPMOVSX ile yazmaç kaynaklı ve bayt veya sözcük yayınlamaları AVX stub'larında çalışır. 256 bitlik depolamalar, skaler karşılaştırmalar, maskeli depolamalar, RDTSCP, XSAVEOPT ve RDSEED için Rosetta geçici çözümleri uygulanmaz. Diğer AVX2 komutları, F16C, PDEP ve PEXT henüz aşağı çekilmez ([liste](../dev/debt/relinker.md)). `--to-intel`'i ima eder. Yalnızca macOS çalışma zamanı ana bilgisayar CPU'sunu denetlemeden başladığı için `--macos` gerektirir. |
| `unused-filter=0` | İçe aktarılan tüm NID başvurularını korur. |
| `unused-filter=1` | Kullanılmayan PLT dışı import'ları kontrol akışı ve GOT erişim analiziyle süzer; adresi alınan kodu erişilebilir sayar; PLT import'larını korur. |
| `unused-filter=2` | Katı kullanılmayan import analizi uygular ve PLT'yi sıkıştırır. Desteklenmeyen analiz durumları hataya yol açar. |
| `--registry` | Çıktı çalıştırılabilir dosyasının yanına `<output-stem>.registry.json`, dönüştürülen her paket modülü için de `<output-stem>.<module>.registry.json` yazar. |
| `--rpath <path>` | Sistem kütüphanesi arama yolunu ayarlar. Kabuğun genişletmesini önlemek için `$ORIGIN`'i tırnak içine alın; örneğin Bash veya PowerShell'de `--rpath '$ORIGIN/libs'`. Linux ve macOS'ta iki nokta üst üste ile ayrılmış her girdi boş olmamalıdır ve guest modüllerle her biri mutlak olmalı ya da `$ORIGIN` ile başlamalıdır. Windows boş olmayan bir ASCII yolu gerektirir ve `$ORIGIN`'i çalıştırılabilir dosyanın dizini olarak destekler. |
| `--omit-unavailable-dependencies` | Linux çıktısı: hiçbir dönüştürülmüş modülün sağlamadığı, modülün hiçbir şey import etmediği ve dönüştürme sırasında hiçbir `--rpath` dizininin barındırmadığı bir paket modülü bağımlılığını dışarıda bırakır. O sırada her çalışma yolu dizini mevcut olmalıdır. Anahtar olmadan böyle bir bağımlılık korunur ve dönüştürme onu adlandıran bir not yazdırır. Yalnızca çalışma yolu her sistem kütüphanesini barındırıyorsa kullanın: daha sonra `LD_LIBRARY_PATH` ile bulunan bir kütüphane artık yüklenmez. `--windows` veya `--macos` ile kullanılamaz. |
| `--autorun` | Dönüştürmeden sonra çıktıyı çalıştırır, çıkış kodunu yazdırır ve Enter'a basılmasını bekler. Beklemeyi atlamak için `ANYPS5_AUTORUN_NO_PAUSE=1` ayarlayın. Hedef işletim sistemini ve hazırlanmış çalışma zamanı yerleşimini gerektirir. |
| `--sce-module-path <path>` | Modül dizinlerinin üst dizini; modül dizininin kendisi değil. Varsayılan: girdi ELF'inin üst dizini; göreli girdi yolları işlemin çalışma dizininden çözülür. Açık mutlak yol: doğrudan kullanılır. Açık göreli yol (`.` ve `..` dahil): işlemin çalışma dizinine görelidir. Çıktı yolunun etkisi yoktur. Boş olmayan tek bir değer gerektirir; tekrarlar, var olmayan veya dizin olmayan yollar, erişim hataları ve `--skip-sce-module` ile birleştirme hatadır. |
| `--module-dir <dir>` | Modül üst dizininin altındaki bir dizinin ELF modüllerini, `sce_module`/`sce_modules`/`prx`'e ek olarak relink eder; çıktılar göreli yolu korur. Gerekli modüller açılışta eşlenir. Linux'ta bu ek dizinlerdeki modüller yalnızca açıkça yüklendiğinde, `sceKernelLoadStartModule`'den gelen argümanlarla başlatılır (örneğin `Media/Plugins` içindeki Unity eklentileri). Windows'ta böyle bir modül, açıkça yüklendiğinde, aynı şekilde başlatılan ve ihtiyaç duyduğu modüllerden sonra başlatılır (aynı başlatma argümanlarını alırlar); ancak bu dizinlerin dışındaki bir modül ona doğrudan ya da başka modüller aracılığıyla ihtiyaç duyuyorsa açılışta başlatılır; böyle bir modülün ilk açık yüklemesi, bir modülün bağımlı olduğu diğerlerini de elinden geldiğince başlatır. macOS'ta, başka bir modülün ya da çalıştırılabilir dosyanın ihtiyaç duyduğu bir modül açılışta başlatılır. Diğer modüller yüklendiğinde başlatılır. Tekrarlanabilir; `.`, girdi ELF'i olmadan üst dizinin kendisidir. `--skip-sce-module` ile çakışır. |
| `--executable <path>` | Oyunun, `sceSystemServiceLoadExec` ile başlattığı başka bir çalıştırılabilir dosyasını da dönüştürür; yol, modül üst dizinine görelidir, örneğin `picker/picker.bin`. Çıktının yanına, yolun `/` karakterleri `.` ile değiştirilmiş ve çıktının uzantısı verilmiş adıyla (`picker.picker.bin.exe`) yazılır ve aynı `libs/`, `app0/` ve guest modüllerini kullanır. Bir guest modül onun için farklı dönüşüyorsa hatadır. Tekrarlanabilir. `--skip-sce-module` ile çakışır. |
| `--patches <file>` | Bu dosyadaki bayt yamalarını dönüştürme sırasında uygular, bkz. [oyun yamaları](#oyun-yamaları). `relinker-patches.toml` keşfini geçersiz kılar. `--no-patches` ile çakışır. |
| `--no-patches` | Yama uygulamaz ve girdi dizininden `relinker-patches.toml` yüklemez. |
| `--verify-patches <file>` | Bir yama dosyasını hiçbir şey yazmadan `<path>`'e karşı denetler, bkz. [oyun yamaları](#oyun-yamaları). Bir yol alır, çıktı yolu almaz. |
| `--verify-output` | `--verify-patches` ile birlikte `<path>`'i, girdi çalıştırılabilir dosyası yerine dönüştürülmüş bir Linux ya da Windows çıktısı olarak ele alır. |
| `--function-symbol-roots` | Dışa aktarma doğrulaması ve Windows/macOS kod analizi sırasında `st_size` aralıklarına güvenmek yerine tanımlı fonksiyon sembollerini kod giriş noktaları olarak ele alır. Sembol boyutlarının farklı kodu tarif ettiği modüller içindir; örneğin üretilmiş yükleyici yardımcıları. Fonksiyon girişleri çalıştırılabilir ve dosyaya bağlı kalmalıdır; veri, TLS ve unwind aralıkları denetimlerini korur. Özgün sembol boyutları korunur. |
| `--skip-sce-module` | Kullanımdan kaldırılmıştır. Paket modülü işlemeyi ve modül dizini denetimlerini devre dışı bırakır. Açık `--sce-module-path` ile çakışır. |
| `--exclude-sce-module <file>` | Kullanımdan kaldırılmıştır. Bir paket modülünü yola göre değil, tam dosya adına göre hariç tutar. Birden fazla dosya için tekrarlayın; eksik bir dosya adı hatadır. `--skip-sce-module` ile çakışır. |
| `--skip-syscall-check` | Kullanımdan kaldırılmıştır. Çalıştırılabilir dosyada ve paketle gelen modüllerde syscall taramasını devre dışı bırakır. |
| `--trap-sdk-syscall-stubs` | Kullanımdan kaldırılmıştır. Yalnızca hata ayıklama içindir. ps5-payload-dev'in genel ve numaralı syscall sarmalayıcı stub'larını ile SDK libc/mman.c'deki tanınan satır içi mmap/mprotect dizilerini ve SDK libc/mount.c'deki satır içi `__syscall6` stub'larını (`mount`, `nmount`, `unmount`) `UD2` ile değiştirir; böylece kullanılmayan sarmalayıcılar relink etmeyi engellemez. Diğer syscall komutları yine hataya yol açar; ancak bir yazılım kesme tablosunun `int 0x7f; ret` ve `int 0x81; ret` girdileri arasındaki bir `int 0x80; ret`, bu seçenek olmadan da `UD2` ile değiştirilir. Tuzaklanmış bir sarmalayıcı çalışırsa illegal-instruction istisnası fırlatır. |
| `--frame-red-zone` | Yalnızca Windows: değerleri RSP'nin altında tutan (System V red zone) yaprak fonksiyonlara bir çerçeve verir; böylece Windows'un bir guest iş parçacığında ilettiği bir istisna bunların üzerine yazamaz. `push rbp; mov rbp, rsp; push ...` prologu olan yaprakları, yalnızca yazmaç iten yaprakları (itmelerden sonra `sub` ayırması olsun olmasın) ve ikisi de olmayanları kapsar; bir atlamadan kısa bir prolog veya çıkış, yanındaki komutları da içine alır. Çerçeve işaretçisi olmayan yapraklarda RSP yalnızca yaprağın altına eriştiği kadar düşürülür ve RSP işlenenleri onunla birlikte, yerinde ya da kodlamaları taşınan yer değiştirmeyi tutamıyorsa bir stub içinde taşınır. Fonksiyon sınırları çalıştırılabilir dosyanın `.eh_frame` bölümünden gelir; yoksa relinker hatayla durur. Relinker kaç tanesine çerçeve verdiğini ve diğerlerini neden atladığını yazdırır ve çerçeve verilmiş bir noktanın üzerindeki oyun yamasını reddeder. |
| `--lazy-binding` | Kullanımdan kaldırılmıştır. İsteğe bağlı (eager) yerine tembel sembol bağlamayı etkinleştirir. Paketle gelen ELF modülleriyle uyumsuzdur. Linux çalıştırılabilir dosyası `0x400000`'a yerleştirilmek yerine konumdan bağımsız (`ET_DYN`) kalır. Yalnızca Windows ve Linux çıktısı. |

`unused-filter=0|1|2` değerini `--` olmadan ve en fazla bir kez belirtin. Bilinmeyen seçenekler ve fazladan konumsal argümanlar hatadır. Dosyaları dönüştürmeden kullanım sözdizimini stdout'a yazdırıp `0` koduyla çıkmak için `relinker --help` kullanın. `relinker`'ı argümansız çağırmak kullanım sözdizimini stderr'e yazdırır ve `1` koduyla çıkar.

`--skip-sce-module`, `--exclude-sce-module <file>`, `--skip-syscall-check`, `--lazy-binding` ve `--trap-sdk-syscall-stubs` bayrakları kullanımdan kaldırılmıştır. Uygulama bu bayraklar etkinken çalışırsa son derece kararsız olur ve genel kullanıma uygun değildir. Bu bayraklar yalnızca hata ayıklama içindir.

`--trap-sdk-syscall-stubs` de hata ayıklama içindir. Bir syscall sarmalayıcısı çağrılırsa bir çalışma zamanı hatasını korur; FreeBSD syscall'larını uygulamaz ve bu uygulamaları genel olarak çalıştırılmaya güvenli hale getirmez.

## Oyun yamaları

Bir yama dosyası, ana bilgisayarın karşılayamadığı bir değeri koda sabit yazmış bir oyun için, dönüştürmeden önce girdi çalıştırılabilir dosyasının ya da paketle gelen bir modülün baytlarını değiştirir. Relinker, değiştirdiği baytları denetler; böylece oyunun başka bir sürümü için hazırlanmış bir yama, onu bozmak yerine başarısız olur. Yama dosyası yoksa çıktı değişmez.

Danışman (advisor) ve katalog doğrulayıcı Python 3.11 veya daha yenisini gerektirir. Yerel bir yama kataloğunu denetlemek için şunu çalıştırın:

```sh
python3 tools/suggest_patches.py --catalog <directory> --param-json <sce_sys/param.json> --input <input.elf> --relinker <relinker> [--sce-module-path <directory>] [--module-dir <directory>]... [--executable <path>]... [--skip-sce-module] [--timeout-seconds <seconds>] [--list-verified] [--json]
```

Bir katalogdaki her girdiyi paylaşmadan önce, oyun dosyaları ya da relinker olmadan denetlemek için şunu çalıştırın:

```sh
python3 tools/validate_patch_catalog.py --catalog <directory>
```

Doğrulayıcı; dizin biçimini, yinelenen JSON anahtarlarını, zorunlu ve isteğe bağlı alanları, göreli yama ve hedef yollarını, yama dosyasının varlığını, TOML yapısını, hedef karma kapsamını ve aynı oyun sürümüyle eşleşebilecek yinelenen `id` girdilerini denetler. Aynı `id` ve içerik sürümüne sahip girdiler, paylaşılan bir hedefin farklı karmaları varsa bir arada bulunabilir; bu durumda eşleşmeleri birbirini dışlar. Aynı sürüm için yeni bir `revision` yayımlamak üzere eski girdiyi kaldırın. Boş bir `entries` dizisi geçerlidir ve `0 entries valid` bildirir.

Doğrulayıcı her geçersiz girdiyi bildirir ve herhangi bir girdi geçersizse `2` ile çıkar; geçerli bir katalog `0` ile çıkar. Bu yapısal bir denetimdir: relinker TOML'un daha katı bir alt kümesini kabul eder ve yamanın ayrıştırılıp bu dosyalarla eşleştiğini yalnızca oyun dosyalarıyla `relinker --verify-patches` belirleyebilir. Doğrulayıcı hedef dosyaları okumaz, hiçbir şey indirmez ve yama uygulamaz.

Danışman, belirtilen `param.json` dosyasından `titleId` ve `contentVersion` okur, aday bir yamanın hedeflediği her dosyanın SHA-256 değerini denetler, ardından `--verify-patches` çalıştırmak için `--relinker` ile belirtilen relinker'ı çağırır. `--relinker ./relinker` gibi göreli bir yol o yerel dosyaya çözülür. `param.json` ve açılmış ELF dosyaları ayrı yerlerde saklansa bile açık meta veri yolu çalışır. Katalog dizini, `format: 1` ve bir `entries` dizisi içeren bir `index.json` içermelidir. Her girdi `titleId`, `contentVersion`, göreli bir `patch` yolu ve her yama hedefini SHA-256'sına eşleyen `targets` içerir. Onaltılık özetlerde harf büyüklüğü önemsizdir. Örneğin:

```json
{
  "format": 1,
  "entries": [{
    "titleId": "TEST00001",
    "contentVersion": "01.000.000",
    "patch": "patch.toml",
    "targets": {"Media/Modules/module.prx": "<SHA-256 in hexadecimal>"}
  }]
}
```

Katalog girdileri şu isteğe bağlı alanları da içerebilir. Bunlar olmayan kataloglar aynı davranışı ve `format: 1`'i korur. Danışmanın eski sürümleri bu alanları içeren eşleşen girdileri reddeder; bunları destekleyen bir danışman sürümü kullanın.

| Alan | Değer |
|-------|-------|
| `id` | `[a-z0-9][a-z0-9_-]*` ile eşleşen kararlı yama tanımlayıcısı. Başka bir içerik sürümü için yeniden kullanılabilir. Bir oyun ve sürüm için aynı kimliğe sahip doğrulanmış girdiler reddedilir. |
| `name` | Yama dosyasının tamamı için kullanıcıya gösterilen ad. |
| `category` | `compatibility`, `fps`, `graphics`, `memory` veya `other` değerlerinden biri. |
| `description` | Yama dosyasının neyi değiştirdiği. |
| `tested` | Aşağıda açıklanan test kayıtlarından oluşan, boş olmayan dizi. |
| `notes` | Kullanıcı için sınırlamalar veya yönergeler. |
| `contributors` | Birbirinden farklı katkıda bulunan adlarından oluşan, boş olmayan dizi. |
| `source` | Yamanın kaynağına HTTPS bağlantısı. Danışman onu yalnızca gösterir; getirmez. |
| `revision` | Yama tanımının, oyunun `contentVersion` değerinden ayrı, pozitif tamsayı revizyonu. |

`category` yamanın alanını tanımlar; etkisinin bir düzeltme mi yoksa iyileştirme mi olduğunu değil; etkiyi açıklamak için `description` kullanın. `revision` en yeni girdiyi otomatik olarak seçmez. Bir oyun ve içerik sürümü için aynı `id`'ye sahip bir yama tanımını değiştirmek üzere eski girdiyi katalogdan kaldırın. Her iki revizyon da doğrulanırsa, danışman ikisini de yinelenen kimlik olarak reddeder.

`name`, `description` ve `notes` varsa boş olmayan tek satırlık metindir. Katalog girdisini bir bütün olarak tanımlarlar; yama TOML'unun `[[patch]]` bölümü içindeki `name` ve `description` ise tek tek bayt değişikliklerini tanımlar. Her `tested` kaydı `stage` (`verification`, `conversion`, `launch` veya `gameplay`), `result` (`passed`, `failed` veya `partial`) ve boş olmayan tek satırlık `evidence` gerektirir. İsteğe bağlı `environment` tek satırlık metindir; isteğe bağlı `date` ise `YYYY-MM-DD` biçiminde bir takvim tarihidir. Örneğin:

```json
{
  "tested": [{
    "stage": "verification",
    "result": "passed",
    "environment": "CachyOS Linux",
    "date": "2026-10-26",
    "evidence": "Relinker reported 1 ok, 0 not-checked, 0 mismatch"
  }]
}
```

Danışman bu alanları eşleşen katalog girdileri için doğrular ve hatalı biçimlendirilmiş bir girdiyi, kalan girdileri durdurmadan bildirir. Meta verileri yalnızca karma ve relinker denetimlerini geçen girdiler için yazdırır. Test kayıtları katkıda bulunanların sağladığı iddialardır; danışman başlatma veya oynanış kanıtlarını bağımsız olarak doğrulamaz.

Danışman, yama dosyalarını yalnızca istenen oyun ve sürümle eşleşen girdiler için doğrular. Hatalı biçimlendirilmiş eşleşen bir girdiyi ya da relinker doğrulama hatasını bildirir ve atlar; böylece başka bir geçerli girdi yine de önerilebilir. Doğrulama, girdi başına varsayılan olarak 120 saniyede zaman aşımına uğrar; sınırı değiştirmek için `--timeout-seconds <positive integer>` kullanın. Sistem modülleri olmayan sentetik girdiler için, dönüştürme de kullanıyorsa `--skip-sce-module` kullanın; bu seçenek `--sce-module-path` ile çakışır. Eşleşen girdi olmaması veya doğrulanmış yama olmaması `1` ile çıkar; geçersiz girdi ya da bir relinker hatasından sonra doğrulanmış yama olmaması `2` ile çıkar. Birden fazla girdi doğrulanırsa, danışman seçim yapamadığı veya birleştiremediği için hiçbirini önermez ve eşleşmeleri incelemek için `--list-verified`'a yönlendirir. Araç yama getirmez veya uygulamaz. Bir öneri, `--patches` ile dönüştürürken kullanılacak modül ve çalıştırılabilir dosya seçeneklerini yazdırır. Otomatik `relinker-patches.toml` keşfini önlemek için katalog yama dosyalarını girdi çalıştırılabilir dosyasının dizininin dışında tutun.

`--list-verified` ile danışman, meta veri, karma ve relinker denetimlerini geçen her girdiyi katalog sırasıyla yazdırır. Her sonuç, katalog numarasını, yama dosyasını, hedef karmalarını ve dönüştürme seçeneklerini içerir. En az bir girdi doğrulanırsa, başka bir girdi reddedilmiş olsa bile `0` ile çıkar; doğrulanmış girdi yoksa mevcut `1` veya `2` çıkış durumu geçerlidir. Bu seçenek olmadan tek bir doğrulanmış girdi önerilir ve birden fazla doğrulanmış girdi belirsiz kalır. Liste, kullanıcılara tek bir yama dosyası seçmelerini söyler. Yama dosyalarını birleştirmez ya da dönüştürme sırasında uygulamaz; seçilen yamayı uygulamadan önce gözden geçirin.

stdout'ta makine tarafından okunabilir tek bir sonuç için `--json` kullanın. `schemaVersion`, hatalar dahil her sonuç için `1`'dir; tüketiciler diğer alanları okumadan önce bunu denetlemelidir. Mevcut `status`, `entries`, `recommended_entry` ve çıkış kodları anlamlarını korur. `status: "verified"` olan her girdi ayrıca `conversion_options` içerir: `["--patches", "/absolute/catalog/patch.toml", "--skip-sce-module"]` gibi tekil argümanlardan oluşan bir dizi. Bunlar, metin modunun yazdırdığı seçeneklerle aynıdır ve aynı sıradadır; relinker programını ya da girdi ELF'ini içermezler. Göreli modül yolları, danışman çağrısındaki anlamlarını korur. Reddedilen girdilerin dönüştürme seçeneği yoktur. Dizi, girdilerin bir birleşimini değil, tek bir doğrulanmış yama dosyasını tanımlar ve yamayı otomatik olarak uygulamaz. Argüman ayrıştırma hataları yine argparse'ın stderr çıktısını ve çıkış durumunu kullanır.

Hedef karmaları, bir yamanın değiştirdiği tam açılmış dosyaları tanımlar; bunlar ana çalıştırılabilir dosyayı içermeyebilir. Farklı dump araçları aynı oyun sürümü için farklı baytlar üretebilir; bu durumda danışman, işe yarayacak olsa bile yamayı reddeder. Tüm dump araçlarının özdeş dosyalar ürettiği doğrulanmamıştır. Karmalar bir kataloğun kimliğini doğrulamaz ve bir yamanın güvenli olduğunu ya da oyunun tamamen oynanabilir olduğunu kanıtlamaz. Güvendiğiniz kaynaklardan gelen kataloglar kullanın ve yamayı uygulamadan önce gözden geçirin.

Yama başka bir oyun çalıştırılabilir dosyasını hedefliyorsa, danışmana her ek çalıştırılabilir dosya için `--executable <path>` verin; yollar modül üst dizinine görelidir, örneğin `picker/picker.bin`. Danışman bu hedef dosyaların karmasını alır ve aynı seçenekleri `relinker --verify-patches`'e iletir. `--patches` ile dönüştürürken aynı `--executable` seçeneklerini kullanın.

Girdi çalıştırılabilir dosyasının dizinindeki `relinker-patches.toml` otomatik olarak yüklenir ve `Patches: <path>` olarak duyurulur. `--patches <file>` başka bir dosya belirtir, `--no-patches` hiçbirini yüklemez. Uygulanan her yama bildirilir:

```
Patch: <target> [<name>] va=0x<address> off=0x<offset> <old> -> <new> (<description>)
```

Dosya TOML'un bir alt kümesidir; başka herhangi bir TOML sözdizimi, satırı adlandıran bir hatadır. UTF-8'dir; boş satırlar, dizelerin dışındaki `#` yorumları, `[[patch]]` girdileri ve `key = value` satırları içerir. Tamsayılar ondalık ya da `0x` onaltılıktır, dizeler kaçış olmadan çift tırnaklıdır ve anahtarlar `[A-Za-z0-9_]+` ile eşleşir.

```toml
format = 1

[[patch]]
name        = "pool_size"
target      = "eboot.elf"
va          = 0x1234
type        = "i32"
expected    = 0x10000000
value       = 0x18000000
max         = 0x7f000000
align       = 0x1000
description = "pool size"
```

`format = 1` zorunludur. Üst düzey `relinker_version`, `relinker_commit`, `config`, `config_sha256`, `main_target`, `linux_base`, `settings` ve `settings_sha256` anahtarları herhangi bir dosyada kabul edilir; bunlardan yalnızca `main_target` ve `linux_base`, `--verify-output` tarafından kullanılır. Başka herhangi bir üst düzey anahtar hatadır.

| Girdi anahtarı | Anlamı |
|-----------|---------|
| `name` | İsteğe bağlı. `[a-z0-9_]` karakterlerinden oluşan bir ayar adı; dosya içinde benzersiz olmalı ve ayrılmış üst düzey anahtar adlarından (`format` ve yukarıdaki sekiz anahtar) biri olmamalıdır. İlerleme satırında, manifestoda ve doğrulama raporunda gösterilir. |
| `target` | Zorunlu. Ana çalıştırılabilir dosyanın dosya adı ya da `--executable` dosyaları ve paketle gelen modüller için modül üst dizinine göreli yol, örneğin `sce_module/libc.prx`. |
| `va` | Zorunlu. Bir disassembler'ın gösterdiği şekliyle ELF'in sanal adresi. |
| `output_rva` | İsteğe bağlı, `0` ile `0xffffffff` arası. `--windows` dönüştürmesi tarafından manifestoya yazılır: girdinin ilk baytının PE çıktısındaki, imaj tabanına göre adresi. Yalnızca `--verify-output` tarafından kullanılır; bir dönüştürme onu yok sayar ve kendininkini yazar. |
| `type` | İsteğe bağlı: `u8 u16 u32 u64 i8 i16 i32 i64`. Bu durumda `expected`, `value`, `min`, `max` ve `align` tamsayılardır, aralık denetimi yapılır ve little-endian yazılır. Olmadan, `expected` ve `value` aynı uzunlukta onaltılık bayt dizeleridir. |
| `expected` | Zorunlu. Girdinin içermesi gereken baytlar. |
| `value` | Zorunlu. Yazılacak baytlar. |
| `min`, `max`, `align` | İsteğe bağlı, yalnızca tipli girdilerde; `value`'ya karşı denetlenir. |
| `description` | İsteğe bağlı. Bildirilir ve manifestoya kopyalanır. |

x86 `mov r/m64, imm32` ve `add`/`sub`/`cmp r/m64, imm32`, anlık değerlerini işaret genişletir. 64 bitlik bir alanı imm32 üzerinden yamalamak için `type = "i32"` kullanın ve negatif değerleri dışarıda tutmak için `min` ekleyin.

Girdiler dosya sırasıyla uygulanır. Bir yama aralığı, bir `PT_LOAD`'ın dosya baytları içinde ve ELF başlığı ile program başlığı tablosunun dışında kalmalıdır. Baytlar `expected` ile farklıysa, bir aralık geçerli değilse, iki girdi çakışıyorsa, bir hedef hiçbir dosyayla ya da birden fazla dosyayla eşleşiyorsa veya bir aralık relinker'ın kendisinin yeniden yazdığı baytlarla (`--to-intel` değiştirmeleri ve trambolinler, relinker yamaları, syscall tuzakları) ya da dosyanın bir yer değiştirmesinin 8 baytlık hedefiyle çakışıyorsa, dönüştürme herhangi bir çıktı yazılmadan önce başarısız olur.

Çalıştırılabilir dosyanın ya da paketle gelen bir modülün, artık GOT girdisi üzerinden bağlanmış `jmp` ile başlamayan bir PLT stub'ı, o girdinin import'u onun üzerinden çağrılmadığı için dönüştürme sırasında bildirilir:

```
Replaced PLT stub: [<target>] va=0x<address> <linked> -> <found> [(jump to 0x<address>)]; import <NID> is not called through it; a --patches entry with expected = "<found>" and value = "<linked>" restores the linked stub
```

`<target>` modülün yama hedefidir ve çalıştırılabilir dosya için yazılmaz. Baytlar 6 baytlık atlamadır; `push` ve PLT başlığına `jmp` de farklıysa 16 baytlık stub'ın tamamıdır. Dönüştürme bunun dışında değişmez ve değişimi olduğu gibi dönüştürür. Hiçbir stub'ın bu şekilde bağlanmadığı bir tablo bildirilmez. Böyle bir stub'ın atladığı kodda, o kodun ilk dönüşünden veya atlamasından önce bir syscall komutu reddedildiğinde, hata stub'ı adlandırır. Stub'ı geri yüklemek o kodu başvurulmamış ama yine de taranan halde bırakır; bu yüzden dosya dönüşmeden önce ikinci bir girdinin onun üzerine yazması gerekir.

En az bir yama uygulandığında, çıktının yanına `<output-stem>.patches.toml` yazılır. Dönüştürülen her dosya için uygulanan her girdiyi, `main_target`, `linux_base` (bir Linux `ET_EXEC` çıktısının yerleştirildiği adres) ve `config` (eğik çizgili biçimde yama dosyası yolu; yol bir `"`, ters eğik çizgi veya kontrol karakteri içeriyorsa dosya adına indirgenir; bu bile tutulamıyorsa dönüştürme herhangi bir çıktı yazılmadan önce başarısız olur) ile birlikte listeler; tipli girdiler `type`, tamsayıları ile `min`, `max` ve `align` değerlerini korur ve `--patches` dönüştürmeyi yeniden üretmek için onu kabul eder. Manifesto, okunduğu yama dosyasının yerini almamalıdır.

`--verify-patches <file> <path>` hiçbir şey yazmaz ve hiçbir girdi uyumsuz değilse `0` ile çıkar. `--verify-output` olmadan, `<path>` herhangi bir türdeki girdi çalıştırılabilir dosyasıdır, modüller bir dönüştürmedeki gibi bulunur (modül seçenekleri ve `--executable` geçerlidir) ve her girdi `applicable` (girdi `expected`'ı içeriyor) ya da `mismatch` olur. Hiçbir şeyle eşleşmeyen bir hedef, `--verify-output`'u öneren bir hatadır.

`--verify-output` ile `<path>`, dönüştürülmüş bir Linux ELF'i veya Windows PE'sidir; Mach-O çıktıları reddedilir. Dosyanın `main_target` değeri olmalıdır, bu yüzden elle yazılmış bir dosyayı değil, üretilen manifestoyu doğrulayın. Girdiler bir `ET_EXEC` çıktısında `va` artı manifestonun `linux_base` değerinde denetlenir; bu da `linux_base` gerektirir; `--lazy-binding`'den gelen bir `ET_DYN` çıktısı ise onsuz bir manifesto gerektirir. `linux_base` olmayan bir manifesto gerektiren PE çıktısında, her girdi `output_rva` konumunda denetlenir ve denetlenen her girdide bu bulunmalıdır; bir bölümün dosya verisinin ötesindeki baytlar, yükleyicinin oraya eşlediği sıfırlardır ve bir aralık yalnızca bu tür baytlar içeriyorsa rapor bunu belirtir. Hedefi `main_target` olan girdi `<path>`'e karşı denetlenir; verilen `--executable` dizeleri için olan girdiler ise bir dönüştürmedeki gibi türetilen çıktı dosyasına karşı denetlenir; `value` içeren bir girdi `applied` olur. Diğer her girdi, yanlış yazılmış bir modül hedefi dahil, `not-checked (module, or executable not named with --executable)` olur. `main_target`'a eşit bir `--executable` gibi aynı ada sahip iki hedef, `ambiguous patch target` hatasıdır. Modül keşif seçenekleri `--verify-output` ile reddedilir.

Doğrulama modu, dönüştürme seçeneklerini (`--windows`, `--macos`, `--to-intel`, `--rpath`, `--patches`, `--no-patches` ve benzerleri) ve ikinci bir yolu reddeder.

## Çalışma zamanı yerleşimi

Windows çalıştırılabilir dosyasında `DYNAMIC_BASE` yoktur ve en düşük ELF yükleme sayfasını `0x400000` adresinde yüklemeyi tercih eder; Ruffle (PPSA68091), VLC (PPSA85300) ve XPSemu (PPSA97358) `app0/imports.txt` dosyalarını okuduklarında imajlarını orada bekler; PE başlıkları önceki 64 KiB'ı kaplar (tercih edilen taban `0x3f0000`). Yer değiştirmeler korunur, bu yüzden bu aralık doluysa Windows onu yeniden konumlandırabilir. Çalıştırılabilir dosya kesin bir adres gerektiriyorsa, en düşük ELF yükleme sayfasını o adrese yer değiştirme olmadan eşleyen `relinker --windows --windows-fixed-base <address> source/input.elf app.exe` kullanın; dolu bir adres yüklenemez. Modüller `DYNAMIC_BASE`'i korur. Bu seçenek macOS çıktısını değiştirmez.

İmajının `0x400000`'da başladığını varsayan bir çalıştırılabilir dosya için, Ruffle (PPSA68091), VLC (PPSA85300) ve XPSemu (PPSA97358) `app0/imports.txt` dosyalarını okuduklarında yaptığı gibi, macOS'ta `relinker --macos --macos-fixed-base 0x400000 source/input.elf app` kullanın. Bu seçeneği yalnızca çalıştırılabilir dosya sabit bir adres gerektirdiğinde kullanın; olmadan macOS çıktısı en düşük ELF yükleme sayfasını `0x140010000`'a yerleştirir. Linux çıktısı, seçenek olmadan sanal adres 0'ı `0x400000`'a yerleştirir; bu, bir oyunun kendi `sceLibcMspaceCreate`'inin denetlediği 40 bitlik pencerenin içindedir; `--linux-fixed-base` başka bir adres seçer ve `--lazy-binding` ile çalıştırılabilir dosya konumdan bağımsız kalır.

Aynı taban, `--executable` ile seçilen çalıştırılabilir dosyalara da uygulanır; paketle gelen PRX modülleri yeniden tabanlanmaz.

Yollar, çıktı çalıştırılabilir dosyasına görelidir:

```text
app.elf (Linux) or app.exe (Windows)
libs/
    *.prx
app0/
    <app resources>
    eboot.bin
    sce_module/
        <converted modules>
```

Girdi dizininin adı buysa `sce_module/` yerine `sce_modules/` veya `prx/` kullanın. Relinker her modülün dizinini `app0/` altında korur ve tam yolunu yazdırır. Uygulama kaynaklarını, oyunun özgün `eboot.bin` dosyası dahil, `app0/` içine ayrıca koyun: oyunlar kendi dizinlerini bulmak için `/app0/eboot.bin`'i sınar, çünkü konsolda her zaman bulunur (ProsperoTV, ProsperoRadio, ProsperoLight ve ProsperoLichess, onsuz boş bir `/data/homebrew/<title id>` dizinine başvurur ve yazı tiplerini yükleyemez). Yalnızca dönüştürülmüş çalıştırılabilir dosya çalışır. Linux'ta, `app0/` ağacı içindeki bir sembolik bağlantı, işaret ettiği dosya veya dizinin türüyle listelenir; bu yüzden bağlantılı bir dizin gerçek bir dizin gibi gezilir. Hedefi `app0/` dışında olan, bağlantılı bir dizinin içindeki bağlantılar bağlantı türünü korur; iç içe ağaçlar için yalnızca üst düzey girdileri bağlayın ya da kopya veya sabit bağlantı kullanın. Derlenmiş sistem kütüphanelerini `build/core/libs/libs/*.prx` konumundan `libs/` içine kopyalayın; hedef işletim sistemi için derlenmiş kütüphaneleri kullanın. Özel bir `--rpath`, sistem kütüphanesi konumunu değiştirir.

Bazı uygulamalar çalışma zamanında özgün `app0/sce_sys/param.json` dosyasına da ihtiyaç duyar. AnyPS5 bu dosyayı oyun kimliği ve görünen ad için okur; boş olmayan bir `titleId` dizesi ve, boş olmayan bir `titleName` içeren bir dil girdisi bulunan, boş olmayan bir `localizedParameters` nesnesi içermelidir. `defaultLanguage` ile adlandırılan girdiyi (`localizedParameters` içinde ya da üst düzeyde) seçer; o girdi yoksa `en-US`'i, ardından nesne değerli ilk dil girdisini dener. Değerleri oyunun kendi meta verilerinden koruyun ve dosyayı diğer uygulama kaynaklarıyla aynı `app0/` dizininin altına koyun. Açılışta, çalışma dizininde bulunması `data/` dizinini oluşturur; sıfırdan farklı bir `downloadDataSize` ayrıca `download0/` dizinini oluşturur. `param.json` olmadan AnyPS5, bunun yerine `app0/sce_sys/param.sfo` dosyasını (`TITLE`, `TITLE_ID` ve `DOWNLOAD_DATA_SIZE`) okur; `data/` ve `download0/` üzerindeki etki aynıdır; her iki dosya da yoksa hiçbir dizin oluşturulmaz. İkisi de yoksa ya da okunan geçersizse, oyun/pencere kurulumu gibi meta veri çağrıları bir `param.json` veya `param.sfo` hatasıyla başarısız olabilir.

Kaynaklar, `app0/` yanında bir `app0.zar` içine de paketlenebilir; bu, kökü `app0/` dizini olan bir [ZArchive](https://github.com/Exzap/ZArchive)'dir. `app0/` içinde bulunan bir dosya oradan, diğer her dosya arşivden okunur; çalıştırılabilir dosya, `sce_module/` ve `sce_sys/` `app0/` içinde kalır. Arşiv yalnızca `sceKernelOpen`/`open`, tanıtıcıları üzerindeki read, seek ve stat çağrıları ve `sceKernelStat` aracılığıyla okunur; arşivlenmiş bir yoldaki diğer çağrılar, arşivin de içerdiği bir dizini listelemek ve içindeki bir dosyayı yazmak için açmak [teknik borçta](../dev/debt/libkernel.md) açıklandığı gibi başarısız olur. Açılamayan bir arşiv, oyunu ilk dosya erişiminde durdurur.

Paketle gelen oyun modülleri için, relinker'ın `app0/` içine yazdığı ve yazdırdığı yerleşimde listelediği `.guest.prx` dosyalarını kullanın. `libs/`, özgün PS5 `.prx` dosyaları için değil, AnyPS5 sistem kütüphaneleri içindir. Özgün bir PS5 modülünü Windows'ta `libs/` içine koymak, Windows'un onu DLL olarak yüklemeye çalışmasına yol açar ve 193 hatasıyla (geçerli bir Win32 uygulaması değil) başarısız olabilir. Oyunun çalışırken yüklediği, relink edilmemiş bir modül, örneğin `--module-dir`'e verilmeyen bir dizindeki, de yüklenmez: oyun bir hata alır ve devam eder, ve çalışma zamanı her dosya için bir kez `[module] dlopen: <path> was not relinked: ...` yazdırır. O modülün dizinini `--module-dir <dir>` ile relink edin.

Windows'ta, kendi derlediğiniz bir `libs/` ayrıca kütüphaneleri derleyen araç zincirinin `mingw64/bin` dizininden `libgcc_s_seh-1.dll`, `libstdc++-6.dll` ve `libwinpthread-1.dll` dosyalarını da gerektirir; sürüm arşivleri bunları zaten içerir. Kütüphaneler `PATH` aranmadan yüklenir, bu yüzden sistemde başka yerdeki bir kopya kullanılmaz ve bunlardan birine ihtiyaç duyan bir kütüphane, `.prx` dosyası mevcut olsa bile 126 hatasıyla (belirtilen modül bulunamadı) başarısız olur. `APS5_MIMALLOC` ayarlıysa oyun `libs/` içinden `mimalloc.dll` ve `mimalloc-redirect.dll` dosyalarını yükler ve mimalloc üzerinden bellek ayırır. İkisini de `build/core/libs/libs` konumundan kopyalayın; sürüm arşivleri bunları içerir. Onlar olmadan oyun C çalışma zamanının yığınını korur ve hiçbir şey yazdırmaz.

macOS'ta, `APS5_MIMALLOC` ayarlıysa oyun, `libs/` içinden `libmimalloc.dylib` yüklenmiş olarak hemen yeniden başlar ve mimalloc üzerinden bellek ayırır; kendi derlediğiniz `build/core/libs/libs` bunu zaten içerir. Dosya olmadan oyun sistem ayırıcısını korur ve hiçbir şey yazdırmaz.

Linux'ta, `APS5_MIMALLOC` ayarlıysa oyun, `libs/` içinden `libmimalloc.so` `LD_PRELOAD` içindeyken hemen yeniden başlar ve mimalloc üzerinden bellek ayırır; sürüm arşivleri ve kendi derlediğiniz `build/core/libs/libs` bunu içerir. Dosya olmadan oyun sistem ayırıcısını korur ve hiçbir şey yazdırmaz. Boşluk veya iki nokta üst üste içeren bir `libs/` yolu `LD_PRELOAD` içinde listelenemez: oyun bu durumda bir satır yazdırır ve sistem ayırıcısını korur. Yeniden başlatmadan sonra `APS5_MIMALLOC` ve `LD_PRELOAD` ayarlı kalır, bu yüzden oyunun kendi ortamı ve başlattığı her işlem bunları taşır. mimalloc sistem ayırıcısından daha fazla bellek tutar: PPSA21564'te onunla yaklaşık 0,9 GiB daha az ana bilgisayar belleği kullanılabilirdi.

Windows'ta doğrudan bellek (`sceKernelAllocateDirectMemory`, oyun başına en fazla 13824 MiB), oyun onu ayırdığında ya da sayfalarını ilk kullandığında değil, eşlediğinde commit edilir. Sistem commit sınırı (kurulu bellek artı sayfa dosyası boyutu, Görev Yöneticisi'ndeki Committed'ın ikinci değeri), eşlenen doğrudan belleği diğer tüm commit edilmiş bellekle birlikte karşılamalıdır. Aksi halde `sceKernelMapDirectMemory`, Windows hatasıyla birlikte `replace guest placeholder with resident direct memory` fırlatır (bir sayfa ikinci bir adrese eşlendiğinde `commit direct memory pages`, eşlemesi kaldırıldığında `keep unmapped direct memory`); sayfa dosyasını büyütün ya da diğer uygulamaları kapatın.

Linux'ta, Vulkan sürücüsü dma-buf belleğini içe aktardığında (NVIDIA'nın kapalı kaynak sürücüsü hariç), paylaşılan doğrudan bellek `/dev/udmabuf` üzerinden içe aktarılır ve oyunu çalıştıran kullanıcının ona okuma-yazma erişimine ihtiyacı vardır. Birçok dağıtım onu `root:kvm` ve `0660` modunda oluşturur: kullanıcıyı `kvm` grubuna ekleyin ve yeniden oturum açın (`setfacl -m u:$USER:rw /dev/udmabuf` gibi bir ACL bir sonraki yeniden başlatmaya kadar sürer). Erişim olmadan açılış `[gpu] open /dev/udmabuf: Permission denied` yazdırır, bu aralıklar içe aktarılmak yerine kopyalanır ve bunlara FLAT/GLOBAL adresleri üzerinden yapılan GPU depolamaları `BDA access failed` ile başarısız olur. 64 MiB'tan büyük bir aralık, aygıt seyrek belleği bağlıyorsa (RADV, ANV) 64 MiB'lık udmabuf arabellekleri üzerinde tek bir seyrek arabellek olarak içe aktarılır. Seyrek bağlama olmadan ya da aygıtın seyrek blok boyutu 64 KiB'tan büyükse, udmabuf'un `size_limit_mb` değerini (varsayılan 64 MiB) aşan aralıklar da kopyalanır.

Linux:

```sh
./app.elf
```

Relinker, `app.elf` dosyasını ve listelediği her `Executable file:` dosyasını çalıştırma izniyle yazar. Dosya modunu kaybetmiş bir kopya, örneğin bir tarayıcı indirmesi, önce her biri için `chmod +x` gerektirir; onsuz başka bir çalıştırılabilir dosya başlatan bir oyun `sceSystemServiceLoadExec: execv failed: Permission denied` ile durur.

Windows PowerShell:

```powershell
.\app.exe
```

Standart çıktı veya hata tanıtıcısı olmadan başlatılan bir Windows çalıştırılabilir dosyası, örneğin Explorer'dan başlatılan bir `--windows-gui` derlemesi, bu çıktıyı yanındaki `anyps5.log` dosyasına yazar ve önceki çalıştırmanın dosyasının yerini alır. O dizine yazma erişimi olmadan günlüksüz çalışır.

### Ekran çözünürlüğü ve pencere boyutu

Oyun, video çıkışı çözünürlüğünü seçer; AnyPS5 işleme için o modu kullanır. Ekran penceresi yeniden boyutlandırılabilir veya tam ekrana geçirilebilir; bu, sunulan görüntüyü pencereye sığacak şekilde ölçekler. F11, pencere ile tam ekran arasında geçiş yapar ve pencereyi önceki boyutuna döndürür. Pencerenin baştan masaüstü çözünürlüğünde tam ekran açılması için `ANYPS5_FULLSCREEN` değerini `1` veya `borderless` yapın; pencereli tutmak için `0` veya `windowed` (varsayılan) yapın. `true` veya `yes` gibi boş olmayan herhangi bir diğer değer yine eskisi gibi tam ekran açar ve stderr'deki bir uyarı değeri adlandırır. Tam ekran, pencerenin üzerinde bulunduğu ekrana gider; bu, açılışta `ANYPS5_DISPLAY_INDEX`'in seçtiği ekrandır. Oyun bir fare açmadıkça pencere tam ekranken fare imleci gizlenir. Pencere boyutunu değiştirmek oyunun işleme çözünürlüğünü değiştirmez. Bir oyunun okuduğu çözünürlüğü değiştiren tek ana bilgisayar ayarı, aşağıda açıklanan `APS5_OUTPUT_RESOLUTION`'dır; bunun dışında AnyPS5 işleme çözünürlüğü geçersiz kılma sunmaz, bu yüzden bir oyunun sunduğu durumda onun kendi ekran ayarını kullanın.

`APS5_OUTPUT_RESOLUTION=<width>x<height>`, örneğin `APS5_OUTPUT_RESOLUTION=1920x1080`, genişlik 3840'ın ve yükseklik 2160'ın altındaysa oyunun video çıkışı durumunun, ekranın geçerli modunun sınıfı yerine 1080p sınıfını bildirmesini sağlar; aksi halde 2160p sınıfını bildirir. İki sayı, `x` veya `X` ile ayrılmış pozitif tamsayılar olmalıdır. Boş bir değer, ayarlanmamış bir değerle aynıdır; başka herhangi bir değer, değişkeni adlandıran bir hatayla oyunu durdurur.

`ANYPS5_DISPLAY_INDEX=<n>`, pencereyi SDL'nin ekranları listeleme sırasına göre 0'dan sayılan `n` numaralı ekranda ortalanmış olarak açar. Ayarlanmazsa ilk ekran kullanılır. Bağlı bir ekranın dizini olmayan bir değer, boş bir değer dahil, oyunu açılışta değişkeni adlandıran bir hatayla durdurur.

Linux'ta, birlikte gelen SDL2, Wayland kompozitörü xdg-decoration duyurduğunda sunucu tarafı pencere süslemeleri ister. GNOME dahil xdg-decoration desteği olmayan kompozitörler, mevcutsa libdecor kullanır. libdecor pencereyi üç saniye içinde yapılandıramazsa AnyPS5 xdg-shell ile yeniden dener. Yeniden deneme süslemesiz açılabilir ve kompozitör o pencereyi de yapılandıramazsa açılış bir hata bildirir. libdecor'u devre dışı bırakmak için `SDL_VIDEO_WAYLAND_ALLOW_LIBDECOR=0`, önce onu denemek için `SDL_VIDEO_WAYLAND_PREFER_LIBDECOR=1` ya da X11 veya XWayland'i seçmek için `SDL_VIDEODRIVER=x11` ayarlayın.

### VR uyumluluğu

AnyPS5 şu anda PS VR2 başlıklarını ya da OpenXR çalışma zamanı üzerinden bağlanan PC VR başlıklarını desteklemez. Başlık takibi, stereoskopik çıktı veya yeniden projeksiyon sağlamaz. Bir VR oyunu, aygıt algılanmadığında başlık kurulumunu atlayabilir, ancak bu VR oynanışının desteklendiği anlamına gelmez.

### Test çalıştırmaları için kumanda girdisi

Yerel bir komut dosyasından kumanda darbelerini etkinleştirmek için uygulamayı başlatmadan önce `APS5_PAD_INPUT_FILE` değerini mutlak bir yola ayarlayın. Uygulama kumanda girdisini okumaya başladıktan sonra, depo kökünden yardımcı araçla bir düğme gönderin:

```sh
APS5_PAD_INPUT_FILE=/tmp/anyps5-pad ./app.elf
python3 tools/pad_input.py /tmp/anyps5-pad cross
python3 tools/pad_input.py /tmp/anyps5-pad down --hold-ms 500
python3 tools/pad_input.py /tmp/anyps5-pad release
python3 tools/pad_input.py /tmp/anyps5-pad release --left-stick 128 0 --hold-ms 2000
python3 tools/pad_input.py /tmp/anyps5-pad cross --left-stick 255 128 --right-stick 0 128
python3 tools/pad_input.py /tmp/anyps5-pad touch-right
python3 tools/pad_input.py /tmp/anyps5-pad l1+r1
```

Zamanlı düğme dizileri için bkz. [Kumanda zaman çizelgeleri](INPUT_MAPPING.tr.md#kumanda-zaman-çizelgeleri).

Yardımcı aracı ikinci bir terminalde çalıştırın. Komutlar kumanda okumaları sırasında en fazla her 100 ms'de bir yoklanır ve düğmeleri fiziksel kumanda girdisine eklenir. Bir basış varsayılan olarak 300 ms sürer, uygulama onu okuduğunda başlar ve otomatik olarak bırakılır; `--hold-ms` 1–5000 ms kabul eder. `--left-stick X Y` ve `--right-stick X Y`, çubukları aynı süre boyunca tutar: her eksen 0-255'tir, 128 merkezdir, X sağa doğru, Y aşağıya doğru artar. Böyle bir komut etkinken her iki çubuk da fiziksel kumanda yerine ondan gelir ve adlandırılmamış bir çubuk merkezde kalır. `touch-left` ve `touch-right`, `TouchLeft` ve `TouchRight` tuşlarının yaptığı gibi, dokunmatik yüzeye sol veya sağ yarısında bir parmakla basar; komut dosyasında düğme alanının `0x200000` ve `0x400000` bitleridir. `l1+r1` gibi `+` ile birleştirilen düğme adları tek bir komutta birlikte basılır; `release` birleştirilemez. Başka bir basış göndermeden önce uygulamanın bir bırakmayı gözlemlemesini bekleyin. Dosya yalnızca en son komutu tutar; hızlı komutlar birbirinin yerini alabilir. POSIX'te, var olan bir komut dosyasını değiştirmek izin modunu korur; ayrıcalıklı bir yardımcı araç, oyun kullanıcısı yine okuyabilsin diye sahibini ve grubunu da korur. Uygulama kumandayı ilk okuduğunda geride kalan komutlar yok sayılır. Ortam değişkeni olmadan dosya girdisi devre dışıdır.

Pencerenin klavye ve fare olaylarını yok saymasını sağlamak için `APS5_NO_PAD_INPUT` değerini herhangi bir değere, boş olan dahil, ayarlayın. Klavye ve fare bağlamaları, fare çubuğu ve pencere tuşları (F10, F11 ve F12 dahil) o zaman hiçbir şey yapmaz. Kumandalar ve `APS5_PAD_INPUT_FILE` komutları etkilenmez.

Windows'ta, SDL'nin Windows Gaming Input kumanda arka ucunu kapatmak için `APS5_NO_WGI` değerini herhangi bir değere ayarlayın. AnyPS5 orada DirectInput'u her durumda kapatır ve bu değişken video çıkışı sürücüsü oluşturulduğunda bir kez okunur.

### Ana bilgisayar mikrofonu seçimi

Varsayılan olarak `libSceAudioIn`, SDL'nin varsayılan ana bilgisayar girdi aygıtından yakalar. Başka bir mikrofon seçmek için `ANYPS5_AUDIO_IN_DEVICE` değerini SDL'nin bildirdiği tam bir yakalama aygıtı adına ayarlayın. Ayarlanmamış ya da boş bir değer veya `default`, SDL'nin varsayılanını korur. Seçilen ad yoksa, birden fazla kez görünüyorsa ya da açılamıyorsa, oyunun AudioIn açma çağrısı `0x8026010F` geçersiz aygıt hatasını döndürür; AnyPS5 sessizce başka bir mikrofona geçmez. SDL2 yakalama aygıtlarını adlarıyla tanımlar, bu yüzden yinelenen adlar tek tek seçilemez. Ayar, oyun bir AudioIn portunu her açtığında okunur.

```sh
ANYPS5_AUDIO_IN_DEVICE="USB Microphone" ./app.elf
```

### Kumanda titreşim gücü

Oyunun titreşiminin kumandanıza ne kadar güçlü ulaştığını ölçeklemek için `ANYPS5_RUMBLE` değerini `0` ile `100` arasında bir tamsayıya ayarlayın. Değişken ayarlanmadığında da değer olan `100`, titreşimi oyunun istediği şekilde aynen iletir. `0` titreşimi kapatır: motorlar hiçbir zaman sıfırdan farklı bir istek almaz ve çalışmakta olan bir titreşim durdurulur. Aradaki değerler motor gücünü o yüzdeyle ölçekler ve bağlı her kumandaya uygulanır.

```sh
ANYPS5_RUMBLE=50 ./app.elf
ANYPS5_RUMBLE=0 ./app.elf
```

Ayar; titreşim motorlarını, bu özelliğe sahip kumandaların darbe-tetik (impulse-trigger) titreşimini ve bir oyunun ses olarak çaldığı titreşimi kapsar; hem DualSense'in kendi ses kartında çalındığında hem de motor titreşimine dönüştürüldüğünde. DualSense uyarlanabilir tetik efektlerini (direnç ve tetik titreşim profilleri) değiştirmez; bunlar oyunun gönderdiği şekilde kumandaya ulaşmaya devam eder. Boş, işaretli, kesirli ya da 100'ün üzerinde bir değer, oyunu açılışta `ANYPS5_RUMBLE`'ı adlandıran bir hatayla durdurur.

Bu, konsol davranışı değil, ana bilgisayar tarafında bir konfor ayarıdır: oyunla kumandanız arasında AnyPS5 tarafından uygulanır, açılışta bir kez okunur ve onun için bir kısayol tuşu ya da katman girdisi yoktur. Herhangi bir konsol ayarının modeli değildir.

### Pencere odakta değilken titreşim

Varsayılan olarak kumanda, AnyPS5 penceresi odakta olmadığında titreşmeyi bırakır ve odak geri döndüğünde bir sonraki darbeyle yeniden titreşir. Pencere arka plandayken titreşimi açık tutmak için `ANYPS5_RUMBLE_ON_FOCUS_LOSS=0` ayarlayın. Yalnızca titreşim motorları etkilenir: ışık çubuğu ve kumandanın durumu değişmez ve tanımlama darbeleri pencere odaktayken yine çalışır. `0` veya `1` dışındaki herhangi bir değer, oyunu açılışta değişkeni adlandıran bir hatayla durdurur.

### Kumanda bağlantısı ve kullanıcı 0

Varsayılan olarak, tek kumanda çıkarılsa bile port 0 bağlı ve ilk kullanıcı oturum açmış kalır; bu yüzden oyun son durumu okumaya devam eder ve kumandanın gittiğini hiç öğrenmez. Oyunun bunun yerine kumandayı görmesini sağlamak için `ANYPS5_PAD_PORT0_STATE=controller` ayarlayın:

```sh
ANYPS5_PAD_PORT0_STATE=controller ./app.elf
```

`controller` ile Oyuncu 1'e bağlı kumandayı çıkarmak, oyunun sıfırlanmış girdiyle bağlantısı kesilmiş bir kumanda görmesini sağlar ve kullanıcı 0 oturumu kapatır; kumandayı geri takmak kumandayı bağlar ve kullanıcının oturumunu yeniden açar. Bir değişiklik yalnızca kumanda 500 ms boyunca yok ya da var kaldıktan sonra uygulanır; böylece kopup geri gelen bir kablo oyunun gidip gelmesine yol açmaz. Klavye ve fare bu modda kumanda sayılmaz: Oyuncu 1'e hiçbir kumanda bağlı olmadığı sürece girdileri oyuna verilmez. `default` değeri ya da değişkeni ayarlamamak, yukarıdaki davranışı korur. Boş olan dahil başka herhangi bir değer, video çıkışı başladığında oyunu `ANYPS5_PAD_PORT0_STATE`'i adlandıran bir hatayla durdurur.

Yalnızca port 0 ve kullanıcı 0 kumandayı izler. Oyuncu 2 ila 4, öncesinde sahip oldukları durumu bildirmeye devam eder.

### Kumanda tanımlama darbesi

Hangi fiziksel kumandanın hangi oyuncu olduğunu hissetmek için `ANYPS5_PAD_IDENTIFY` ayarlayın. Varsayılan `off` hiçbir şey göndermez. `connect`, bir kumanda bir oyuncu yuvasına bağlandığında ona kısa bir desen gönderir: oyuncu 1 için bir darbe, oyuncu 2 için iki, oyuncu 4 için dörde kadar. `always` ayrıca oyun titreşmiyorken her 30 saniyede bir aynı deseni tekrar gönderir. Desen, oyun 500 ms boyunca titreşmediğinde başlar. Oyun titreşmeye başlarsa desenin geri kalanı hemen durur ve oyunun titreşimi her zamanki gibi gönderilir. Desen `ANYPS5_RUMBLE` ile ölçeklenir ve `0` değeri hiçbir şey göndermez. Titreşim desteği olmayan bir kumanda bir satır günlüğe yazar ve darbe almaz. Klavye ve fare hiçbir zaman darbe almaz. `off`, `connect` veya `always` dışındaki bir değer, boş olan dahil, oyunu açılışta değişkeni adlandıran bir hatayla durdurur.

```sh
ANYPS5_PAD_IDENTIFY=connect ./app.elf
```

Bu, konsol davranışı değil, ana bilgisayar tarafında bir kolaylıktır: oyun bu darbeleri göndermez. Darbe zamanlaması ve gücü AnyPS5 tarafından seçilmiştir ve gerçek kumandalara karşı ölçülmemiştir.

### Video çıkışı ve kumanda telemetrisi

Uygulamayı başlatmadan önce `APS5_VIDEOOUT_TELEMETRY_DIR` değerini bir dizine ayarlarsanız, çalıştırmanın sunumları (present) ve kumanda değişiklikleri `<directory>/videoout.jsonl` dosyasına, satır başına bir JSON nesnesi olacak şekilde yazılır; dosya zaten varsa sonuna eklenir. Her satır `"v":1` ile, ana bilgisayarın sabit saat nanosaniyeleriyle `t_ns` alanında ve olay adıyla `ev` alanında başlar. Bir çalıştırma, `{"v":1,"t_ns":<ns>,"ev":"contract","contract":"v1-pad","commit":"<12 hex>","pid":<pid>,"prx":"libSceVideoOut"}` biçiminde bir `contract` satırıyla başlar: `commit`, kütüphanelerin derlendiği kaynak commit'tir (yapılandırma sırasında Git bulunamadıysa `unknown`) ve `pid`, aynı dosyaya ekleme yapan çalıştırmaları birbirinden ayırır. Oyunun okuduğu kumanda durumu düğmelerinde, çubuklarında veya tetiklerinde değiştiğinde bir `pad` satırı yazılır: `{"v":1,"t_ns":<ns>,"ev":"pad","n":<present>,"buttons":<bits>,"sticks":[<left x>,<left y>,<right x>,<right y>],"triggers":[<L2>,<R2>]}`; burada `n`, değişiklik yayımlandığında tamamlanmış sunum sayısıdır, yani pencere başlığında gösterilen sayı. `buttons` ondalık düğme bit alanıdır, çubuk eksenleri 0-255 aralığındadır ve 128 merkezdir, her iki tetik de 0-255 aralığındadır. Hareket, dokunma ve aygıt türü karşılaştırılmaz; bu yüzden yalnızca kumandayı hareket ettiren bir çalıştırma `pad` satırı yazmaz. Bir `pad` satırının bildirdiği durum, SDL üzerinden okunan kumanda ile klavye/fare girdisidir; `APS5_PAD_INPUT_FILE` ile beslenen girdi daha sonra, `libScePad` içinde uygulanır, bu yüzden betikli bir çalıştırma `pad` satırı yazmaz. İlk sunumdan sonra ve ondan sonra her 30 sunumda bir, pencere başlığında gösterilen sayı ve oranla birlikte `{"v":1,"t_ns":<ns>,"ev":"present","n":<present>,"fps":<rate>}` biçiminde bir `present` satırı yazılır. Satırları bir arka plan iş parçacığı yazar ve oyun beklenmez: geride kaldığında satır atılır ve şimdiye kadar atılan sayıyla birlikte bir `drops` satırı, `{"v":1,"t_ns":<ns>,"ev":"drops","dropped":<count>}`, sayı arttığı sürece on saniyede bir ve normal çıkışta bir kez, yalnızca satır atıldıysa yazılır; statik yıkıcıları atlayan bir çıkış son satırı yazmaz. Dolu kuyruğun reddettiği bir pad satırı, sonraki bir yayımlama değişikliği yazsa bile atılmış sayılır. Değişken olmadan hiçbir dosya açılmaz ve hiçbir iş parçacığı başlatılmaz; boş bir değer, ayarlanmamış bir değerle aynıdır ve yazılamayan bir dizin, video çıkışı başladığında dosyayı adlandıran bir hatayla oyunu durdurur.

Tamamlanmış guest flip aralığını ölçmek için `APS5_VIDEOOUT_FLIP_TIMING=FIRST:COUNT` ayarlayın ve her çalıştırma için yeni bir `APS5_VIDEOOUT_TELEMETRY_DIR` kullanın. Örneğin `601:600`, 601 ile 1200 arasındaki flip'leri kaydeder. İlk flip numarası pozitiftir ve sayı 2–100000'dir. Yakalama, bir flip'i tamamlayan ilk ana çıkışı kullanır. Sözleşme `flip_start` ve `flip_count` ekler ve her `flip_complete` satırı tamamlanma sırası `n`'yi, çıkış `generation`'ını ve işlem başlangıcından itibaren monoton mikrosaniye cinsinden guest flip durum zaman damgası `process_us`'u verir. Bitişik `process_us` değerleri, temizlenmiş kareler dahil, tamamlanmış flip'ler arasındaki aralığı ölçer.

Tek bir generation içinde sözleşmenin tüm penceresini kapsayan sıralı, bitişik bir dizi kullanın. Eksik kayıtlar eksik bir yakalamayı gösterir; örneğin bir kuyruk düşmesinden, çıkışın yeniden açılmasından veya erken çıkıştan sonra. Kaybolan telemetri kayıtlarını incelemek için mevcut `drops` olayını kullanın.

### Kumanda hoparlörü yedek sesi

Kumanda hoparlörü yokken ana stereo çıkışa katlanan AudioOut2 kumanda hoparlörü portlarının kazancını değiştirmek için uygulamayı başlatmadan önce `APS5_PAD_SPEAKER_FALLBACK_DB` ayarlayın. Değer, bir oyun AudioOut2 bağlamını oluşturduğunda bir kez okunur. Varsayılan `0` dB'dir ve karışımı değiştirmeden bırakır. `-60` ile `0` arasında bir sayıyı ya da sessizlik için `mute`, `off` veya `-inf` değerlerini kabul eder; çevresindeki boşluklar yok sayılır. Boş, aralık dışı, sonlu olmayan bir sayı ya da sonda metin içeren bir değer de dahil olmak üzere başka herhangi bir değer, oyunu değişkeni ve değeri adlandıran bir hatayla durdurur. Ana karışım portlarını veya gerçek bir kumanda hoparlörünü etkilemez ve eski AudioOut kumanda hoparlörü portuna uygulanmaz.

Hem bir ana porttan hem de bir kumanda hoparlörü portundan ses çalan bir oyun, yedek sessize alındığında ana port kopyasını korur. O kopya oyunun kendi ses ayarlarını izler; kumanda hoparlörü kopyası izlemeyebilir.

### Sistem yazı tipleri

Konsolun sistem yazı tipi kümelerini açan oyunlar (`sceFontOpenFontSet`), çıktı çalıştırılabilir dosyasının yanındaki bir `anyps5-fonts/` dizininde yazı tipi dosyalarına ihtiyaç duyar; başka bir dizin kullanmak için `ANYPS5_SYSTEM_FONTS` ayarlayın. Konsoldan dökülen dosyalar kendi adlarıyla kullanılır (`SST-Roman.otf`, `SST-Bold.otf`, `SSTJpPro-Regular.otf`, ...). Onlar olmadan, bulunduklarında bu açık lisanslı yedekler kullanılır: `NotoSans-{Light,Regular,Medium,Bold}.ttf` ve `NotoSans-{LightItalic,Italic,MediumItalic,BoldItalic}.ttf` (Latin ve Vietnamca), `NotoSansMono-{Light,Regular,Medium,Bold}.ttf` (daktilo), `NotoSansThai-{Light,Regular,Medium,Bold}.ttf` (Tayca), `NotoSansArabic-{Light,Regular,Medium,Bold}.ttf` (Arapça) ve `NotoSansCJK-{Light,Regular,Medium,Bold}.ttc` (Japonca, Çince ve Korece). Birkaç yazı sistemini kapsayan bir küme, ana dosyasında bulunmayan karakterler için diğer yazı sistemlerinin dosyalarını da (`SSTThai-*.otf`, `SSTArabic-*.otf`, `SSTJpPro-*.otf`, `DFHEI5-SONY.ttf`, `SCEPS4Yoongd-*.otf` veya bunların yedekleri) açar; eksik olan bir dosya, o karakterleri glifsiz bırakır. İkisi de yoksa, bir sistem yazı tipi kümesini açmak başarısız olur (günlük `file=(none)` ile `sceFontOpenFontSet` gösterir) ve sonrasında ne olacağı oyuna bağlıdır: bazıları bu yazı tiplerinde hiç metin göstermez, bazıları ise açmanın başarılı olduğunu doğruladıkları için açılışta durur. Yedekler, SIL Open Font License altında [Noto projesinden](https://github.com/notofonts/notofonts.github.io) gelir (Noto Sans dosyaları `fonts/NotoSans/full/ttf` içinde, Arapça olanlar `fonts/NotoSansArabic/full/ttf` içindedir); yukarıda adı geçen dosyaları `anyps5-fonts/` içine kopyalayın. Kurulu olduğu yerde bir Linux dağıtımının Noto paketi de kopyalamadan kullanılabilir: `ANYPS5_SYSTEM_FONTS` değişkenini `NotoSans-Regular.ttf` dosyasını barındıran dizine yönlendirin (örneğin Arch Linux'un `noto-fonts` paketinden `/usr/share/fonts/noto`). O dizin yalnızca bir paketin dosyalarını barındırır ve `ANYPS5_SYSTEM_FONTS` tek bir dizini adlandırır: Arch, `NotoSansCJK-*.ttc` dosyalarını `noto-fonts-cjk` paketinden `/usr/share/fonts/noto-cjk` içine kurar, bu yüzden Japonca, Çince ve Korece kümelerin orada dosyası yoktur. Bunları kapsamak için iki paketin dosyalarını tek bir dizine bağlayın ya da kopyalayın ve `ANYPS5_SYSTEM_FONTS`'u ona yönlendirin.

### Konsol dili

Oyunlar konsolun dilini `sceSystemServiceParamGetInt` ile ister ve sahip olduklarında metinlerini o dilde gösterir. `ANYPS5_LANGUAGE` başka bir konsol dili numarasına ayarlanmadıkça İngilizce (Amerika Birleşik Devletleri)'dir: 0 Japonca, 1 İngilizce (ABD), 2 Fransızca, 3 İspanyolca, 4 Almanca, 5 İtalyanca, 6 Felemenkçe, 7 Portekizce (Portekiz), 8 Rusça, 9 Korece, 10 Çince (Geleneksel), 11 Çince (Basitleştirilmiş), 12 Fince, 13 İsveççe, 14 Danca, 15 Norveççe, 16 Lehçe, 17 Portekizce (Brezilya), 18 İngilizce (Birleşik Krallık), 19 Türkçe, 20 İspanyolca (Latin Amerika), 21 Arapça, 22 Fransızca (Kanada), 23 Çekçe, 24 Macarca, 25 Yunanca, 26 Romence, 27 Tayca, 28 Vietnamca, 29 Endonezce, 30 Ukraynaca. Örneğin `ANYPS5_LANGUAGE=21` Arapça'yı seçer. `ANYPS5_LANGUAGE=auto` (herhangi bir harf büyüklüğünde) dili bunun yerine ana bilgisayar işletim sisteminden alır: Linux'ta `LC_ALL`, `LC_MESSAGES` ve `LANG` içinden ayarlı ve boş olmayan ilki, bu yerel ayar `C` veya `POSIX` değilse önce `LANGUAGE` girdileriyle; Windows'ta kullanıcının yerel ayar adı ve macOS'ta tercih edilen dil listesi. Konsolun sahip olduğu bölgesel varyantlar korunur (`en-GB` İngilizce (Birleşik Krallık), `fr-CA` Fransızca (Kanada), `es-MX` İspanyolca (Latin Amerika), `pt-PT` Portekizce (Portekiz), `zh-TW`, `zh-HK` ve `zh-Hant` Çince (Geleneksel), `zh-CN` ve `zh-Hans` Çince (Basitleştirilmiş)) ve bölge için girdisi olmayan bir dil ana girdisini kullanır. Konsolun girdisi olmayan bir ana bilgisayar dili ile `C` ve `POSIX` yerel ayarları İngilizce (ABD) verir ve stderr'e yerel ayarı adlandıran bir satır yazdırır. Ana bilgisayar işlem başına bir kez okunur ve her sorgu o zaman aynı dili yanıtlar. Boş bir değer, ayarlanmamış bir değerle aynıdır; başka herhangi bir değer, oyunu ilk dil sorgusunda değişkeni adlandıran bir hatayla durdurur.

### Kayıt verisi boş alanı

Varsayılan olarak bir oyunun kayıt verisi disk alanı yetersizliği nedeniyle hiçbir zaman reddedilmez ve gördüğü boş bloklar kendi kaydının boş bloklarıdır. Kayıt verisini barındıran birim, kaydın boyutundan daha az yere sahipse yeni bir kaydın oluşturulmasını reddettirmek için `ANYPS5_FREE_SPACE=host` ayarlayın; böylece oyun diskin dolu olduğunu bildirebilir. Oyun hiçbir boyut istemediğinde kullanılan boyut olan 16384 bloklu bir kayıt 1 GiB gerektirir. Bir oyunun bir kayıt için okuduğu boş bloklar o zaman, biriminin hâlâ sahip olduğu yerle sınırlanır. Değişken ayarlanmadığında kullanılan değer olan `fixed` bugünkü davranışı korur ve boş olan dahil başka herhangi bir değer, oyunu bir sonraki kayıt bağlamasında ya da bir kayıt bulan kayıt aramasında `ANYPS5_FREE_SPACE`'i adlandıran bir hatayla durdurur. Windows'ta boş bir değer ayarlanmamış sayılır, çünkü `cmd` içinde `set ANYPS5_FREE_SPACE=` ve PowerShell'de `$env:ANYPS5_FREE_SPACE = ""` değişkeni kaldırır. Var olan kayıtlar her iki modda da eskisi gibi açılır. Bir konsolun bir kaydı aynı şekilde reddedip reddetmediği ölçülmemiştir; `libSceSaveData.native` için [teknik borca](../dev/debt/libSceSaveData.native.md) bakın.

### Ana bilgisayar oyun modu

Oyun çalıştığı sürece ana bilgisayardan oyun ya da performans modunu istemek için `APS5_GAME_MODE=on` ayarlayın. Varsayılan `off`'tur; bu, ana bilgisayara dokunmaz: hiçbir şey yüklenmez ve hiçbir şey istenmez.

Linux'ta istek [Feral GameMode](https://github.com/FeralInteractive/gamemode)'a gider: AnyPS5 başlarken `libgamemode.so.0`'ı (sonra `libgamemode.so`) `dlopen` ile yükler, `gamemoded`'dan oyun modunu başlatmasını ister ve oyun çıktığında isteği sonlandırır. `gamemoded`'ın bundan sonra neyi değiştirdiği (CPU yöneticisi, işlem önceliği, GPU ayarları) AnyPS5'ten değil, kendi yapılandırması olan `gamemode.ini`'den gelir. macOS'ta, App Nap ve zamanlayıcı birleştirmesini işlemden uzak tutan, kullanıcı tarafından başlatılmış, gecikmeye duyarlı bir `NSProcessInfo` etkinliğini tutar. Windows'ta yapılacak bir istek yoktur: AnyPS5 yüklendiğinde zaten işlem zamanlayıcılarını kısıtlamamasını ister.

`APS5_GAME_MODE=auto` aynı isteği yapar ama ana bilgisayar onu kabul edemediğinde (GameMode kütüphanesi yok, Foundation yok) hiçbir şey söylemez. `on` ile eksik bir kütüphane bir kez `[host-performance]` olarak günlüğe yazılır. Her iki modda da, örneğin `gamemoded` çalışmadığı için, daemon'un reddettiği bir istek bir kez günlüğe yazılır ve oyun her iki durumda da çalışmaya devam eder. Başka herhangi bir değer, oyunu açılışta `APS5_GAME_MODE`'u adlandıran bir hatayla durdurur.

### Eklentiler (Add-on)

Oyunlar bir eklentinin (indirilebilir içerik) kurulu olup olmadığını `sceNpEntitlementAccessGetAddcontEntitlementInfo` ile sorar ve kurulu olanları `sceNpEntitlementAccessGetAddcontEntitlementInfoList` ile listeler. Bir eklenti, yalnızca hak etme (entitlement) etiketi, çıktı çalıştırılabilir dosyasının yanındaki `anyps5-entitlements.ini` içinde ya da `ANYPS5_ENTITLEMENTS`'in adlandırdığı dosyada listelenmişse kurulu sayılır: satır başına bir etiket, en fazla 16 karakter; `#` veya `;` bir yorum başlatır. Kopyanızın içerdiği eklentileri listeleyin. Diğer her etiket kurulu değil olarak bildirilir ve her biri için ilk sorgu, `[entitlements] the title asked for the add-on EXAMPLEADDON0000, which anyps5-entitlements.ini does not list, so it is reported as not installed` gibi bir satır yazdırır. Satır dosyayı `anyps5-entitlements.ini` olarak ya da ayarlıysa `ANYPS5_ENTITLEMENTS`'in verdiği yolla adlandırır. Yazdırılabilir ASCII dışındaki etiket baytları ve ters eğik çizgi `\xNN` olarak yazılır. 64 böyle etiketten sonra bir satır daha, sonrakilerin satırsız bildirildiğini söyler. Bazı oyunlar, onsuz devam etmek yerine açılışta aradıkları bir eklentiyi bekler: bir oyun (PPSA26344, sürüm 02.002.000), istediği eklenti etiketi listelenene kadar yükleme göstergesinde kalır.

### Bağlantıları ana bilgisayar tarayıcısında açma

`sceSystemServiceLaunchWebBrowser` ile bir web sayfası isteyen bir oyun başarısızlık alır ve hiçbir şey açılmaz; `ANYPS5_OPEN_URLS` ayarlanmadıkça. `ANYPS5_OPEN_URLS=ask`, adresin tamamını varsayılan düğmesi İptal olan bir mesaj kutusunda gösterir ve yalnızca Aç'a bastığınızda varsayılan tarayıcıda açar; kutuyu gösterecek ekran yoksa hiçbir şey açılmaz. `ANYPS5_OPEN_URLS=always` onu sormadan açar; adresi oyun seçtiği için bu önerilmez. Adresi önce gördüğünüz `ask`'in aksine `always`, oyunun adlandırdığı her `http` veya `https` ana bilgisayarını açar; `localhost` ve bir yönlendiricinin web sayfası gibi özel ağ adresleri dahil. `never`, boş bir değer ve ayarlanmamış bir değişken başarısızlığı korur. Başka herhangi bir değer, oyunu ilk tarayıcı isteğinde değişkeni adlandıran bir hatayla durdurur.

Tarayıcı istekleri tüm adresler ve tüm oyun iş parçacıkları için birlikte sınırlıdır: bir kutu ya da başlatma hâlâ etkinken gelen istek hemen reddedilir, herhangi bir 60 saniyede en fazla üç kutu veya başlatma gerçekleşir ve İptal'e bastıktan sonra 10 dakika boyunca başka kutu ya da başlatma olmaz. Reddedilen istekler varsayılan gibi başarısız olur.

Yalnızca en fazla 2048 bayt uzunluğundaki `http://` ve `https://` adresleri açılır. Ana bilgisayar; kullanıcı adı veya parola, boşluk, kontrol karakteri, ASCII dışı bir karakter (punycode ve yüzde kaçışlarını kullanın), hatalı biçimlendirilmiş ya da kontrol karakterli bir yüzde kaçışı, boş bir ana bilgisayar ya da 1 ile 65535 dışında bir port içeren adresleri ve `psno:` adreslerini reddeder. Tarayıcı kabuksuz başlatılır: Linux'ta `xdg-open`, macOS'ta `open` ve Windows'ta `ShellExecuteW`.

### Konsol tarih ve saat biçimi

Oyunlar ayrıca konsolun tarih biçimini ve saat biçimini `sceSystemServiceParamGetInt` ile ister. Hiçbir ayar olmadan tarih biçimi gün, ay, yıl ve saat biçimi 24 saatliktir. `ANYPS5_DATE_FORMAT` tarih biçimini 0 (yıl, ay, gün), 1 (gün, ay, yıl) veya 2 (ay, gün, yıl) olarak ayarlar ve `ANYPS5_TIME_FORMAT` saat biçimini 0 (12 saatlik) veya 1 (24 saatlik) olarak ayarlar. Örneğin `ANYPS5_DATE_FORMAT=2` ile `ANYPS5_TIME_FORMAT=0`, ay, gün, yıl ve 12 saatlik bir saat verir. Her iki değişken de, seçimi ana bilgisayarın kısa tarih ve saat desenlerinden alan `auto` değerini (herhangi bir harf büyüklüğünde) kabul eder: Linux'ta `LC_ALL`, `LC_TIME` ve `LANG` içinden ayarlı ve boş olmayan ilkinin adlandırdığı yerel ayar (`LANGUAGE` ve `LC_MESSAGES` sayılmaz), Windows'ta Bölge ayarlarından kullanıcının kısa tarih ve saat biçimleri ve macOS'ta geçerli yerel ayar. Yalnızca gün, ay ve yılın sırası ve saatin 12 mi 24 saatlik mi olduğu kullanılır; ayırıcılar ve adlar kullanılmaz. Ana bilgisayarın bir tercihi olmadığında (hiçbir şey ayarlı değil, `C` ve `POSIX` yerel ayarları, kurulu olmayan bir yerel ayar) ya da tarih deseninin konsolun biçimi olmayan bir sırası olduğunda, değer 1 kalır ve stderr'e değişkeni adlandıran bir satır yazılır. Ana bilgisayar işlem başına bir kez okunur, bu yüzden her sorgu o zaman aynı yanıtı verir. Saat dilimi ve yaz saati zaten ana bilgisayarı izler. Boş bir değer, ayarlanmamış bir değerle aynıdır; başka herhangi bir değer, oyunu o parametrenin ilk sorgusunda değişkeni adlandıran bir hatayla durdurur.

### Kupalar (Trophies)

Bir oyun, `libSceNpUniversalDataSystem`'e int32 bir `_trophy_id` özelliğiyle bir `_UnlockTrophy` olayı göndererek ya da aşağıda açıklanan istatistik kuralları aracılığıyla bir kupanın kilidini açar. Kilit açma, oyunun çalıştığı dizinde (`app0/`'ı barındıran) `_trophy/<user id>_<service label>.txt` dosyasında, kupa başına bir `<trophy id> <unlock time>` satırı olarak saklanır; zaman bir RTC tikidir, 0001-01-01 UTC'den itibaren mikrosaniyedir. Kupalarını yeniden kilitlemek için oyunu başlatmadan önce dosyayı silin. Her yeni kilit açma, `[trophy] PPSA01556 unlocked trophy 12 for user 268435456` gibi bir satır yazdırır; bir kupanın kilidini yeniden açmak hiçbir şey yazdırmaz. `sceNpTrophy2GetTrophyInfo`, `sceNpTrophy2GetTrophyInfoArray` ve `sceNpTrophy2GetGameInfo`, saklanan kilit açmaları ve zamanlarını oyuna bildirir. Oyunun kupa verisi yüklendiğinde (aşağıya bakın), verinin içermediği bir kupanın, platinin ya da ilerlemeli bir kupanın gönderilen kilit açması saklanmaz ve nedeni yazdırır; örneğin `[trophy] ignored the posted unlock of trophy 0 for user 268435456: it is the platinum, which unlocks with the last trophy that names it`; platin, onu adlandıran son kupayla kilidi açılır ve ilerlemeli bir kupa istatistiği aracılığıyla açılır. Kupa verisi olmadan 0 veya daha büyük her gönderilen kimlik saklanır ve diğer kupalar açıldığında platin açılmaz.

Kupa verisi oyunun kendi `app0/sce_sys/trophy2/trophyNN.ucp` dosyasından gelir; burada `NN` servis etiketidir (çoğu oyun için `00`). Kupa sorguları; adlarını, açıklamalarını ve ödüllerini konsol dilinde (`ANYPS5_LANGUAGE`, yukarıya bakın), o dilde metni yoksa İngilizce (ABD) ya da verinin varsayılan dilinde ve derecelerini, gruplarını, gizli bayraklarını, kupa sayısını ve derece sayılarını bildirir. Bir platin kupa, `platinumTrophyId` içinde onu adlandıran her kupa açılır açılmaz kilidi açılır. İlk kupa sorgusu ya da olayı, okunanı yazdırır; örneğin `[trophy] /app0/sce_sys/trophy2/trophy00.ucp: 52 trophies, 1 group, texts in en-US`.

Birçok oyun kupaları bunun yerine istatistik aracılığıyla açar: `app0/sce_sys/uds/udsNN.ucp`, her olay için değeri bir istatistiğe dönüşen özelliği adlandırır ve bir kupanın `unlockCondition` değeri bir istatistiği bir hedefle karşılaştırır. Her istatistiğin son değeri, istatistik başına bir `<stat id> <value>` satırı olarak `_trophy/<user id>_<service label>.stats.txt` içinde saklanır ve çalıştırmalar arasında korunur; baştan başlamak için onu kilit açma dosyasıyla birlikte silin. İlerlemeli bir kupa istatistiğini ilerleme olarak bildirir. Yalnızca en son değeri (`"aggregation":"latest"`) ya da en büyük değeri (`"max"`) `uint32`, `uint64` veya `int32` olarak tutan istatistikler ve adıyla seçilen bir olayın bir özelliğini kopyalayan kurallar (`$.name` veya `$.object.name`; isteğe bağlı olarak yalnızca bir özellik bir tamsayıya eşit olduğunda, `_UpdateTrophyProgress` için `_trophy_id` ile) uygulanır; diğerleri dosya okunduğunda `[trophy]` satırlarında listelenir.

`APS5_TRACE_UDS=1`, gönderilen her olayı, örneğin `[uds] context 1 posted _UnlockTrophy {"_trophy_id":12}`, bir olayın ayarladığı her istatistiği, örneğin `[uds] stat 10 of user 268435456 is 10 (COLLECT $.counter)`, ve eksik bir `trophyNN.ucp` veya `udsNN.ucp` için bir `[uds]` satırını yazdırır. Kullanılmayan bir olay, nedenini bir `[trophy] ignored` satırında yazdırır: 0 veya daha büyük bir int32 `_trophy_id` olmayan ya da var olmayan bir bağlama gönderilmiş bir `_UnlockTrophy` olayı; ve kural başına bir kez, özelliği eksik, negatif, tamsayı olmayan ya da istatistiğin aralığı dışında olan bir olay.

Eksik veya hatalı biçimlendirilmiş bir `trophyNN.ucp` ya da `udsNN.ucp`, yalnızca sağladığı şeyi kapatır ve hatalı biçimlendirilmiş olan nedenini bir kez yazdırır: kupa verisi olmadan sorgular yer tutucular bildirir ve istatistikler hiçbir şeyin kilidini açmaz; istatistik kuralları olmadan olaylar hiçbir istatistiği değiştirmez. Simgeler okunmaz: `sceNpTrophy2GetGameIcon`, `sceNpTrophy2GetGroupIcon` ve `sceNpTrophy2GetTrophyIcon` oyunu bir hatayla durdurur ve `sceNpTrophy2GetRewardIcon` `SCE_NP_TROPHY2_ERROR_ICON_FILE_NOT_FOUND` döndürür. `_trophy/` içinde hasarlı (yeni bir `<id> <value>` çifti olmayan bir satır, örneğin bir düzenlemenin boş bıraktığı ya da satır başı karakteriyle bitirdiği bir satır) ya da okunamayan veya yazılamayan bir kilit açma ya da istatistik dosyası da aynı sonucu verir. Hiçbir bildirim gösterilmez ve `sceNpTrophy2RegisterUnlockCallback` ile kaydedilen geri çağrılar çağrılmaz.

### Meta verisiz PlayGo dump'ları

Bir dump'ta ne `sce_sys/playgo-chunk.dat` ne de `sce_sys/playgo-scenario.json` varsa, AnyPS5 sıfırdan farklı hangi PlayGo chunk kimliklerinin var olduğunu söyleyemez. Varsayılan olarak, `sce_sys` hiçbir `playgo-` dosyası barındırmadığında, 0 ile 999 arasındaki chunk kimlikleri kurulu (`LOCAL_FAST`) olarak yanıtlanır ve 1000 ve üzerindeki kimlikler `SCE_PLAYGO_ERROR_BAD_CHUNK_ID` döndürür; 1000 sınırı bir konsoldan değil, oyun çalıştırmalarından gelir. O dump için başka bir yanıt seçmek üzere `APS5_PLAYGO_UNLISTED_CHUNKS` ayarlayın:

| Değer | Listelenmemiş bir chunk kimliği için yanıt | Buna ihtiyaç duyan gözlemlenmiş oyun |
|-------|---------------------------------|------------------------------|
| `installed` | Kurulu (`LOCAL_FAST`) olarak bildir. | Bilinen yok: kayıtlarını yüklemek için buna ihtiyaç duyan High on Life (PPSA15388) yalnızca varsayılanın yanıtladığı 0 ile 999 arasındaki kimlikleri sorar. |
| `absent` | `SCE_PLAYGO_ERROR_BAD_CHUNK_ID` döndür. | Sifu (PPSA03001), Demon Slayer: Kimetsu no Yaiba - The Hinokami Chronicles (PPSA03490) ve Guilty Gear -Strive- (PPSA02257) chunk kimliği aramalarını sonlandırmak için bu sonucu kullanır. `installed` ile PPSA03490 daha yüksek chunk kimliklerini sonsuza dek sormaya devam eder; bu yüzden chunk listesi, açılış `Ran out of memory allocating 5565054976 bytes` hatasıyla başarısız olana kadar büyür. PPSA02257, 30 saniyede yaklaşık 245 milyon `scePlayGoGetLocus` çağrısından sonra aynı şekilde başarısız olur. |
| Virgülle ayrılmış kimlik listesi, ör. `0,1` | Bu kimlikleri kurulu (`LOCAL_FAST`) ve listelenmemiş diğer her kimliği yok (`SCE_PLAYGO_ERROR_BAD_CHUNK_ID`) olarak bildir. | The Medium (PPSA03718) chunk 0 ve 1'e ihtiyaç duyar; `installed` ile her kimliği sorar ve UE4 belleği tüketir. |

Yukarıdaki varsayılanı korumak için değişkeni ayarlamayın. `playgo-scenario.json` içeren ama `playgo-chunk.dat` içermeyen bir dump, 0 ile 999 arasındaki kimlikleri listelenmiş sayar ve 1000 ve üzerindeki bir kimlik, değişken ayarlanmadıkça yine bir hatayla durur. Bir liste 0 ile 65535 arasında ondalık chunk kimlikleri içerir ve chunk 0'a eklenir; yalnızca dump'ta ne `playgo-chunk.dat` ne de `playgo-scenario.json` bulunduğunda uygulanır. Başka herhangi bir değer de bir hatayla durdurur. `playgo-chunk.dat` veya `playgo-scenario.json` içeren dump'lar etkilenmez. Bu oyuna özgü sonuçlar emülatör çalıştırmalarından gelir ve bir konsola karşı doğrulanmamıştır.

### Ekran görüntüleri

Oyun penceresinde F12'ye basmak, bir sonraki görüntülenen kareyi, pencerenin ölçeklemesi, letterbox'ı veya [performans katmanı](#performans-katmanı-overlay) olmadan, oyunun ekran arabelleğinin çözünürlüğünde PNG olarak kaydeder. BT.2100 PQ ekran arabelleği SDR'ye dönüştürülerek kaydedilir; [HDR çıkışı](#hdr-çıkışı) ile gösterildiğinde de. Dosyalar `<title ID>-<YYYYMMDD-HHMMSS>.png` olarak adlandırılır ve çıktı çalıştırılabilir dosyasının yanındaki, gerektiğinde oluşturulan bir `anyps5-screenshots/` dizinine yazılır; başka bir dizin kullanmak için `ANYPS5_SCREENSHOTS` ayarlayın. Kaydedilen yol stderr'e yazdırılır. Önceki ekran görüntüsü hâlâ yazılırken yapılan basışlar tek bir basışta birleştirilir. Tuş, [girdi eşlemesinin](INPUT_MAPPING.tr.md) `Screenshot` eylemidir. Yazılamayan bir ekran görüntüsü oyunu hatayla durdurur.

### Ana bilgisayar ses düzeyi ve sessize alma

F5 ana bilgisayarın oyun penceresi için çıkış ses düzeyini düşürür, F6 yükseltir ve F8 sessize alır; tuşlar [girdi eşlemesindeki](INPUT_MAPPING.tr.md) `VolumeDown`, `VolumeUp` ve `ToggleMute` ile değiştirilebilir. Bu, bir medya oynatıcının ses düzeyi gibi, ana bilgisayarın bir denetimidir: konsol davranışı değildir, oyun onu görmez ve oyunun kendi ses ayarları değiştirilmez.

Ses düzeyi 0 ile 100 arasında 5'lik adımlarla gider ve 100'de ya da `ANYPS5_VOLUME`'da (0 ile 100 arasında bir tamsayı) başlar. Çıkış genliği, 100'ün oranı olarak ses düzeyinin karesidir, yani 50 genliğin dörtte biridir. Sessize alma, ses düzeyinden ayrıdır: sessizden çıkarmak ses düzeyini geri getirir ve sessizdeyken bir ses düzeyi tuşu sessizden çıkarır. Kazanç değişikliği, sıçramak yerine 10 ms boyunca hareket eder. Her basış, ortaya çıkan düzeyi ya da sessiz durumu stderr'e bir satır olarak yazdırır; ekran üstü bir gösterge yoktur.

Ses düzeyi 100'de ve sessiz değilken ses, hiçbir işlem olmadan ana bilgisayar aygıtına değiştirilmeden iletilir. Ses düzeyi, oyunun `libSceAudioOut` ve `libSceAudioOut2` üzerinden çaldığına, `libSceAudio3d`'nin bunlar üzerinden çaldıkları dahil, uygulanır. Kumandanın kendi ses kartı ve dokunsal geri bildirimleri (haptics) ile mikrofon etkilenmez.

Ekran penceresi odakta değilken sesi sessize almak ve odak geri döndüğünde geri yüklemek için `ANYPS5_MUTE_ON_FOCUS_LOSS=1` ayarlayın. `0` veya ayarlanmamış, sesi olduğu gibi bırakır. Tuşla ayarlanan sessize almayı değiştirmez: sessize aldığınız bir pencere odağı geri kazandığında sessiz kalır. Yalnızca odak olayları sayılır, bu yüzden odağı kaybetmeden simge durumuna küçültülen bir pencere duyulabilir kalır ve odaksız açılan bir pencere sessiz başlar. Her iki değişkenin de geçersiz bir değeri, boş olan dahil, oyunu onu adlandıran bir hatayla durdurur.

Tuşlar, ekran penceresi odaktayken çalışır. Gerçek bir ses aygıtında ya da bir konsolda ölçülmemiştir.

Ekran penceresi odakta değilken guest'e sessiz mikrofon örnekleri döndürmek için `ANYPS5_AUDIO_IN_MUTE_ON_FOCUS_LOSS=1` ayarlayın. Yakalama SDL'de açık kalır, ancak sıraya alınmış örnekler odak değiştiğinde temizlenir; böylece sessizdeyken yakalanan konuşma odak geri döndükten sonra iletilmez. `0` veya ayarlanmamış, mikrofon girdisini değiştirmez; geçersiz bir değer oyunu değişkeni adlandıran bir hatayla durdurur.

### GPU seçimi

Oyun, grafik ve hesaplama kuyrukları ile swapchain sunumuna sahip ilk Vulkan 1.1 aygıtında çalışır; ayrık bir GPU'yu tümleşik olana tercih eder. Başka bir aygıtta çalıştırmak için `ANYPS5_GPU` değerini, büyük/küçük harf gözetmeden karşılaştırılan bir aygıt adı parçasına ayarlayın; adlar açılışta `Physical device candidate` satırlarında yazdırılır. Kullanılabilir hiçbir aygıt bu metni içermediğinde başlatma başarısız olur ve hata aygıt adlarını listeler.

Pencere dikey eşzamanlama (`fifo`) ile sunar. Oyunun ekranı beklemesini önlemek için `ANYPS5_PRESENT_MODE` değerini `fifo_latest_ready`, `mailbox` veya `immediate` olarak ayarlayın: `fifo_latest_ready` ve `mailbox` yine de ekranın yenileme hızında tam kareler gösterir ve aradakileri atar, `immediate` hemen sunar ve yırtılma (tearing) yapabilir. Başka herhangi bir değerde ve yüzey modu sunmuyorsa başlatma başarısız olur (`fifo_latest_ready`, `VK_KHR_present_mode_fifo_latest_ready` veya EXT biçimini gerektirir). Flip'ler her modda 59,94 Hz'de hızlandırılmış kalır. Bir Wayland kompozitörü, göstermediği bir pencereye, örneğin başka bir çalışma alanındaki bir pencereye, kare iletmeyi bırakabilir: NVIDIA sürücüsüyle Hyprland'de bir `fifo` swapchain o zaman yaklaşık saniyede bir kez sunar; `fifo_latest_ready` ve `immediate` ise oyunu normal hızında tutar.

En fazla bu kadar kare sunmak için `ANYPS5_FPS_LIMIT` değerini saniyedeki kare sayısına ayarlayın; 0,5 ile 1000000 arasında, en fazla üç ondalıkla (`30`, `59.94`). Pencerenin her flip'i sunulmadan önce sırasını bekler ve oyunun flip'i o zaman tamamlanır; böylece oyun o hızdaki bir ekran tarafından olduğu gibi hızlandırılır; oyuna bildirilmez ve vblank olayları ile yenileme hızı çıkışınkiler olarak kalır. `0`, boş bir değer ve ayarlanmamış bir değişken sınır yok anlamına gelir; değişkensiz davranıştır. Başka herhangi bir değer, oyunu ilk flip'inde değişkeni adlandıran bir hatayla durdurur.

Sınır, yukarıdaki flip hızlandırmasının üzerine uygulanır; bu, flip'leri her sunum modunda çıkışın yenileme hızında tutar (59,94 Hz ya da o adlı çıkış modunda 119,88 Hz; oyun 1 veya 2 flip hızı ayarladığında her ikinci ya da üçüncü vblank): hızı bunun altına düşürür ve üzerinde hiçbir şey yapmaz; sınırdan zaten yavaş olan bir oyun geciktirilmez. Bir duraklama bir patlamayla telafi edilmez: bir periyottan az gecikmiş bir kare bir sonraki tarafından telafi edilir ve daha uzun bir duraklamadan sonra en fazla bir kare erken sunulur. Bekleme, son teslim tarihine kısa bir süre kalana kadar uyur ve kalanı için döner. Pencere olayları flip'ler arasında işlenir, bu yüzden düşük bir sınırda pencere yaklaşık kare başına bir kez yanıt verir. `fifo` ile ekran yine de her yenilemesinde bir kare gösterir, bu yüzden yenileme hızını bölmeyen bir sınır, bazı kareleri bir yenileme, bazılarını iki yenileme boyunca gösterir. Sınır bir ana bilgisayar seçimidir; konsolun yoktur.

Windows'ta, OpenCL, OpenGL ve Vulkan Compatibility Pack, D3D12 aygıtlarını Vulkan üzerinden sunan Mesa Dozen'i kurabilir. Bazı D3D12 sürücüleri, AnyPS5 yerel bir Vulkan aygıtı seçecek olsa bile, Vulkan yükleyicisi bağdaştırıcıları numaralandırırken çökebilir. Bu olursa ve sistemde başka bir Vulkan sürücüsü varsa, AnyPS5'i başlatmadan önce geçerli PowerShell oturumu için Dozen'i devre dışı bırakın:

```powershell
$env:VK_LOADER_DRIVERS_DISABLE = '*dzn*'
.\app.exe
```

Kullanılabilir tek Vulkan sürücüsü Dozen ise onu devre dışı bırakmayın; uygulamanın kullanacağı aygıt kalmaz. `VK_LOADER_DRIVERS_DISABLE`, Vulkan sürücü manifesto dosya adlarıyla eşleşir ve 1.3.234 veya daha yeni başlıklarla derlenmiş bir Vulkan yükleyicisi gerektirir ([yükleyici belgeleri](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderInterfaceArchitecture.md#filter-environment-variables)). 1.4.365 sürümüne kadarki yükleyiciler onu yükseltilmiş bir işlemde, örneğin **Run as administrator** ile açılan bir PowerShell'den başlatılanda, yok sayar.

Windows'ta NVIDIA sürücüsü 610.47, işlem hatları oluşturulurken shader derleyicisinde çökebilir: günlük, bir sürücü iş parçacığında `rip` içinde `nvgpucomp64.dll` ile `FATAL: unhandled exception 0xc0000005` ile biter. Bir RTX 3060 üzerinde PPSA25344, 3 New Game yüklemesinin 3'ünde, `APS5_NO_PIPELINE_LTO=1` ile de, bu şekilde durdu; sürücü 617.42 ile aynı derleme ve sahne 40 dakika çalıştı. Çökme `nvgpucomp64.dll` içindeyse sürücüyü güncelleyin.

### Ana bilgisayar raporu

Bir hata raporunun ihtiyaç duyduğunu, oyunun Vulkan aygıtı seçildiğinde bir kez stderr'e yazdırmak için `ANYPS5_HOST_REPORT=1` ayarlayın. Bloğun tamamını sorun kaydına ekleyin; işletim sistemini, GPU'yu ve sürücüyü elle yazmanın yerini alır. `0`, boş bir değer veya değişken olmaması hiçbir şey yazdırmaz ve başka herhangi bir değer, başlatmayı değişkeni adlandıran bir hatayla durdurur.

Raporun sabit bir sırada sabit bölümleri vardır, bu yüzden bir makinedeki iki çalıştırma aynı metni yazdırır: `[system]` (commit, işletim sistemi (Linux ve macOS'ta çekirdek sürümü ve makine türü, Windows'ta sürüm ve derleme), CPU modeli, donanım iş parçacıkları, MiB cinsinden fiziksel bellek ve CPU denetimi), `[sdl]` (çalışma zamanında bağlanan SDL sürümü ve karşısında derlenen sürüm), `[gpu]` (aygıt adı ve türü, PCI satıcı ve aygıt kimlikleri, Vulkan sürümü, `VK_KHR_driver_properties`'ten sürücü adı ve bilgisi, NVIDIA'nın sürücüsü, Intel'in Windows sürücüsü ya da Vulkan'ın sürüm makrolarının kodladığı şekilde çözülen sürücü sürümü (`VK_KHR_driver_properties` olmadan ve MoltenVK'de `unknown`) ve ham değeri, alt grup boyutu, en büyük aygıt-yerel yığın, bulunan her Vulkan aygıtı, yüzeyin sunum modları ve `ANYPS5_PRESENT_MODE`'un istediği), `[vulkan]` (oyunun ihtiyaç duyduğu her uzantı ve özellik ile kullandığı isteğe bağlı uzantılar, `present` ya da `missing` olarak işaretli), `[shader_cache]` (`disabled`, `absent` veya MiB cinsinden boyutuyla `present`) ve `[environment]` (ayarlı `APS5_`, `ANYPS5_`, `VK_` ve `SDL_` değişkenleri). Okunamayan bir bilgi `unavailable` yazdırır ve son bir `[unavailable]` bölümünde açıklanır. `cpu_gate` satırı, rapor göründüğü sürece `passed` der, çünkü gerekli bir uzantıdan yoksun bir ana bilgisayar CPU'su, Vulkan aygıtı seçilmeden önce oyunu durdurur (Girdi ve dönüştürme altındaki CPU gereksinimine bakın); macOS gibi o denetimin çalışmadığı yerde `unavailable` der. Shader önbelleği boyutu en fazla 200000 dosyayı sayar.

Rapor kullanıcı adı, ana bilgisayar adı, seri numarası veya MAC adresi içermez. Yola benzeyen herhangi bir sözcük (`/` veya `~` ile başlayan, ters eğik çizgi içeren ya da sürücü harfiyle başlayan) `<path>` olarak, URL'ler `<url>` olarak, MAC adresleri `<redacted>` olarak yazdırılır ve hesap adı, ana bilgisayar adı ve ev dizini adı, tam sözcük olarak göründükleri her yerde değiştirilir. Yine de yayınlamadan önce metni kontrol edin, çünkü listelediği değişkenlerin değerleri ayarlandığı gibi yazdırılır.

Seçilen GPU, oyunun gerektirdiği bir uzantıdan ya da özellikten yoksunsa (`VK_KHR_8bit_storage`, `VK_KHR_buffer_device_address`, `VK_KHR_shader_float_controls`, `storageBuffer8BitAccess`, `bufferDeviceAddress`, `shaderInt64`, `shaderSignedZeroInfNanPreserveFloat32`, `vertexPipelineStoresAndAtomics`, `fragmentStoresAndAtomics`, `samplerAnisotropy`, `textureCompressionBC` ve bir pencere için `VK_KHR_swapchain`), başlatma, GPU'yu, sürücüsünü ve Vulkan sürümünü adlandıran, eksik her öğeyi listeleyen ve `ANYPS5_GPU`'nun seçebileceği diğer Vulkan aygıtlarına işaret eden tek bir mesajla başarısız olur. `ANYPS5_HOST_REPORT=1` ile rapor önce yazdırılır, bu yüzden başarısız bir başlatmada da raporu vardır.

### HDR çıkışı

HDR çıkışı yalnızca Windows'ta kullanılabilir. Oyunlar onu, pencereyi gösteren monitör HDR modunda çalıştığında (Ayarlar > Ekran > HDR kullan), pencerenin Vulkan yüzeyi 10 bitlik biçimde bir HDR10 (`VK_COLOR_SPACE_HDR10_ST2084_EXT`) swapchain sunduğunda, örnekte `VK_EXT_swapchain_colorspace` ve aygıtta `VK_EXT_hdr_metadata` olduğunda görür. Monitörün parlaklığı DXGI'den gelir; tepe parlaklık bildirmeyen bir monitör, 1000 nit tepe ve tam kare, 0 nit siyah olarak alınır. Linux'ta ve diğer ana bilgisayarlarda çıkış her zaman SDR'dir, çünkü yüzeyin biçimleri ekranın HDR'de olup olmadığını göstermez. Bir oyunun BT.2100 PQ ekran arabellekleri o zaman ekrana değiştirilmeden ulaşır; HDR çıkışı olmadan SDR'ye dönüştürülür.

### Shader önbelleği

Yeniden derlenmiş shader'lar ve Vulkan işlem hattı önbelleği, çıktı çalıştırılabilir dosyasının yanındaki bir `shader_cache/` dizininde tutulur; böylece sonraki çalıştırmalar yeniden derlemeyi atlar. Başka bir dizin kullanmak için `ANYPS5_SHADER_CACHE_DIR` ayarlayın ya da her iki önbelleği de devre dışı bırakmak için `ANYPS5_NO_SHADER_CACHE` değerini `0` dışında boş olmayan herhangi bir değere ayarlayın. Shader'lar, yeniden derleyicinin kaynak sürümünün adını taşıyan bir alt dizinde saklanır; 14 gündür kullanılmayan diğer sürümlerin alt dizinleri, bir çalıştırma bir shader'ı ilk aradığında kaldırılır. İşlem hattı önbelleği, aynı alt dizinde GPU ve sürücü sürümü başına bir `vk-pipelines-*.bin` dosyasıdır; böylece derlendiği shader'larla birlikte gider. Geçerli sürümün girdileri hiçbir zaman kaldırılmaz ve dizinin boyut sınırı yoktur, bu yüzden çok sayıda shader derleyen bir oyun dakikalar içinde birkaç GB ekleyebilir. Dizini silmek güvenlidir: sonraki çalıştırmada yeniden oluşturulur.

İlk çalıştırma, GPU sürücüsü işlem hattını derlerken çok büyük bir hesaplama shader'ında durabilir: yeni kare gösterilmez ve sürücünün derleyicisi birkaç CPU iş parçacığını meşgul eder; bu, birkaç milyon SPIR-V sözcüğünden oluşan shader'lar için dakikalardan saatlere sürebilir. `APS5_PROFILE_DRAW=1` ile günlük, böyle bir derlemeyi başlamadan önce adlandırır (`[dispatch] creating a pipeline for N SPIR-V words`, 100000 sözcüğün üzerinde yazdırılır) ve bittiğinde süresini yazar (`[dispatch] created a pipeline for N SPIR-V words in N ms`). Oyunun commit edilmiş belleği derleme sırasında büyümeye devam edebilir, bu yüzden sistem commit sınırının (bkz. Çalışma zamanı yerleşimi) buna yer olması gerekir; Windows'ta böyle bir çalıştırmadan önce sayfa dosyasını büyütün. Sonraki çalıştırmalar sürücünün işlem hattı önbelleğini yeniden kullanabilir ve işlem hattı günlüğü (journal) kaydedilen işlem hatlarını arka plan iş parçacıklarında yeniden oluşturur; bu, onlara ilk erişildiğinde beklemeyi önleyebilir; hiçbiri sürücünün derlemeyi atlayacağını garanti etmez ve bir sürücü güncellemesi ya da başka bir GPU ayrı bir önbellek başlatır.

### İşlem hattı günlüğü (Pipeline journal)

Yeniden derlenmiş shader'lara ek olarak shader önbelleği dizini `pipelines.journal` dosyasını tutar: işlem hattı deposunun tuttuğu her Vulkan grafik işlem hattı ve bir çalıştırmanın oluşturduğu her hesaplama işlem hattı, oluşturuldukları özelleştirilmiş shader modülleriyle birlikte. Oyunun bir sonraki çalıştırması, yüklenirken bu işlem hatlarını arka plan iş parçacıklarında oluşturur; böylece önceki çalıştırmanın zaten yaptığı bir çizim ya da dispatch, ilk erişildiğinde işlem hattını derlemez. Günlük, yanındaki sürücünün işlem hattı önbellek dosyası gibi, onu yazan GPU ve sürücüye aittir: başka bir GPU ya da sürücü sürümündeki bir çalıştırma onu baştan başlatır ve sürücünün kendi işlem hattı önbelleği her işlem hattının ne kadar hızlı yeniden oluşturulacağına karar verir. Dizini paylaşan iki çalıştırma `pipelines.journal.lock` üzerinde sırayla çalışır; kilitleyemeyen bir çalıştırma hiçbir şey kaydetmez. `APS5_NO_PIPELINE_JOURNAL=1` kaydı ve yeniden oynatmayı devre dışı bırakır; `APS5_PIPELINE_JOURNAL_THREADS` yeniden oynatma iş parçacıklarını ayarlar (varsayılan olarak CPU iş parçacıklarının yarısı; `0` yeniden oynatmadan kaydeder), `APS5_PIPELINE_JOURNAL_ENTRIES` kaç işlem hattının yeniden oynatılacağını (4096, en son kaydedilenler) ve `APS5_PIPELINE_STORE_ENTRIES` kaç grafik işlem hattının bellekte kalacağını (varsayılan olarak depo, yeniden oynatmayı ve geri gelmeye devam eden işlem hatlarını tutmak için 1024'ten 4096'ya büyür) ve `APS5_PIPELINE_JOURNAL_MAX_MIB` (1024) kaydın durduğu dosya boyutunu ayarlar. Dosyayı silmek, ön avantajdan başka hiçbir şeyi kaybettirmez. `APS5_PROFILE_DRAW=1` ile bir `[pipeline-journal]` satırı neyin yüklendiğini, yeniden oynatıldığını ve kaydedildiğini bildirir.

### Çizim hattı (Draw pipeline)

`APS5_DRAW_PIPELINE=1` (varsayılan olarak kapalı), her çizimin daha büyük bir kısmını oyunun komut arabelleklerini okuyan iş parçacığından uzaklaştırır: ikinci bir iş parçacığı, bir sonraki çizim okunurken her çizimi bitirir; çizimleri kaydeden iş parçacığı, girdileri değişmeyen işi atlar ve bir çizimin kaynaklarının hâlâ güncel olduğuna dair denetimler, oyun ya da GPU belleğe yazana kadar yeniden kullanılır. `APS5_PIPELINED_DRAWS=32 APS5_PIPELINE_DISPATCH=1` ile birlikte kullanılması amaçlanmıştır. Görüntü değişmez; değişirse bu bir hatadır ve anahtarsız bir çalıştırma nedenin o olup olmadığını gösterir. `APS5_VERIFY_DRAW_PIPELINE=1`, tam denetimleri yeniden kullanılanların yanında, hız pahasına çalıştırır ve kaçının farklı olduğunu belirten `[epoch-proofs]` satırları yazdırır (sıfırdan farklı herhangi bir sayı bir hatadır). `APS5_PROFILE_DRAW=1` her çizimi okuma iş parçacığında bitirir, bu yüzden onunla bir çalıştırma bu anahtarı ölçmez. Bunu onsuz görmek için `APS5_PIPELINE_STATS` ayarlayın (herhangi bir değer; `1` ayrıca zaman maliyeti olan Vulkan işlem hattı istatistiklerini açar): her 10 saniyede bir bir `[finisher]` satırı ikinci iş parçacığına devredilen çizimleri ve okuma iş parçacığında bitirilenleri sayar ve `[finisher-held]` satırları, yazmalarının bilinmeme nedenine göre geri tutulan çizimleri sayar.

### Doku önbelleği bütçesi

İmaj ve arabellek bellek ayırmaları, kullanılmayan havuzlanmış arabellekler ve geri kazanılabilir doku önbelleği girdileri serbest bırakıldıktan sonra yeniden denenir. Kurtarma, bekleyen GPU sonuçları olan depolama imajlarını ve kaydedilmiş işin tuttuğu kaynakları korur. `[bufferpool]` kurtarma ve hata satırları istenen ayırmayı, serbest bırakılan belleği ve yeniden deneme sayısını bildirir. Kurtarma yeterli belleği boşaltamazsa ayırma yine başarısız olur. Aygıt-yerel yığının `VK_EXT_memory_budget` bütçesinde bir imaj, aygıt arabelleği veya yerleşik yığın yansıma slab'ı için yer kalmadığında, doku önbellekleri önce ayırma için ve yığının bütçesini aştığı miktar için geri kazanılabilir girdileri serbest bırakır; ayırma yalnızca bunlar bundan azını serbest bırakırsa ve yeniden okunan bütçede hâlâ yer yoksa ana bilgisayar belleğine gider.

Örneklenmiş dokular ve depolama imajları, bütçelerini aştıklarında en az yakın zamanda kullanılan girdilerini çıkaran iki önbellekte aygıt belleğinde kalır. Depolama imajı bütçesi, en büyük aygıt-yerel yığının `VK_EXT_memory_budget` bütçesinin, yığının önbelleğe alınmış depolama imajlarının kendileri dışındaki kullanımından ve bütçenin sekizde birinden sonra bıraktığı kısımdır; bu bütçenin yarısı ya da yığının boyutunun yarısından küçük olanıyla sınırlıdır ve hiçbir zaman 512 MiB'ın altına düşmez. Örneklenmiş doku bütçesi, bildirilen bütçenin, yığının önbelleğe alınmış dokular dışındaki kullanımından, yığın boyutunun dörtte birinden (en az 2 GiB) ve bütçenin sekizde birinden sonra bıraktığı kısımdır ve hiçbir zaman 2 GiB'ın altına düşmez. Yığının diğer kullanımı büyüdükçe önce örneklenmiş bütçe bu tabana, ardından depolama bütçesi düşer; depolama bütçesi ayrıca yaşayan örneklenmiş dokuların büyüdükçe aldığı kısmı da geri verir. Her önbellek bir sonraki eklemesinde kırpılır. Her iki bütçe de her saniye yeniden okunur; `[gpu] sampled texture cache budget` satırları bunları açılışta ve ikisinden biri onda birden fazla kaydığında, en fazla her 10 saniyede bir bildirir. `APS5_TEXTURE_CACHE_MIB=<n>` her iki bütçeyi de `n` MiB'a sabitler ve satırlar o zaman yazdırılmaz.

Otomatik olandan büyük sabit bir bütçe, kartın karenin geri kalanının yanında tuttuğu miktarı aşabilir. Ayırmalar o zaman GPU'nun onları PCIe üzerinden okuduğu ana bilgisayar belleğine gider (`[gpu] ... placed in host memory` satırları). 8 GB RTX 4060 üzerinde PPSA16064'ün, önceki kurala göre otomatik depolama imajı bütçesi 1797 MiB'dı; `APS5_TEXTURE_CACHE_MIB=3072` ile 868 ve 878 MiB'ı ana bilgisayar belleğine yerleştirdi ve ilk seviyesini 4,46 ve 4,83 FPS ile yürüdü; değişken ayarlanmamışken hiçbirini yerleştirmedi ve aynı yürüyüşü 6,48 ve 6,07 FPS ile çalıştırdı. Karttaki bir ölçüm aksini göstermedikçe ayarlamadan bırakın.

### Her flip'te GPU sonuçlarını depolama

Depolama imajlarındaki ve render hedeflerindeki GPU sonuçları, bir şey belleklerini okuyana kadar GPU'da kalır. GPU işini bekledikten sonra böyle bir imajı serbest bırakan ve belleği yeniden kullanan bir oyunun yeni verisi, sonuçlar daha sonra depolandığında ezilebilir ([teknik borç](../dev/debt/libSceAgcDriver.md)). `APS5_STORE_AT_FLIP=1`, bir ekran arabelleği flip edildiğinde bekleyen her sonucu depolar; bedeli bekleyen imaj ve kare başına bir geri yazmadır. Başka herhangi bir değer oyunu ilk flip'inde durdurur.

Bir oyunun o belleği kendi CPU okumaları görülmez, bu yüzden bir GPU etiketini bekleyip sonra sonuçları okuyan bir oyun bayat bellek okur. `APS5_STORE_AT_LABEL=1`, GPU'nun yazdığı her etiketten (`RELEASE_MEM`, `WRITE_DATA`, `DMA_DATA`) önce bekleyen her sonucu depolar; bedeli etiket başına her bekleyen imajın geri yazılmasıdır. Başka herhangi bir değer oyunu ilk etiketinde durdurur.

### Yükseltme ölçekleme (Upscaling)

Pencere, oyunun ekran arabelleğini çift doğrusal filtrelemeyle sığacak şekilde ölçekleyerek gösterir. Bunun yerine AMD FidelityFX Super Resolution 1.0 ile ölçeklemek için `ANYPS5_UPSCALE=fsr` ayarlayın. Sığdırılmış imaj, ekran arabelleğinden herhangi bir eksende büyük olduğunda, örneğin 2560x1440 veya 3840x2160 bir pencerede 1920x1080 ekran arabelleği, EASU onu yükseltir ve RCAS sonucu keskinleştirir. EASU, her eksende ekran arabelleğinin boyutunun iki katına kadar için tasarlanmıştır: 3840x2160 bir pencerede 1280x720 arabellek gibi daha büyük çarpanlar yine çalışır ama daha yumuşak görünür. Aynı boyutta ya da daha küçük olduğunda FSR o kare için kapalı kalır ve kare `ANYPS5_UPSCALE` olmadan olduğu gibi sunulur: orada tek başına RCAS ayrıntı eklemeden kare hızına mal olur.

`ANYPS5_FSR_SHARPNESS`, RCAS keskinliğini stop cinsinden `0.0` (en keskin) ile `2.0` (en zayıf) arasında ayarlar; varsayılan `0.2`'dir. `ANYPS5_UPSCALE=fsr` olmadan etkisi yoktur. RCAS yalnızca FSR'nin yükselttiği karelerde çalışır ve orada her zaman keskinleştirir: `2.0` onun en zayıf ayarıdır, kapalı değildir. Yaklaşık tersi ayrıca düz alanları değerlerinin sabit bir kesri kadar karartır: varsayılan `0.2`'de %0,17 (düz 0,5, 0,4992 olur), `0.0`'da %0,32 ve `2.0`'da %0,07. Bu, SDR için en fazla bir 8 bit düzeyi ve HDR10 için en fazla üç 10 bit kod değeridir.

FSR, SDR ve HDR10 çıkışında, ekranın aldığı değerler üzerinde aynı şekilde çalışır: SDR için sRGB kodlu, HDR10 için PQ kod değerleri. Keskinleştirme onları hiçbir zaman sinyal aralığının dışına çıkarmaz. EASU çalışırken, uçuştaki her sunum iki imaj daha tutar; biri ekran arabelleğinin boyutunda, biri sığdırılmış imajın boyutunda ve her kare iki tam pencere geçişi alır.

Her iki değişkenin boş bir değeri, ayarlanmamış bir değerle aynıdır. `ANYPS5_UPSCALE` için `fsr` dışındaki herhangi bir değerde ve 0,0 ile 2,0 arasında bir sayı olmayan `ANYPS5_FSR_SHARPNESS` değerinde başlatma başarısız olur.

### Kıyaslama (benchmark) raporları

Bir oyunun bir makinede nasıl çalıştığını ölçmek için onu `APS5_TRACE_SUMMARY_EVERY=30 APS5_TRACE_BENCH=1` ile başlatın, sahneyi birkaç dakika oynayın ve çıkın. Çalışma zamanı her 30 saniyede bir `APS5_TRACE_DIR` ya da çalışma dizininde `aps5-bench-<commit>-<pid>.json.gz` dosyasını yeniden yazar ve `aps5-summary.jsonl` dosyasına bir satır ekler. Dosya, her iki sunum arasındaki süreyi, commit'i, GPU'yu, sürücüyü, işletim sistemini, CPU adını, iş parçacığı sayısını ve fiziksel belleği ve `app0/sce_sys/param.json` mevcutsa oyun kimliğini, adını ve içerik sürümünü içerir. Hiçbir oyun verisi içermez. Yalnızca tamamlanmış aralıklar kaydedilir ve en yeni 240'ı tutulur; `intervals_dropped`, dosyadan kaldırılan eski aralıkları sayar. Oyun çıktığında hâlâ açık olan aralık yazılmaz. Alanlar [uzun bir oturum için özetler](../dev/BUILD.md#summaries-for-a-long-session) altında açıklanmıştır.

Bir yakalamayı kaynak deposundan Python 3 ile okuyun:

```sh
python3 tools/benchmark_compare.py summarize captures/run.json.gz
python3 tools/benchmark_compare.py summarize captures/run.json.gz --intervals 1:2 --json
```

`--intervals START:COUNT`, tamamlanmış aralıkları, tutulan dosyadaki sıfır tabanlı konumlarına göre seçer: `1:2`, 1 ve 2 numaralı konumları seçer. Olmadan tüm tutulan aralıklar dahil edilir. Yükleme ya da shader derleme sırasında kaydedilen bir aralık, daha sonraki bir aralık seçmediğiniz sürece dahil kalır. Rapor, daha önceki geçmiş atılmış olsa bile, seçilen aralıkların kaydedilmiş sürelerini ve genel sunum numaralarını gösterir. Bir boşluğu aşan seçilmiş aralık reddedilir.

FPS, kare sayısını seçilen aralıkların geçen sürelerinin toplamına bölerek kullanır. Ortalama kare süresi ve p50, p90, p99 ile en büyük kare süresi, mikrosaniyeye kırpılmış kaydedilmiş kare sürelerini havuzlar. Bu değerler FPS'nin tersinden farklı olabilir. Doygunlaşmış süreler bir uyarı üretir ve etkilenen ham süre metriklerini kullanılamaz kılar.

Tekrarlanan A/B çalıştırmaları için, yakalamaların yanına bir seri JSON'ı yazın. Denemeleri çalıştıkları sırayla listeleyin ve kollar olarak `baseline` ve `candidate` kullanın. Amaçlanan her eşleşen çifte aynı `pair` değerini verin. Bu örnek A, B, B, A'yı, ardından iki başarısız denemeyi kaydeder:

```json
{
  "format": "aps5-bench-series/1",
  "recipe": {
    "scene": "Load the same save and stand still after loading",
    "save": "Local save slot 1",
    "input": "No input during the selected intervals",
    "cache": "Warm caches"
  },
  "runs": [
    {"id": "A1", "arm": "baseline", "pair": "1", "capture": "captures/base-1.json.gz", "intervals": [1, 2]},
    {"id": "B1", "arm": "candidate", "pair": "1", "capture": "captures/change-1.json.gz", "intervals": [1, 2]},
    {"id": "B2", "arm": "candidate", "pair": "2", "capture": "captures/change-2.json.gz", "intervals": [1, 2]},
    {"id": "A2", "arm": "baseline", "pair": "2", "capture": "captures/base-2.json.gz", "intervals": [1, 2]},
    {"id": "B3", "arm": "candidate", "pair": "3", "status": "failed", "reason": "No benchmark was produced"},
    {"id": "A3", "arm": "baseline", "pair": "3", "status": "excluded", "reason": "The display slept during the run"}
  ]
}
```

Yakalama yolları seri dosyasına görelidir. `intervals`, `[START, COUNT]`'tur ve tüm tutulan aralıkları dahil etmek için çıkarılabilir. Dahil edilen bir çalıştırma, özgün yakalama baytlarının o özetle eşleşmesini zorunlu kılmak için `sha256` verebilir. Başarısız ve hariç tutulan denemeler bir neden gerektirir ve yakalamaları yoktur. Rapor verilen her denemeyi göstersin diye onları seride tutun. Aynı yakalamanın yeniden kullanılması, yeniden adlandırılmış veya yeniden sıkıştırılmış bir kopya dahil, yinelenen kanıt olarak reddedilir.

```sh
python3 tools/benchmark_compare.py compare series.json
python3 tools/benchmark_compare.py compare series.json --require-matching-context --json
```

Karşılaştırma çalıştırma sırasını korur, her çifti bildirir ve tam çiftleri ayrı sayar. Değişiklikler aday eksi temeldir; toplam, eşleşmiş yüzde değişikliklerinin ortalamasını ve aralığını bildirir. Başarısız, hariç tutulan ya da eksik kolu olan bir çift, eşleşmiş değişikliğe katkıda bulunmaz.

Kaydedilen bağlam farklılıkları, kirli derlemeler ve eksik kimlik alanları raporda görünür. `--require-matching-context`, kaydedilen bağlam farklılıklarını reddeder. Tarif, operatörün sahne, kayıt, girdi ve önbelleklere ilişkin anlatımını kaydeder. Çalıştırmaların aynı sahneye ulaştığını kontrol edin: eşleşen meta veri ve karmalar, bağımsız çalıştırmaları ya da bir kod değişikliğinin hız farkına neden olduğunu kanıtlamaz. Araç var olan dosyaları okur ve Markdown, ya da `--json` ile JSON yazdırır.

### Çalıştırma raporu

Dönüştürülmüş bir oyunun her çalıştırması, `exit` ile bittiğinde `anyps5-report.json` yazar: oyunun ne olduğu, hangi derlemenin çalıştırdığı, ne kadar ilerlediği, kaç kare sunulduğu, shader ve eksik fonksiyon sayıları. Makinenizde kalır; hiçbir şey yüklenmez. Alanlar [çalıştırma raporunda](run-report.tr.md) açıklanır; `docs/run-report.schema.json` kesin biçimdir.

Rapor varsayılan olarak açıktır. Başlatma komut satırında `--aps5-report=off` ya da ortamda `APS5_REPORT=0` onu kapatır ve `--aps5-report=on` veya `APS5_REPORT=1` onu dönüştürülmüş bir oyun olmayan bir işlem için açar; bayrak değişkene üstündür. Oyun bayrağı kendi argümanlarında görür. Rapor kapalıyken hiçbir şey toplanmaz ve hiçbir şey yazılmaz.

Rapor oyunun çalıştırılabilir dosyasının yanına yazılır. O klasöre yazılamadığında, veri klasöründe `reports/<run id>/anyps5-report.json` konumuna gider (Linux'ta `$XDG_DATA_HOME/AnyPS5` veya `~/.local/share/AnyPS5`, Windows'ta `%LOCALAPPDATA%\AnyPS5`, macOS'ta `~/Library/Application Support/AnyPS5`). Windows'ta yalnızca guest'in `exit` ve `_exit` çağrıları rapor yazar; giriş noktasından dönüş hiçbirini yazmaz. Hiçbir yerde yakalanmayan bir istisnada (`std::terminate`) duran bir çalıştırma, örneğin uygulanmamış bir fonksiyona çağrı, raporu terminate işleyicisi çalışmadan önce ya da oyun kendi işleyicisini kurduysa sonra, `abort` çıkışıyla yazar. Öldürülen ya da çöken bir çalıştırma rapor yazmaz.

Bir çalıştırmanın hangi dönüm noktalarını kaydettiği sisteme bağlıdır:

| Sistem | `loaded`, `started` | `firstDraw`, `firstPresent` | `stage`, kareler, kare hızı |
|---|---|---|---|
| Linux | kaydedilmez | kaydedilir | kaydedilir |
| Windows | kaydedilmez | kaydedilir | kaydedilir |
| macOS | kaydedilir | kaydedilir | kaydedilir |

Raporda eksik olan bir dönüm noktası, ya çalıştırmanın ulaşmadığı ya da bu sistemin kaydetmediği bir noktadır: Linux ve Windows'ta `loaded` ve `started` hiçbir zaman kaydedilmez, bu yüzden yokluğu hiçbir şey söylemez ve `stage` ile `framesPresented` çalıştırmanın ne kadar ilerlediğinin görüldüğünü söyler. Windows ve macOS kodu çalıştırılmamıştır.

Bir çalıştırmayı paylaşmak için `anyps5-report.json` dosyasını oyunun tartışmasına ekleyin. Çalışma zamanı kullanıcı adlarını, ev klasörlerini, ana bilgisayar adlarını, adresleri ve hesap numaralarını rapordan süzer ve hiçbir zaman kayıt verisi yazmaz. Bu bir süzgeçtir, garanti değil; bu yüzden paylaşmadan önce dosyayı okuyun.

### Sistem kütüphanesi çağrılarını izleme

Windows, Linux ve macOS. `--trace-imports` ile dönüştürülmüş bir çalıştırılabilir dosya, `libkernel,libSceSaveData` gibi virgülle ayrılmış bir liste ya da `all` olan `APS5_TRACE_CALLS` içinde adı geçen kütüphanelere yaptığı çağrıları kaydeder. Değişken olmadan hiçbir şey kaydedilmez. Bir import'un kütüphane etiketi yoksa, izleyici fonksiyonunu içeren ana bilgisayar modülünün dosya adını kullanır; guest takma adı yerine `libSceAvPlayer.native` gibi o adı seçin. Çökme ve abort raporları o zaman son 64 çağrıyla biter (çökme raporu olmayan macOS'ta yalnızca abort bunları yazdırır): sıra numarası, iş parçacığı, kütüphane, çalıştırılabilir dosyanın import ettiği haliyle import adı (bir NID) ve çağıranın adresi. Raporlayan iş parçacığının yaptığı bu çağrıların son 32'si, en son 65536 çağrıdan alınarak izler; böylece çağrıları daha meşgul bir iş parçacığı tarafından son 64'ün dışına itilen bir iş parçacığı onları yine gösterir. Sonra açılıştan beri en çok çağrılan 32 import, çağrı sayılarıyla gelir.

```
last traced system library calls (APS5_TRACE_CALLS):
  #1 thread 9384 libkernel.prx k+AXqu2-eBc from 0x0000000000400006 app.exe+0x10006
last traced calls of thread 9384:
  #1 libkernel.prx k+AXqu2-eBc from 0x0000000000400006 app.exe+0x10006
most called traced imports (1 calls in total):
             1 libkernel.prx k+AXqu2-eBc
```

Çökmeyle bitmeyen bir çalıştırma sırasında bir oyunun çağırdığı her import'u listelemek için ayrıca `APS5_TRACE_CALLS_FILE` değerini bir dosya yoluna ayarlayın. Dosya her 10 saniyede bir, şimdiye kadar çağrılan her import için bir satırla yeniden yazılır, en çok çağrılan ilk sırada: çağrı sayısı, kütüphane ve import adı. Öldürülen bir çalıştırmadan, son yeniden yazımına kadar sağ çıkar.

```
245557 libc.prx gQX+4GDQjpM#guest
3866 libSceAgc.prx 57labkp+rSQ
```

Sınırlar [libc](../dev/debt/libc.md) ve [relinker](../dev/debt/relinker.md) teknik borcunda listelenir.

### Performans katmanı (overlay)

F10, sunulan imajın üzerinde bir performans katmanını üç durum arasında döndürür: kapalı, sayfa 1 (aşağıdaki tablo), sayfa 2 (ayrıntılı bir sayfa, ikinci tablo), sonra yine kapalı. `ANYPS5_OVERLAY=1` ilk kareden itibaren sayfa 1'i, `ANYPS5_OVERLAY=2` sayfa 2'yi gösterir; F10 oradan devam eder. `ANYPS5_OVERLAY=0`, ayarlanmamışla aynıdır. `ANYPS5_OVERLAY_POS` onu pencerenin bir köşesine yerleştirir: `tl` (sol üst, varsayılan), `tr`, `bl` veya `br`. Her iki değişken için boş bir değer, ayarlanmamış bir değerle aynıdır ve başka herhangi bir değer oyunu ilk karesinde değişkeni adlandıran bir hatayla durdurur. Tuş, [girdi eşlemesindeki](INPUT_MAPPING.tr.md) `TogglePerformanceOverlay` ile taşınabilir. Metin, 5x7 piksel yazı tipinin tam katı olarak pencere yüksekliğiyle ölçeklenir; HDR10 swapchain'de katman 203 nitte çizilir. `ANYPS5_OVERLAY_SCALE=N` (1 ile 8) bir metin ölçeği N ister; katman yine de pencereye sığacak şekilde küçülür.

| Satır | Anlamı |
|------|---------|
| `PROFILED` | `APS5_PROFILE_GPU=1` ya da `APS5_PROFILE_DRAW=1` ayarlıyken ilk satır. Bu anahtarlar her batch'e zaman damgası sorguları ve her pakete zamanlayıcılar ekler; bu yüzden böyle bir çalıştırmanın sayıları profilsiz biriyle karşılaştırılamaz. |
| `FPS` | Son saniyede saniyede sunulan kare. Son saniye ikiden az sunum içerdiğinde (1 fps'nin altı), onun yerine son karenin hızı, 1000'in ms cinsinden süresine bölümü: her 2 saniyede bir kare `0.50` gösterir. Bir oyunun temizlenmiş olarak sunduğu kareler, örneğin yüklenirken, dahil her sunum sayılır. |
| `FRAME` | Son iki sunulan kare arasındaki süre, ms cinsinden. |
| grafik | Son 120 kare süresi, en yenisi sağda. Tam yükseklik, bunların en uzunu ya da ortalamalarının iki katından hangisi büyükse odur. |
| `MIN/AVG/MAX` | 120 kare süresinin en kısası, ortalaması ve en uzunu, ms cinsinden. |
| `GPU` | Son saniyede kare başına GPU süresi, ya da 1 fps'nin altında son karenin süresi, ms cinsinden: kaydedilmiş her batch'in ilk komutundan sonuncusuna kadar aralığı, `APS5_PROFILE_GPU=1`'in zaman damgası sorgularından. Yalnızca o değişkenle gösterilir. Batch'ler tamamlandıklarında sayılır, bu yüzden satır ani bir değişikliğin en fazla bir saniye, 1 fps'nin altında bir kare gerisinde kalır. Farklı kuyruklarda çakışan batch'lerin her biri tam sayılır, bu yüzden satır `FRAME`'i aşabilir. |
| `DRIVER` | Paket işçilerinin dispatch ve çizim paketlerinde harcadığı, son saniyede kare başına CPU süresi, ya da 1 fps'nin altında son karenin süresi, ms cinsinden (`APS5_PROFILE_DRAW=1`'in `[gpu] worker` satırının `dispatch` ve `draw` süresi). Yalnızca o değişkenle gösterilir. Her işçinin süresi tam sayılır, bu yüzden birkaç işçi aynı anda meşgulken satır `FRAME`'i aşabilir. |
| `DRAWS` | Paket işçilerinin yürüttüğü şekliyle, son saniyede kare başına çizim paketleri ya da 1 fps'nin altında son karenin, ardından kare başına dispatch paketleri (`DISPATCH`); koşullamanın (predication) atladığı paketler sayılmaz. Oyun ikisinden birini gönderdikten sonra gösterilir. |
| `SHADERS` | Son saniyede ya da 1 fps'nin altında son kare sırasında oluşturulan işlem hatları, ardından başlangıçtan beri (`TOTAL`): işlem hattı önbelleğinin görmediği her çizim durumu için bir grafik işlem hattı ve her yeni dispatch shader'ı için bir hesaplama işlem hattı. Oyun takılırken bir artış, işlem hattı derlemesine işaret eder. Bir işlem hattı oluşturulduktan sonra gösterilir. |
| `VRAM` | `VK_EXT_memory_budget`'tan, GPU'nun en büyük aygıt-yerel bellek yığınının kullanımı ve bütçesi, saniyede bir okunur. Sürücüde uzantı yoksa gösterilmez. |
| `CPU` | Linux sysfs thermal ve hwmon sensörlerinden (`/sys/class`) saniyede bir okunan, santigrat derece cinsinden CPU paket sıcaklığı. Hiçbir sensör bildirmiyorsa gösterilmez ve Windows'ta okunmaz. |
| `RAM` | İşlemin özel belleği, saniyede bir okunur: Windows'ta `GetProcessMemoryInfo`'nun `PrivateUsage` değeri, Linux'ta `/proc/self/status`'un `RssAnon` artı `VmSwap` değeri. Oyunun doğrudan belleği her iki sistemde de paylaşımlı bellektir ve dahil değildir. |
| `RES` | Ekran arabelleğinin boyutu, ardından pencerenin swapchain boyutu, örneğin `RES: 1920X1080 / 2560X1440`. Ekran arabelleği olmayan kareler için gösterilmez. |

Süreler 100'ün altında bir ondalıkla ve üstünde tam sayı olarak gösterilir; 9999'un üzerindeki her şey 9999 olarak gösterilir, yani `FRAME` ve `MIN/AVG/MAX` 10 saniye veya daha uzun kareler için 9999 okur. `FPS` aynı biçime sahiptir, 1'in altında iki ondalıkla. Sayılar tam sayıdır; 10000'den itibaren binler (`12K`) ve bir milyondan itibaren milyonlar (`3M`) olarak. Bellek 2^30 baytlık GB cinsindendir. Katman gizliyken sunum onun için hiçbir şey kaydetmez ve sunulan imaj değişmez; gösterildiğinde, küçük bir render geçişi onu her sunulan karenin üzerine çizer. Her çizim ve dispatch paketi ve her işlem hattı oluşturma, katman gizliyken de sayılır; paket işçisine ait ya da işlem hatları için paylaşılan bir sayacın bir artışıyla; bellek ve sayaçlar yalnızca katman gösterilirken okunur.

Sayfa 2 daha geniştir, 46 karakter, ve grafiği yoktur. Gösterilirken kaynaklarını saniyede bir okur, bu yüzden hızları son saniyeyi kapsar; sayfadan bir saniye sonra görünürler. Kaynağı çalıştırmada bulunmayan bir satır dışarıda bırakılır ve bir satırın bir kısmı da.

| Satır | Anlamı |
|------|---------|
| `FPS`, `FRAME` | Sayfa 1'deki gibi. |
| `BUSY %` | Her kuyruğun paket işçisinin (kuyruk 0 için `Q0`, kuyruk 1 için `Q1` ve benzeri) son saniyenin paketlere, askıya almalara, gönderim sonlarına, geri sarmalara ve tamamlanmalara harcadığı payı; izleme özetinin `worker_busy_pct` değerinin saydığı şekilde. İzleme toplamlarından. |
| aşamalar | Son saniyede kare başına en çok CPU süresine sahip 5 aşama, ms cinsinden, sürücünün iş parçacıkları üzerinden toplanır ve izleme özetinin `top_phases_ms_per_frame` alanındaki gibi adlandırılır: zamanlanmış bir kapsam ya da bir kapsam ve aşamalarından biri. Bir kapsamın süresi, içindeki kapsamları ve aşamaları içerir ve aynı anda çalışan iş parçacıklarının her biri sayılır, bu yüzden aşamalar toplamda `FRAME`'den fazla olabilir. İzleme toplamlarından. |
| `Q0 WAIT MS/FRAME` | Kuyruk 0'ın paket işçisinin beklediği, kare başına süre, ms cinsinden: `MEM` guest belleğini ya da işlemenin bitmesini bekleyen paketlerde, `LOCK` GPU kilidi için, `GPU` kaydedicinin senkronizasyonlarında ve seri beklemelerinde ve komut batch'lerinin çit (fence) beklemelerinde. GPU'yu senkronize eden bir bellek beklemesi `MEM`'de ve `GPU`'da sayılır. İzleme toplamlarından. |
| `RENDER PASSES/FRAME` | Kare başına başlatılan render geçişleri; açık geçişi sürdüren bir çizim hiçbirini başlatmaz. İzleme toplamlarından; bir `APS5_TRACE_*` değişkeni ayarlıyken gösterilmez. |
| `PIPELINES` | `COMPILING`: kuyruğa alınmış ya da çalışan shader hazırlıkları ve işlem hattı kütüphanesi ön derlemeleri. `NEW/S`: son saniyede oluşturulan işlem hatları, sayfa 1'deki `SHADERS` gibi. `LEFT`: `COMPILING`'in `NEW/S`'ye bölümü, geçerli hızda kalan saniyeler; yalnızca `NEW/S` sıfırın üzerindeyken gösterilir. Bir tahmindir ve yeni iş geldiğinde yükselir. |
| `HIT %` | `DRAW CACHE`: çizim önbelleğinin son saniyedeki aramalarının, tüm aşamaları eşleşen bir girdi bulanlarının payı. `PIPELINES`: işlem hattı önbelleğinin işlem hattını bulan aramalarının payı. Her biri aramasız bir saniye için dışarıda bırakılır. |
| `FLIPS/S`, `PRESENTS/S` | Oyunun saniyedeki flip'leri ile saniyede sunulan kareler. |
| `DROPPED/S`, `REPEATED/S` | Saniyede: boş swapchain imajı olmadığı için sunulmayan flip'ler (`APS5_PROFILE_DRAW=1`'in `[present]` satırının `dropped` sayısı) ve bir önceki sunumla aynı ekran arabelleğinin sunumları. |
| `COMMIT` | Sistemin commit yükü, commit sınırına karşı, GB cinsinden: Windows'ta `GetPerformanceInfo`'nun `CommitTotal` ve `CommitLimit`'i, Linux'ta `/proc/meminfo`'nun `Committed_AS` ve `CommitLimit`'i; burada sınır yalnızca `vm.overcommit_memory=2` ile uygulanır. Sonra `FAULTS/S`, işlemin saniyedeki sayfa hataları, yumuşaklar dahil: Windows'ta `GetProcessMemoryInfo`'nun `PageFaultCount`'u, Linux ve macOS'ta `getrusage`'in küçük artı büyük hataları. macOS commit yükü bildirmez, bu yüzden orada satır yalnızca `FAULTS/S`'yi gösterir. |
| `HOST IMPORTS` | Kullanımdaki ana bilgisayar bellek import'ları, ardından boyutları import bütçesine karşı, GB cinsinden. Ana bilgisayar belleğini import edemeyen bir aygıtta gösterilmez. |
| `CPU COPIES` | Hiçbir ana bilgisayar import'u ya da yansımasının karşılamadığı aralıklar için, adres tabanlı kaynak derlemelerinin saniyede kopyaladığı guest belleği, 2^20 baytlık MB cinsinden. `CAP`: son saniyedeki bu derlemelerin en büyüğü, kopya sınırına karşı, `APS5_ADDRESS_COPY_MAX_MIB` (ayarlanmadıkça 64 MiB); bunu aşan bir derleme oyunu durdurur. |
| `UNIMPLEMENTED` | AnyPS5'in uygulamadığı fonksiyonlara saniyedeki çağrılar, ardından başlangıçtan beri toplamları. Böyle bir çağrı bir hata fırlatır, bu yüzden 0'ın üzerinde bir sayı oyunun onu yakalayıp devam ettiği anlamına gelir. |
| `REFUSED SHADERS` | Çizimleri ya da dispatch'leri atlanan shader'lar. Yalnızca `APS5_SKIP_REFUSED_DRAWS=1` veya `APS5_SKIP_REFUSED_DISPATCHES=1` ile gösterilir; onlar olmadan reddedilen bir shader oyunu durdurur. |
| `TITLE`, `PRESENT` | Oyunun `param.json` dosyasındaki oyun kimliği ve `ANYPS5_PRESENT_MODE`'un adlandırdığı şekilde swapchain'in sunum modu. |
| `FPS LIMIT` | `ANYPS5_FPS_LIMIT` değeri, `TITLE` ve `PRESENT`'ten sonra kendi satırında ve `FPS` gibi biçimlendirilmiş (`59.94`, `59.9` okunur), 10000'den itibaren binler olarak. Sınır olmadan gösterilmez. |

Meşgul payları, aşamalar, kuyruk 0 beklemeleri ve render geçişleri, izlemenin iş parçacığı başına tuttuğu toplamlardan gelir. Sayfa 2'yi göstermek bu toplamları, bir `APS5_TRACE_FRAMES` izlemesinin olay arabellekleri olmadan açar: sayfa 2 gösterilirken sürücünün iş parçacıklarındaki her zamanlanmış kapsam saati iki kez okur ve süresini iş parçacığının toplamlarına ekler ve başlatılan her render geçişi orada sayılır. Sayfa 2'den çıkmak toplamları yeniden kapatır. Bir `APS5_TRACE_*` değişkeniyle bir çalıştırma izlemesini yapılandırıldığı gibi korur; sayfa 2 o zaman bu satırları yalnızca o izleme kaydederken gösterir. Katman gizliyken ya da sayfa 1'deyken, sayfa 2 başlatılan her render geçişi için bir denetim (her zamanlanmış kapsamın zaten yaptığı aynı denetim), her adres tabanlı derlemenin kopyaladığı baytların iş parçacığı başına bir sayımını, uygulanmamış fonksiyon çağrılarının bir sayımını ve sunum başına ekran arabelleğinin adresini ekler; kaynakları yalnızca gösterilirken okunur.

### Günlüğü bir dosyaya kaydetme

Dönüştürülmüş bir çalıştırılabilir dosyanın ortamında `ANYPS5_LOG_FILE=<path>` ayarlayarak günlüğünü bir hata raporuna ekleyebileceğiniz bir dosyada tutun. Çalıştırılabilir dosya yine her şeyi terminale yazdırır; dosya, standart çıktıya ve standart hataya yazdıklarının bir kopyasını alır: çalışma zamanının günlük satırları, `[tag]` mesajları, oyunun orada kendi yazdırdıkları ve çökme ile abort raporları. `APS5_LOG_TIMESTAMPS`, dosyadaki satırları terminaldeki gibi damgalar. Relinker, `APS5_TRACE_*` değişkenlerinin izleme dosyaları ve bu iki akışa yazmayan programların çıktısı buna dahil değildir.

```
ANYPS5_LOG_FILE=run.log ./app.elf
```

- Dosya, çalıştırılabilir dosya yüklendiğinde açılır ve yoksa oluşturulur. Göreli bir yol çalışma dizinine göre çözülür.
- Dosyaya ekleme yapılır, hiçbir zaman kısaltılmaz; bu yüzden başka bir dönüştürülmüş çalıştırılabilir dosya başlatan bir oyun aynı dosyaya yazmaya devam eder ve ikinci bir çalıştırma birincinin altından devam eder. Her başlatma yalnızca dosyaya `[log] ANYPS5_LOG_FILE: appending to this file, process <id>` satırını yazar. Yeni bir günlük başlatmak için önce dosyayı silin. Yolda `{pid}` ya da tarih genişletmesi yoktur.
- Boş bir değer, ayarlanmamış bir değişkenle aynıdır. Açılamayan bir yol (eksik bir dizin, bir dizin, izin yok), çalıştırılabilir dosyayı çalışmadan önce, durum 1 ve `ANYPS5_LOG_FILE`'ı, yolu ve işletim sistemi hatasını adlandıran bir mesajla durdurur.
- Tek bir `write` çağrısının ürettiği bir satır dosyaya bütün olarak yazılır: bir standart çıktı satırı hiçbir zaman bir standart hata satırının içine düşmez. Aynı akışa borunun atomik boyutundan (macOS'ta 512 bayt, Linux'ta 4096) uzun satırlar yazan iki iş parçacığı, bir terminalde olduğu gibi yine iç içe geçebilir. İki akış iki iş parçacığıyla kopyalanır, bu yüzden bir akışın satırları, neredeyse aynı anda yazılmış diğerinin satırlarından birkaç satır önce ya da sonra çıkabilir. Satır sonu olmayan son bir satır, işlem bittiğinde dosyada bir tane alır.
- Bir çökme ya da abort raporu ve ondan önce yazılan satırlar, işlem bitmeden önce dosyaya yazılır. Dışarıdan sonlandırılan bir işlem, örneğin `SIGKILL` veya `TerminateProcess` ile, son milisaniyelerde yazdıklarını kaybeder.
- Dosyanın boyut sınırı yoktur ve oyun günlüğe yazdığı sürece büyür.
- Dosya artık yazılamıyorsa, örneğin disk dolduğu için, çalıştırılabilir dosya bunu söyleyen bir satır yazdırır ve yalnızca terminal çıktısıyla devam eder.
- Değişken ayarlıyken çalıştırılabilir dosya iki ek iş parçacığı çalıştırır ve standart akışları iki boruda tutar. Oyunun başlattığı bir alt işlem çıktısını aynı borularda tutar, bu yüzden oyun çıktıktan sonra yazdığı çıktı kaybolur ve Linux ile macOS'ta o yazmanın `SIGPIPE`'ı, sinyali yok saymadıkça onu sonlandırır. Oyun `sceSystemServiceLoadExec` ile kendini başka bir çalıştırılabilir dosyayla değiştirdiğinde, gerçek akışlar önce devredilir.
- Konsol olmadığında, örneğin konsolsuz başlatılan bir Windows çalıştırılabilir dosyasında, günlüğü yalnızca dosya alır.
- Windows'ta dosya, bir konsolla, çıktı bir dosyaya yönlendirilmiş ya da bir boru üzerinden okunmuş halde ve konsolsuz denetlendi; çökme raporu yolu ve yedek çalıştırılabilir dosyaya devir testlerle çalıştırılır.

### Hata raporu paketi

`python3 tools/bug_report.py`, bir hata raporunun ihtiyaç duyduğunu tek bir zip dosyasında toplar: `anyps5-report.json` çalıştırma raporu, günlük ve girdi eşlemesi; kişisel bilgileriniz karartılarak. Ayrıca işletim sistemi, makine ve Python sürümünü içeren `host.txt` ve nelerin karartıldığını söyleyen bir `README.txt` yazar.

- Varsayılan olarak `./anyps5-report.json` dosyasını, `ANYPS5_LOG_FILE`'ın adlandırdığı günlüğü ve `ANYPS5_INPUT_CONFIG`'in adlandırdığı girdi eşlemesini (veya `./anyps5-input.ini`) okur ve `bug-report.zip` yazar. Başka dosyalar seçmek için `--report`, `--log`, `--input-config` ve `--out` kullanın.
- Rapor bir oyunu adlandırıyorsa, araç paylaşılan eşlemenin yanında, paylaşılan dosya olmasa bile `anyps5-input.<TITLEID>.ini` dosyasını arar. Paket, o oyunun geçersiz kılmalarını `anyps5-input-profile.ini` olarak içerir.
- Karartma, ev dizininizi `~` ile, kullanıcı ve ana bilgisayar adlarınızı `<user>` ve `<host>` ile ve IPv4 adreslerini `<ip>` ile değiştirir. Yalnızca tam sözcükleri eşleştirir, bu yüzden başka bir sözcüğün içindeki bir kullanıcı adı değiştirilmez. Bir oyuna yazılan ad, adınızı içeren bir dosya adı ya da bir IPv6 adresi gibi diğer kişisel bilgileri bilmez; eklemeden önce paketi açın ve kontrol edin.
- Eksik bir günlük ya da girdi eşlemesi paketin dışında bırakılır. Çalıştırma raporu geçerli bir JSON olmalıdır. Var olan bir paketin üzerine hiçbir zaman yazılmaz: komut 1 durumuyla çıkar ve dosyayı adlandırır.

### Kumanda durum paneli

Sunulan imajın üzerindeki küçük bir panel, oyun başladığından beri bir kumandası olmuş her yuva için bir satır olacak şekilde dört oyuncu yuvasının kumandalarını listeler (`ANYPS5_PAD_STATUS` ayarlanmamış ya da 0 iken, ilk F9 basışından beri, çünkü kumandalar ondan önce izlenmez): `P1 DUALSENSE  WIRED`, `P2 XBOX  BATTERY MEDIUM`, `P3 DISCONNECTED`. Hiçbir zaman kalıcı olarak gösterilmez: onu gösteren son şeyden 4 saniye sonra kaybolur. Henüz hiçbir kumanda görülmediğinde `NO CONTROLLER` okur.

Paneli oyun penceresinde göstermek için F9'a basın (`ShowPadStatus`'ün varsayılan bağlaması). Eylemi yeniden tetiklemek dört saniyelik zamanlayıcıyı yeniden başlatır. Onu [girdi eşlemesinde](INPUT_MAPPING.tr.md) başka bir tuşa ya da bir `PAD:` akoruna bağlayabilirsiniz. `ANYPS5_PAD_STATUS=1` ile ayrıca bir kumanda bağlandığında (oyun başladığında zaten bağlı olan bir kumanda dahil), bağlantısı kesildiğinde, yuvasında başka biriyle değiştirildiğinde ya da pili daha düşük bir sınıfa girdiğinde (dolu, orta, düşük, boş) kendiliğinden görünür; her yeni olay 4 saniyeyi yeniden başlatır. `ANYPS5_PAD_STATUS=0` ya da ayarlanmamış değişkenle panel yalnızca `ShowPadStatus`'u tetiklediğinizde görünür. Bu değişken için boş bir değer, ayarlanmamış bir değerle aynıdır ve başka herhangi bir değer, video çıkışı başladığında oyunu değişkeni adlandıran bir hatayla durdurur.

Bir satır, oyuncu numarası (kumandanın yuvası: yeniden bağlanan bir kumanda en düşük boş yuvayı alır, bu başka bir oyuncu olabilir), kumanda türü ve gücüdür. Tür `DUALSENSE`, `DUALSHOCK 4`, `DUALSHOCK 3`, `XBOX` ya da `SWITCH`'tir; başka herhangi bir kumanda için, katman yazı tipinin karakterlerine indirgenmiş ve son bir nokta ile 12 karaktere kesilmiş SDL'nin bildirdiği addır; SDL ad vermiyorsa `PAD`. Güç, `WIRED` ya da SDL'nin pil sınıflarından biridir: `BATTERY EMPTY` (%5'e kadar), `BATTERY LOW` (%20'ye kadar), `BATTERY MEDIUM` (%70'e kadar) ve `BATTERY FULL`; sürücüsü ya da bağlantısı hiçbir düzey bildirmeyen bir kumanda `BATTERY UNKNOWN` okur. Neyin bildirildiği SDL'ye, işletim sistemine ve kumandaya bağlıdır.

Panel, [performans katmanının](#performans-katmanı-overlay) yazı tipini ve ölçeğini kullanır ve `ANYPS5_OVERLAY_POS`'tan yatay olarak yansıtılmış köşeye, varsayılan olarak sağ üste oturur; böylece o katmanı hiçbir zaman örtmez. Sistem ekran klavyesi gösterilirken panel çizilmez ve 4 saniyesi akmaya devam eder. Ekran görüntüleri onu içermez. Gizliyken, ki F9 ya da `ANYPS5_PAD_STATUS=1` göstermedikçe her zaman böyledir, hiçbir şey çizilmez, onun için hiçbir render geçişi ya da bellek ayrılmaz ve sunulan imaj değişmez. Hâlâ çalışan şey, panelin istenip istenmediğini görmek için her sunumda ve her video çıkışı döngü geçişinde bir kilit ve bir saat okumasıdır. `ANYPS5_PAD_STATUS` ayarlanmamış ya da 0 iken ve F9 basışı olmadan, hiçbir kumanda adı kopyalanmaz ve onun için hiçbir pil düzeyi okunmaz; `ANYPS5_PAD_STATUS=1` ile her döngü geçişinde ve F9'dan sonra panel gösterilirken okunurlar.

### Yalnızca PSN gerektiren diyaloglar

Bazı sistem diyalogları PlayStation Network gerektirir: profil, arkadaş listesi, mağaza, davet diyalogu, Facebook bağlantısı ve yetkili uygulamalar diyalogu. AnyPS5 PlayStation Network sağlamaz, bu yüzden bu diyaloglar bir iptalle aynı yanıtla hemen biter ve hiçbir şey gösterilmez. Web tarayıcı diyalogunun PlayStation Network gerektirdiği bilinmez, bu yüzden bildirimi yalnızca sistem özelliğinin kullanılamadığını söyler.

Bu diyaloglardan biri açıldığında oyunun ekranında bir bildirim göstermek için `ANYPS5_PSN_DIALOGS=show` ayarlayın. Bildirimin bir Tamam düğmesi vardır ve yanıtlanana kadar kumandayı alır. Bir öncekinden sonraki iki saniye içinde yeniden gösterilmez ve başka bir diyalog ya da bildirim gösterilirken gösterilmez. Oyunun sonucu her iki durumda da aynıdır. `silent`, ayarlanmamış bir değişken ve boş bir değer hiçbir şey göstermez. Başka herhangi bir değer, oyunu böyle ilk diyalogunda `ANYPS5_PSN_DIALOGS`'u adlandıran bir hatayla durdurur.

### Linux çökme raporu

Linux x86-64'te, başka hiçbir şeyin işlemediği bir `SIGSEGV`, `SIGBUS`, `SIGILL` veya `SIGFPE`, işlem o sinyalle bitmeden önce stderr'e ve `ANYPS5_LOG_FILE` ayarlıysa günlük dosyasına bir rapor yazdırır:

- sinyal, `si_code`'u, hata adresi ve iş parçacığı kimliği;
- `rip`, bir adres olarak ve eşlenen dosya artı o dosyadaki ofseti olarak, örneğin `app.elf+0xc77cda` veya `libkernel.prx+0xc7e30`;
- çekirdeğin genel koruma bağlamı ve okunabilir komut baytları o kesmeyi tanımladığında, `int 0x45` için `guest abort()`, ya da `int 0x41` için `guest assert/break`;
- on altı genel yazmaç;
- `rbp` çerçeve zincirinin dönüş adresleri, en fazla 32;
- `rsp`'nin 32 KiB üzerinde, çalıştırılabilir eşlemelere işaret eden en fazla 24 yığın sözcüğü.

`--trace-imports` ve `APS5_TRACE_CALLS` ile, abort raporundaki gibi son izlenen çağrılar izler. Bir guest modülündeki ya da çalıştırılabilir dosyadaki bir ofset, dönüştürülmüş dosyadan o dosya ofsetinde disassemble edilebilir.

### Windows çökme tanılamaları

Bir yazmaç, en az dört yazdırılabilir ASCII karakterle başlayan, UTF-16LE ya da tek baytlık okunabilir belleğe işaret ettiğinde, çökme raporu bu metni yazmaç bellek dökümünden sonra, UTF-16LE için `[r13] u"..."` ya da tek baytlar için `[rdi] "..."` olarak yazdırır. Metin bir NUL'da, yazdırılabilir ASCII, sekme, satır besleme ya da satır başı olmayan ilk karakterde, 512 karakterden sonra ya da okunabilir belleğin sonunda biter. Sekmeler, satır sonları, tırnaklar ve ters eğik çizgiler kaçışlanır ve kapanış tırnağından sonraki `...`, bu sınırlar içinde hiçbir NUL'un metni bitirmediği anlamına gelir.

Yazmaç bellek dökümünde bulunan her işaretçiden en fazla 64 bayt eklemek için çalıştırılabilir dosyayı başlatmadan önce `APS5_CRASH_INDIRECT_MEMORY=1` ayarlayın. Rapor her bloğu yazmacı, işaretçi ofseti ve hedef adresiyle etiketler. Erişilemeyen, eksik ve çalıştırılabilir hedefler atlanır. Bu ek döküm, değişken ayarlanmamış ya da boş olduğunda devre dışıdır.

Yığın dönüş adresi adaylarından sonra, hata veren iş parçacığının `rsp`'den başlayan yığın sözcüklerini `n` bayt boyunca (en fazla 1048576, ondalık ya da `0x` onaltılık) satır başına dört sözcük ve `rsp`'den ofsetleriyle yazdırmak için `APS5_CRASH_STACK_BYTES=<n>` ayarlayın. Altlarında rapor, iş parçacığının yığını dışında okunabilir, çalıştırılamaz belleğe işaret eden her ayrı yığın sözcüğünde sekiz sözcük yazdırır; sözcüğün ofseti ve tuttuğu adresle etiketlenmiş, en fazla 256 adres. Bu, çağıran çerçevelerin tuttuğu nesneleri, örneğin bir yıkıcının `this`'ini ya da bir kapsayıcının arabelleğini gösterir. Döküm okunamayan ilk yığın sayfasında durur ve değişken ayarlanmamış, boş ya da 0 olduğunda hiçbir şey yazdırılmaz.

Rapor işlemin diğer iş parçacıklarını, en fazla 128 tanesini (bir satır kaçının dışarıda bırakıldığını söyler), `rip`, `rsp`, `rbp` ve `rbp` çerçeve zincirinin dönüş adresleriyle listeler. Zinciri ikiden az çerçeve veren bir iş parçacığı, örneğin `ntdll.dll` içinde ya da çerçeve işaretçisiz derlenmiş kodda bekleyen biri, ayrıca en fazla 24 yığın dönüş adresi adayı alır: iş parçacığı askıdayken okunan, `rsp`'sinin 32 KiB üzerinde, çalıştırılabilir belleğe işaret eden sözcükler. Hata veren iş parçacığının adayları gibi bunlar da bir tarama olup bir unwind değildir, bu yüzden aralarında tamamlanmış çağrıların bayat dönüş adresleri görünebilir.

### GPU gönderim halkaları ve duraklama dökümleri

Her GPU kuyruğu son 16 komut arabelleği gönderimini tutar: her birinin ne zaman gönderildiği, seri numarası, boyutu ve guest adresi ve dolaylı arabellekler satır içine alındıktan sonra ilk 512 dword'ünün bir kopyası. `APS5_PM4_RING=<n>`, bunun yerine son `n` tanesini tutar, 0 ile 4096 arasında; 0 hiçbirini tutmaz ve başka herhangi bir değer oyunu ilk gönderiminde durdurur. Halkalar, Windows çökme raporu çalıştığında stderr'e, gönderim başına bir satır ve çözülen paket başına bir satır olarak yazdırılır: işlenmeyen bir istisna, `abort()`, geçersiz bir CRT parametresi ya da `APS5_HANG_DUMP_SECS`'in zamanlanmış dökümü.

`APS5_STALL_WATCHDOG_SECS=<n>` ile, bir `WAIT_REG_MEM`, bir `REWIND` ya da kuyruk 0'ın arkasında `n` saniye ilerlemeden bekleyen bir kuyruk, o bekleme için halkaları bir kez yazdırır ve oyun çalışmaya devam eder. Bir `WAIT_REG_MEM`, `APS5_GPU_WAIT_TIMEOUT_MS`'te olduğu gibi başka herhangi bir paket çalışırken ya da tamamlanırken ilerler. `n` 0 ile 86400 arasındadır ve varsayılan 0 bekçiyi devre dışı bırakır; başka herhangi bir değer, `APS5_PM4_RING`'de olduğu gibi oyunu ilk gönderiminde durdurur. Kuyruk 0'ın arkasında tutulan bir kuyruk için halkalar yazdırılırken, yeni gönderimler ve diğer kuyruklar bekler.

`ANYPS5_STALL_PROMPT=1` ile de, izlenen bir bekleme ilk kez süresi dolduğunda ana bilgisayar bir mesaj kutusu gösterir: `Wait` oyunu çalışır tutar ve `Quit` halkalar yazdırıldıktan sonra onu 124 çıkış koduyla bitirir. `Wait`'ten sonra kutu bir süre sonra, aynı bekleme için en fazla üç kez geri gelir; halkalar her durumda bekleme başına bir kez yazdırılır. Aynı anda yalnızca bir kutu açıktır ve onu açık bulan bir bekleme, o kutu yanıtlandıktan sonra sorulur. Hiçbir mesaj kutusu gösterilemiyorsa halkalar yazdırılır ve oyun çalışmaya devam eder. Ayarlanmamış ya da varsayılan `0` kutu göstermez; `0` veya `1` dışındaki herhangi bir değer oyunu ilk gönderiminde durdurur. İstem olmadan bekçi hiçbir zaman bir oyunu bitirmez: her bekleme için halkaları bir kez yazdırır ve oyun çalışmaya devam eder.

## Çıkış kodları

`0`: dönüştürme başarılı oldu ya da yardım veya sürüm görüntülendi. `1`: geçersiz argümanlar ya da uyumsuz olan bir `--verify-patches` girdisi. `2`: dönüştürme başarısız oldu; hata stderr'e yazdırılır. `--autorun` ile başarılı dönüştürme, başlatılan uygulamanın çıkış kodunu döndürür.

## Import denetimi

[`tools/import_audit.py`](../../tools/import_audit.py), dönüştürülmüş bir oyunun import ettiği sistem fonksiyonlarını ve derlenmiş kütüphanelerin bunları sağlayıp sağlamadığını, oyun başlatılmadan önce listeler. Python 3'e ihtiyaç duyar, başka hiçbir şeye değil.

```sh
relinker --registry source/input.elf app.elf
python3 tools/import_audit.py app.registry.json app.libc.prx.guest.prx.registry.json --libs build/core/libs/libs --modules source/sce_module
```

`--registry`, çalıştırılabilir dosya için `app.registry.json` ve dönüştürülen her paket modülü için bir `app.<module>.registry.json` yazar; hepsini verin. Bir modülün import'ları, kaydı çalıştırılabilir dosyanın kaydıyla birlikte verildiğinde o kayıttan, aksi halde `--modules` içindeki dosyasından sayılır; böylece her iki şekilde verilen bir modül iki kez sayılmaz; dosya yine modülün export'larını ve ihtiyaç duyduğu kütüphaneleri sağlar ve bir kayıttan alınan import'lar raporda modülün dosya adıyla işaretlenmez. `--libs`, `libs` hedefinin doldurduğu dizindir ([derleme talimatları](../dev/BUILD.md)); export'lar derlenmiş `.prx` dosyalarından okunur, bu yüzden sonuç yükleyicinin bulduğuyla eşleşir. Import'lar NID ve kütüphane başına bir kez sayılır ve her başvuru tam olarak bir sınıfa düşer:

| Sınıf | Anlamı |
|-------|---------|
| `implemented` | Derlenmiş bir kütüphane NID'yi export eder ve fonksiyonu fırlatan bir stub değildir. |
| `partial` | Export edilmiş, ancak fonksiyon, davranışı bilinmeyen girdiler için fırlatır. |
| `stub` | Export edilmiş, ancak fonksiyonun tamamı `NotImplemented_nid_no_patch`'i çağırır: oyun yüklenir ve onu çağırdığında fırlatır. |
| `absent` | Yükleyicinin aradığı hiçbir derlenmiş kütüphane NID'yi export etmez: yükleyici başarısız olur. |
| `module` | Import, `--modules` içinde bulunan, sembolü ya da NID'sini export eden bir oyun kütüphanesini adlandırır. |

Bir kütüphaneyi adlandıran bir import, birebir export adını gerektirir. Uygulaması yalnızca import adının karmalanmasıyla bulunuyorsa, yine `implemented` veya `stub` sayılır, ancak rapor ad uyuşmazlığını listeler, JSON import kaydında `symbol_mismatch` ayarlar ve özette sayar ve 1 koduyla çıkar. Ad uyuşmazlığı olan güçlü bir import mevcut bir duruştur. Kütüphanesiz import'lar, denetimin addan NID'ye geri dönüşünü korur.

Modül kayıtları ELF zayıf (weak) import'larını `"weak": true` ile işaretler; bu, denetimin mevcut-duruş seçiminin onları atlamasına izin verir. Güçlü import'lar alanı çıkarır. Eski modül kayıtları zayıf bağlamaları korumaz; isteğe bağlı import'ları ayırt etmek için bunları yeniden üretin.

`--modules` içindeki ELF modülleri dosya adından bağımsız olarak denetlenir: tanımsız sembolleri derlenmiş kütüphanelere ve diğer modüllerin export'larına karşı sınıflandırılır, modül dosyası raporda adlandırılır ve ihtiyaç duydukları kütüphanelerin `--libs` ya da `--modules` içinde bir dosyası olmalıdır. Kaydı çalıştırılabilir dosyanın kaydıyla birlikte verilen bir modülün import'ları onun yerine o kayıttan sayılır ve dosya adıyla işaretlenmez; ihtiyaç duyduğu kütüphaneler yine dosyasından okunur. Sıradan modül olmayan yan dosyalar yok sayılır; hatalı biçimlendirilmiş ELF, SELF kapsayıcıları ve modül uzantılı ELF olmayan dosyalar hata olarak kalır; bir Windows derlemesinin dönüştürülmüş `*.guest.prx` PE modülleri kabul edilir ve başka herhangi bir PE dosyası hatadır. Denetim, dinamik tabloları ELF bölüm başlıklarından ya da bölümsüz dosyalar için `PT_DYNAMIC` ve bir SysV ya da GNU karma tablosundan okur. Her iki görünüm de mevcut olduğunda, import, export ve bağımlılık envanterleri uyuşmalıdır; çelişkili meta veri bir hatadır. Bir PS4 modülünün `DT_STRTAB` ya da `DT_SYMTAB`'ı yoktur: SCE etiketleri `0x61000035` ve `0x61000039`, dize ve sembol tablolarının SCE dinamik kütüphane veri segmentindeki (`0x61000000`) ofsetlerini verir, `0x61000037` ve `0x6100003b` dize tablosu boyutunu ve sembol girdisi boyutunu verir ve `0x6100003f` sembol tablosu boyutunu verir. Hem standart hem SCE tablolarını adlandıran bir modül hatadır; PS5 modüllerinin standart tablolarının yanında taşıdığı `0x6100003f`, onların nasıl okunduğunu değiştirmez.

Her kaydın, aynı önek ve `.needed.json` sonekiyle, ana bilgisayar kütüphane bağımlılıklarını, import'sız bildirilmiş `DT_NEEDED` girdileri dahil, listeleyen bir eşlikçisi vardır. Dönüştürülmüş paket modülleri tarafından sağlanan bağımlılıklar, takma adları dahil, çıkarılır. Çalıştırılabilir dosyanın eşlikçisi, paket modüllerinin ana bilgisayar bağımlılıklarını içerir. Denetim bu eşlikçileri otomatik olarak okur; onları kayıtların yanında tutun. Eşlikçisiz eski kayıtlar yine çalışır, ancak import'ı olmayan bağımlılıkları bildiremez; bu bağımlılıkları dahil etmek için `--registry` ile yeniden üretin.

Rapor ayrıca `--libs` ya da `--modules` içinde dosyası olmayan gerekli kütüphaneleri, sıfır import'lu bağımlılıklar dahil, listeler. Bunlar import ya da başvuru sayılarını artırmaz. Hiçbir import'a ihtiyaç duyulmadığında Windows eksik bir bağımlılığı atlayabilir; Linux yine de dosyaya ihtiyaç duyar. Denetim ayrıca adlandırdıkları kütüphaneden başka bir kütüphane tarafından export edilen import'ları da listeler; bunlar Linux'ta çözülür ve Windows'ta başarısız olabilir.

Eşlikçili bir Windows derlemesinde, kütüphanesiz bir import yalnızca giriş stub'ının aradığını sayar: oyunun modülleri, eşlikçilerin listelediği kütüphaneler, biri bir çekirdek kütüphanesiyse çekirdek ailesi ve biri bir çekirdek kütüphanesi ya da `libSceLibcInternal.prx` ise `libc.prx`. Yalnızca diğer derlenmiş kütüphanelerin export ettiği bir import `absent`'tir; JSON import kaydı bunları `unsearched_providers` içinde listeler ve özet ile rapor bunları kütüphane başına sayar. Verilen tüm kayıtların eşlikçileri havuzlanır, bu yüzden bir çalıştırılabilir dosyanın ve modüllerinin kayıtlarını, oyunun dönüştürülmüş modüllerini barındıran her klasör `--modules` içinde olacak şekilde birlikte verin: yalnızca bir oyun modülünün sağladığı bir import, o modülün klasörü eksikse `absent` sayılır. Hiçbir eşlikçi bir kütüphaneyi listelemediğinde ve bir Linux derlemesinde, her derlenmiş kütüphane yine sayılır.

Denetim ayrıca derlenmiş kütüphanelerin kendi ana bilgisayar bağımlılıklarında da gezinir: `--libs` altındaki her `.prx`'in PE import'ları (ya da ELF `DT_NEEDED` girdileri), `--libs` veya `--modules` içinde dosyası olmayan `.prx` adları için denetlenir ve mevcut kütüphaneleri geçişli olarak izler. Boşluklar denetimi başarısız kılar ve `Importer: <root> Chain: <root> -> ... -> <missing>` satırları olarak bildirilir, çünkü her oyun import'u çözülse bile yükleyici onlarda başarısız olur.

| Seçenek | Etki |
|--------|--------|
| `--libs <dir>` | Derlenmiş `.prx` dizini. Zorunlu; birkaç tane için tekrarlayın. |
| `--modules <dir>` | Oyunun kendi modüllerinin dizini; birkaç tane için tekrarlayın. |
| `--source <dir>` | Fırlatan stub'lar için aranan `core/libs/prx` ağacı. Varsayılan olarak bu deponunkidir. |
| `--strict-stubs` | Bir import `stub` sınıfındayken de, zayıf import'lar ve `--modules` dosyalarının import'ları dahil, `1` koduyla çıkar. `partial` import'lar sayılmaz. |
| `--strict-providers` | Bir kütüphaneyi adlandıran bir import yalnızca başka bir kütüphane tarafından export edildiğinde de `1` koduyla çıkar. `module` import'ları ve `libc.prx`'in export ettiği import'lar sayılmaz. |
| `--names <file>` | Satır başına NID ve ad, [`tools/nid_names.py`](../../tools/nid_names.py)'den `aerolib.csv` gibi; `absent` import'ları adlandırır. Hiçbir zaman indirilmez; NID'sine karmalanmayan bir ad işaretlenir. |
| `--json <file>` | Tam sonuç; deponun commit'i ve ağacının değiştirilip değiştirilmediği ile damgalanmış. |

Kayıt `nid` alanları, kodlanmış NID'leri ya da `strcmp` gibi sıradan ELF sembol adlarını içerebilir. Import'lar önce birebir export'larla, birebir arama başarısız olursa hesaplanan NID'leriyle eşleştirilir. Raporlar özgün import adını korur. Export'un kullanılabilirliği, her platform yükleyicisinin o arama yolunu desteklediğini kanıtlamaz.

Çıkış kodları: `0` hiçbir şey yüklemeyi engellemez, `1` `absent` import'lar, import ad uyuşmazlıkları, dosyası olmayan gerekli kütüphaneler ya da kütüphane ana bilgisayar bağımlılıkları var ya da `--strict-stubs` veya `--strict-providers` ile `stub` import'lar ya da yalnızca başka bir kütüphane tarafından export edilen import'lar var, `2` bir girdi okunamadı; mesaj dosyayı ve değeri adlandırır.

Bir modül kaydı, başka hiçbir paket modülünün export etmediği bir fonksiyona karşı her yer değiştirme için, çalıştırılabilir dosyanın NID'lerinin `#` soneki olmadan `nid`, `library` ve `targetOffset` listeler.

## Yerel uygulamaların import'larını yamalama

`nid_patcher --exec <file> [file2 ...]`, import'ları yerinde yamalar ve uygulamanın export'larını korur. PE, ELF64 ve Mach-O64'ü dosya içeriğinden algılar. Kütüphane adı gerekmez. Her hedef platform için ayrı bir çalıştırılabilir dosya derleyin.

Varsayılan olarak import'lar kütüphane export adlandırma kurallarını kullanır: `_nid_no_patch` ve `SDL_*` değişmeden kalır; `_nid_no_patch_cut` karmalamadan kaldırılır; `_nid_postfix` ve sayısal `_nid_disambig` ekleri NID hesaplanmadan önce kaldırılır. Hem `foo` hem `foo_nid_postfix` import edildiğinde `foo` değişmeden kalır. Bu kurallar, açıkça korunmadıkça ana bilgisayar import'larına da uygulanır.

`nid_patcher --exec --only-sce <file>`, yeniden adlandırmayı `sce` ile başlayan (büyük/küçük harfe duyarsız) ya da `_nid_postfix` ile biten adlarla sınırlar. Her iki mod da açık hariç tutmalar için `--preserve-exports <unpatched-library>` ya da `--preserve-export-list <export-list>` kabul eder. Mevcut `nid_patcher <library_name> <file>` modu değişmemiştir.

Desteklenmeyen düzenler, girdi dosyası yazılmadan önce başarısız olur.
