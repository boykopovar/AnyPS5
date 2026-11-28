# Çalıştırma raporu

`anyps5-report.json`, tek bir çalıştırmanın yapılandırılmış özetidir. [`run-report.schema.json`](../run-report.schema.json) biçiminin tek doğruluk kaynağıdır; [`run-report-fixtures/`](../run-report-fixtures/) içindeki örnek raporların hepsi ona göre doğrulanır. Bu sürüm 1'dir. Bir alan eklemek sürüm 1'i korur; bir alanı yeniden adlandırmak ya da kaldırmak veya anlamını değiştirmek sürüm 2'dir.

Bir raporun okunabilir özetini şununla yazdırın:

```sh
python3 tools/run_report_summary.py path/to/anyps5-report.json
```

Özet; çalıştırma sonucunu, gözlemlenen en ileri aşamayı, işleme sayılarını, sistem ayrıntılarını ve mevcut eksik fonksiyon ile shader tanılamalarını içerir. Bir bölümün toplanmadığını ya da bir listenin kısaltıldığını söyler. Çıktı, yönlendirildiğinde de UTF-8'dir. Yerel raporu okur ve yüklemez.

Dosya makinenizde kalır. Paylaşmadıkça hiçbir şey yüklenmez.

## Kapatma

Rapor varsayılan olarak yazılır. Başlatma bayrağı `--aps5-report=off` ya da ortam değişkeni `APS5_REPORT=0` ile kapatın. Bayrak ortam değişkenine, ortam değişkeni de varsayılana (açık) üstündür. Bir ini ayarı yoktur. Bayrak, yeterince yeni bir relinker ile dönüştürülmüş bir oyun gerektirir; ortam değişkeni eski dönüştürmelerle de çalışır. Çalışma zamanı bayrağı komut satırından okur ve kaldırmaz, bu yüzden oyun argümanlarında `--aps5-report=...` değerini görebilir. Oyun kendini yeniden başlattığında seçim `APS5_REPORT` ile iletilir.

## Nereye yazılır

`anyps5-report.json`, oyunun çalıştırılabilir dosyasının yanına yazılır. O klasöre yazılamıyorsa, AnyPS5 veri klasöründe `reports/<run id>/` altına gider ve çalışma zamanı nereye gittiğini yazdırır. `run.id` aynı kimliktir: 32 küçük harfli onaltılık karakter, her çalıştırma için rastgele.

Rapor, çalıştırma guest'in `exit` veya `_exit` çağrısıyla bittiğinde ve Linux ile macOS'ta ayrıca giriş noktası döndüğünde ya da ana bilgisayar `std::exit` çağırdığında bir kez yazılır. Windows'ta yalnızca `exit` ve `_exit` rapor yazar: bir DLL'in çıkış işleyicileri yükleyici kilidi içinde çalıştığı için giriş noktasından dönüş hiçbirini yazmaz. Windows ve macOS kodu ile junction testi çalıştırılmamıştır. Hiçbir yerde yakalanmayan bir istisnada (`std::terminate`) duran bir çalıştırma, örneğin uygulanmamış bir fonksiyona çağrı, raporu terminate işleyicisi çalışmadan önce ya da oyun kendi işleyicisini kurduysa sonra, `abort` çıkışıyla yazar. Öldürülen ya da çöken bir çalıştırma rapor yazmaz.

## `anyps5.modified` ne anlama gelir

`false`, derlemenin git ağacında izlenen dosyalarda hiçbir değişiklik, git'in yok saymadığı izlenmeyen dosya ve bir alt modülün içinde değişiklik olmadığı anlamına gelir. `true`, bunlardan herhangi biri olduğu anlamına gelir. `null`, derlemenin bir git checkout'undan gelmediği anlamına gelir. Farklı derlemelerden gelen karışık bir kütüphane kümesi algılanmaz.

## Bilinmeyen null'dur

Çalışma zamanının bilemeyeceği bir değer, hiçbir zaman tahmin değil, `null`'dır. Bu `anyps5.*`, her `game.*` ve `system.*` alanı, `system.gpu` nesnesinin tamamı, `run.exitCode`, `stop.source` ve komut ayrıntıları için geçerlidir. Null'u yanlış ya da sıfır olarak değil, "bilinmiyor" olarak okuyun.

Rapordaki her metin değeri, kullanıcı adları, ana bilgisayar adları, adresler ve hesap numaraları için aynı süzgeçten geçer. Noktalarla ayrılmış dört sayıdan oluşan bir sürüm (`1.2.3.4`) bir IPv4 adresine benzer, bu yüzden bir adres yazma riskini almak yerine dışarıda bırakılır (null); üç sayılı sürümler (`01.002.000`) ve sürüm etiketleri (`v0.1.3`) korunur. Commit ve çalıştırma kimliği sabit uzunlukta onaltılık değerlerdir ve yalnızca desenleriyle denetlenir; bu yüzden derleme kimliği her zaman bulunur.

`game.titleId`, yalnızca `PPSA` ya da `CUSA` biçimiyle eşleştiğinde (dört büyük harf ve beş rakam) kopyalanır; aksi halde null'dır.

## Çalıştırma (Run)

| Alan | Anlamı |
|---|---|
| `stage` | Çalışma zamanının gözlemlediği en ileri dönüm noktası. `unknown` başlangıç değeridir. Çalışma zamanı hiçbir zaman `converts` iddia etmez: dönüştürme dönüştürücüde gerçekleşir. |
| `loads` | Çalışma zamanı oyunun giriş noktasına ulaştı. Import'lar daha sonra yüklenebileceği için bu, her import'un çözüldüğünü kanıtlamaz. |
| `starts` | Oyun koduna kendi iş parçacığında girildi. |
| `first-draw` | GPU'ya gerçek bir oyun çizimi ya da dispatch'i gönderildi. |
| `presents` | GPU görüntülenmek üzere bir oyun karesini kabul etti. Bu, karenin görünür ya da siyah olmadığını kanıtlamaz. |
| `milestones` | Başlangıçtan itibaren saniye. Ulaşılmayan dönüm noktaları dışarıda bırakılır. |
| `exit` | `clean`, `crash`, `abort` veya `killed`. Oyunun sıfırdan farklı bir çıkış kodu otomatik olarak bir çökme değildir. |
| `exitCode` | Biliniyorsa işlemin çıkış kodu, aksi halde null. |
| `averageFps`, `fpsWindowSeconds` | İlk sunulan kareden sonuncusuna kadar ölçülen saniyedeki kare sayısı ve o pencerenin uzunluğu. İkisi de ikiden az sunulan karede null'dır. |
| `frameMs` | Milisaniye cinsinden kare süresi yüzdelikleri: sunulan kareler arasındaki aralıkların, en son 2.048 aralık üzerinden `p50`, `p90` ve `p99`'u. İkiden az sunulan karede ya da dönüm noktaları kapsanmadığında null. Yüzdelik en yakın sıradır (nearest-rank); isteğe bağlı performans izlemesinin kullandığı aynı matematik. |
| `recovered`, `incomplete` | Bir gözlemin atılması gerektiğinde `incomplete` true'dur. |

## Çalıştırma tarifi (Run recipe)

`anyps5-report.json` dosyasının yanına çalışma zamanı `anyps5-recipe.json` yazar; bir raporun yeniden üretilebilmesi için çalıştırmanın nasıl başlatıldığını kaydeden küçük bir JSON dosyasıdır. `run.id`, `anyps5.commit` (derleme commit'i ya da null), `argv` (işlem argümanları), `environment` (yalnızca ayarlı `ANYPS5_*` değişkenleri) ve `configFiles` (çalıştırmaya verilen yapılandırma dosyalarının, örneğin girdi eşlemesinin, klasörleri olmadan dosya adları) içerir.

Değerler raporla aynı metin süzgecinden geçer, bu yüzden bir yol ya da kullanıcı adı `[redacted]` olarak görünür. Adı `TOKEN`, `KEY`, `PASSWORD` veya `SECRET` içeren bir değişken, değeri ne olursa olsun hiçbir zaman yazılmaz. Tarif yalnızca rapor yazıldığında yazılır; öldürülen ve çöken çalıştırmalar hakkındaki aynı kurallar geçerlidir.

## Durma (Stop)

`stop.kind`, çıkış yoluyla biten bir çalıştırma için `none` ve `stop.reason` `guest-exit`'tir. Ana bilgisayar önce oyundan çıkmasını istediğinde `stop.reason` `quit-requested`'dır: pencere kapatıldığında mesaj `The window was closed.` olur ve isteğin başka bir yoldan geldiğini, örneğin `timeout`'tan SIGTERM, Ctrl+C ya da bir Quit komutu, pencere kapatılmadan çıkması istendiğini söyler. `std::terminate` ile biten bir çalıştırmanın `stop.reason` değeri `runtime-stop`'tur: uygulanmamış bir fonksiyon fırlattığında `stop.kind`, fonksiyon ve `source` ile `missing-function`'dır; aksi halde istisnanın metni mesaj olarak `other`'dır. Şema diğer türleri ve nedenleri listeler; bu derleme yalnızca bunları yazar.

## `system` neyi tutar

`system.osVersion` işletim sistemi adı ve sürümüdür (Linux'ta dağıtımın `os-release` içindeki `NAME` ve `VERSION_ID` değeri ile çekirdek sürümünün ilk üç sayısı, örneğin `Ubuntu 24.04 (kernel 5.15.153)`; satıcının ya da kullanıcının çekirdek sürümüne eklediği her şey dışarıda bırakılır), `system.cpu` CPU model adıdır (` @ 3.50GHz` gibi sondaki saat hızı olmadan), `system.ramGiB` tam GiB cinsinden kurulu bellektir ve `system.gpu` çalışma zamanının seçtiği grafik aygıtıdır: `vendor` (PCI satıcı kimliğinden: `AMD`, `NVIDIA`, `Intel`, `Apple` veya `Other`), `name`, `driver` (sürücünün kendi adı ve sürüm metni) ve `vulkan` (aygıtın API sürümü). `gpu`, aygıt seçildiğinde ayarlanır; bu yüzden ondan önce duran bir çalıştırmanın raporunda null'dır. Bunların model ve sürüm adları olması amaçlanır ve diğer her metin gibi süzülürler: oturum adınızı, ev klasörünüzün adını, bir adresi, bir hesap numarasını, bir UUID'yi, bir yolu, bir e-posta adresini ya da seri numarasına benzer bir belirteci içeren bir dize rapordan çıkarılır (o alan null olur). Ana bilgisayar adınız da karşılaştırılır, tek bir istisnayla: yalnızca `system.osVersion` içinde, bir ana bilgisayar adı kaynaktaki sabit bir listeden (`ubuntu`, `debian`, `fedora`, `arch`, `cachyos`, `manjaro`, `nixos` vb.) herkese açık bir dağıtımın tanımlayıcısına eşitse ve makinenin `os-release` dosyasındaki `ID` aynı girdiyse izin verilir (CachyOS çalıştıran `cachyos` adlı bir makine `CachyOS`'u korur). `ID`, sahibinin düzenleyebileceği bir metindir, bu yüzden yalnızca ayrıştırılıyorsa (eşleşen tırnaklar ya da hiç tırnak yok) ve listedeyse sayılır; `NAME` ve `ID_LIKE` hiçbir zaman sayılmaz. Listede olmayan bir dağıtımın adını taşıyan ya da `os-release` dosyası başka bir şey söyleyen bir ana bilgisayar, `system.osVersion` içindeki işletim sistemi adını kaybeder. Eşleşmeyen tırnaklı bir `os-release` değeri kullanılmaz ve işletim sistemi sürümü o zaman kalana geri döner (örneğin `Linux (kernel 6.8.0)`). `cpu`, `gpu.name` ve `gpu.driver` içinde her ad karşılaştırılır. Bir oturum adına bu izin hiçbir zaman verilmez, bu yüzden dağıtımın adındaki bir sözcüğe eşit bir oturum adı (Arch Linux'ta `arch`) `system.osVersion`'ı null yapar. Oturum adınızı içeren herhangi bir makine dizesi bütün olarak temizlenir, bu yüzden `arc` oturum adı `Intel Arc`'ı da temizler. Seri numarasına benzer kural (ikisi de bulunan on iki ya da daha fazla harf ve rakam), Mesa'nın geliştirme derlemelerinin yaptığı gibi uzun bir git karması taşıyan bir sürücü dizesini de düşürür.

## Bir sistemin hangi dönüm noktalarını kaydettiği

| Sistem | `loaded`, `started` | `firstDraw`, `firstPresent` | `stage`, `framesPresented`, kare hızı |
|---|---|---|---|
| Linux | hiçbir zaman kaydedilmez | kaydedilir | kaydedilir |
| Windows | hiçbir zaman kaydedilmez | kaydedilir | kaydedilir |
| macOS | kaydedilir | kaydedilir | kaydedilir |

`coverage.milestones` her sistemde true'dur; aşamanın, sistemin kaydettiği dönüm noktalarının ve kare sayılarının toplandığı anlamına gelir. Linux ya da Windows'ta eksik bir `loaded` veya `started`'ı "ulaşılmadı" olarak değil, "bu sistem onu kaydetmiyor" olarak okuyun: `stage: first-draw` olup `loaded` olmayan bir rapor, çizim yapmış bir çalıştırmadır. Rapor yalnızca gözlemlenenleri söyler: eksik olan başka herhangi bir dönüm noktası çalıştırmanın ona ulaşmadığı ya da sistemin onu kaydetmediği anlamına gelebilir; bu yüzden bir boşluğu okumadan önce tabloyu okuyun. Özellikle `firstDraw`, tek başına gönderilip beklenen hesaplama işiyle ayarlanmaz.

## Çalışma zamanının saydıkları

- `milestones.firstDraw`, kaydedilmiş bir guest çizimi ya da dispatch'i taşıyan bir batch GPU kuyruğu tarafından kabul edildiğinde ayarlanır. Bir guest çizimi, çalışma zamanının gönderdiği her iki tür batch'te de sayılır; bir guest hesaplama dispatch'i yalnızca çalışma zamanının ana batch'ine kaydedildiğinde sayılır (tek başına gönderilip beklenen bir dispatch dönüm noktasını ayarlamaz). `firstPresent` ve `framesPresented`, swapchain kuyruğunun kabul ettiği guest ekran arabelleklerini sayar (suboptimal olan da dahil); dahili temizlemeler, test pikselleri ve reddedilen sunumlar sayılmaz. `loaded` ve `started`, dönüştürülmüş çalıştırılabilir dosyanın giriş köprüsünden gelir; şimdiye kadar yalnızca macOS köprüsü bunları bildirir, bu yüzden diğer sistemlerde bu ikisi dışarıda bırakılır.
- `missing.functions`, çalışma zamanının uygulamadığı bir fonksiyona çağrıları sayar. Kütüphane, stub'ın derlendiği klasördür; çağrının nerede yapıldığını söylemeyen eski koddan gelen bir çağrının kütüphanesi null'dır. Ad, çalışma zamanının sonekinden arındırılmış export adıdır; şemaya uymayan bir ad listelenmez ve `truncated.functions` içinde sayılır.
- `shaders.compiled` ve `failed`, derlemeleri gerçekleştikleri yerde sayar (varyant başına bir kez); bir varyantın bellekte ya da disk önbelleğinden yeniden kullanımı bir önbellek isabeti sayılır.
- `shaders.compileMs`, başarısız olanlar dahil bu derlemelerde harcanan toplam duvar saati süresidir, milisaniye cinsinden. `shaders.slowCompiles`, 100 ms'den fazla süren derlemeleri sayar. Shader kapsamı kapalıyken ikisi de dışarıda bırakılır. Bunlar oyunu çalıştıran makinede ölçülür, bu yüzden yalnızca aynı makinede karşılaştırın.
- `shaders.unsupported`, yeniden derleyicinin çeviremediği komutları, kodlama ailesi ve sayısal opcode'a göre listeler (ad null'dır). Ayrıca hazırlanıp hiç kullanılmayan shader'larda karşılaşılan komutları da listeler; bu yüzden bir girdi tek başına çalıştırmanın onda durduğu anlamına gelmez; durmayı `stop` söyler.
- `shaders.imageFormats`, dahili olanlar dahil guest biçim aramalarını, kullanımı `other` olarak gösterilerek ve çalışma zamanının reddettiği biçimlerle birlikte sayar. Bir biçimin kaç kez arandığını ölçer, kaç imajın onu kullandığını değil. `shaders.descriptors`, shader'ların bir çizim için hazırlandığı şekliyle bağlamalarını sayar; sayı kullanımı ölçer, ayrı shader'ları değil.
- `truncated`, atılan girdileri (bir fonksiyon, bir komut, bir biçim, bir bağlama türü), kaç çağrı yaptıklarından bağımsız olarak her biri için bir kez sayar. Toplayıcı meşgul olduğu için bir kanca bir gözlemden vazgeçmek zorunda kaldığında, eşleşen sayı en az 1'dir ve rapor `incomplete` olarak işaretlenir.
- Rapor kapalıyken bu listedeki hiçbir şey toplanmaz, sayılmaz ya da taranmaz.

## Listeler, kapsam ve kısaltma

- `missing.functions[]`: `library` null olabilir. `calls`, eksik fonksiyona ulaşan çağrıları sayar. İlki genellikle çalıştırmayı bitirir, bu yüzden 1 yaygındır; bu uygulanmamış her şeyin listesi değildir.
- `shaders.compiled` ve `failed`, gerçek derlemeleri sayar; önbellek isabetleri derleme değildir (`cacheHits` isteğe bağlıdır). `unsupported[]` içinde `encoding`, `opcode` (ad) ve `modifiers` vardır; başarısızlık tek bir guest komutuna sabitlenemediğinde her biri null'dır. Bilinmeyen bir sayısal opcode, null bir `opcode` ile numarasını `opcodeValue` içinde korur.
- `shaders.imageFormats[]` ve `descriptors[]` içinde `status` (`accepted`, `rejected` veya `unknown`), `usage` ve `count` vardır. `count` pikselleri değil, denetimleri sayar. Kabul edilmiş olmak her kullanımın çalıştığını kanıtlamaz.
  - İmaj biçimi `usage`: `sampled`, `storage`, `color-target`, `depth-target`, `transfer`, `other`.
  - Tanımlayıcı (descriptor) `usage` erişimdir: `read`, `write`, `atomic`, `other`.
- `coverage`, çalışma zamanının bu çalıştırma için hangi bölümleri topladığını söyler. Bir liste, hiçbir şey olmadığı için ya da bu derleme onu toplamadığı için boş olabilir. Bir bölümün kapsamı kapalıyken, boş listesini "eksik bir şey yok" olarak okumayın.
- `truncated`, sınırlandırılmış her listeden atılan girdileri sayar; böylece sınırlandırılmış bir liste hiçbir zaman eksiksiz görünmez.

Listeler sayıya göre, en yüksek ilk sırada sıralanır. En fazla 200 (`missing.functions`, `shaders.unsupported`) ya da 100 (`imageFormats`, `descriptors`) girdi tutarlar; dizeler en fazla 300 karakterdir. Dosyanın tamamı en fazla 65.536 bayt UTF-8'dir. Daha büyük olacağı zaman çalışma zamanı önce en düşük sıradaki liste girdilerini, sonra isteğe bağlı açıklamaları atar ve derleme, oyun ya da durma ayrıntılarını hiçbir zaman atmaz.

## Gizlilik

Yalnızca bilinen, yapılandırılmış değerler kopyalanır. Çalışma zamanının kefil olamayacağı serbest biçimli metin dışarıda bırakılır (null). Dosya kullanıcı adları, ev yolları, ana bilgisayar adları, IP ya da MAC adresleri, seri numaraları, hesap kimlikleri, kayıt verisi ya da ekran görüntüsü içermez. Yollar AnyPS5 kaynak ağacına görelidir.
