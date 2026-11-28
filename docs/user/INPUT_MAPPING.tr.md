# Kumanda, klavye ve fare girdisi

AnyPS5, yerleşik klavye ve fare bağlamalarının yanı sıra SDL ile eşlenmiş oyun kumandalarını kabul eder. Aynı anda en fazla 4 kumanda desteklenir. İlk bağlanan kumanda Oyuncu 1'e atanır ve klavye ve fare girdisiyle birleştirilir; ek kumandalar sırayla Oyuncu 2, 3 ve 4'e atanır (`libSceUserService` kullanıcılarıyla eşleşir). SDL'nin tanıdığı Xbox tarzı kumandalar ve PlayStation kumandaları standart PS düğme yerleşimini kullanır; çubuklar ve analog tetikler olduğu gibi aktarılır ve kumandalar oyun çalışırken dinamik olarak takılıp çıkarılabilir. AnyPS5, hiçbir yapılandırma dosyası yokken yerleşik girdi bağlamalarını kullanır. Girdi bağlamalarını değiştirmek için, oluşturulan oyun çalıştırılabilir dosyasının yanında `anyps5-input.ini` oluşturun. Başka bir yoldaki dosyayı kullanmak için `ANYPS5_INPUT_CONFIG` ayarlayın. Hem oyun dizini hem de bu yol `ü` veya `日本` gibi her türlü karakteri içerebilir. `ANYPS5_PAD_PORT0_STATE=controller` ile klavye ve fare girdisi oyuna yalnızca Oyuncu 1'e bir kumanda bağlıyken ulaşır; bkz. [Kumanda bağlantısı ve kullanıcı 0](USAGE.tr.md#kumanda-bağlantısı-ve-kullanıcı-0).

Xbox yerleşimli bir kumanda için A/B/X/Y düğmeleri Cross/Circle/Square/Triangle'a, LB/RB düğmeleri L1/R1'e ve LT/RT düğmeleri analog L2/R2'ye eşlenir. Start, Options'a eşlenir. Yön tuşu (D-pad) ve çubuğa basma düğmeleri desteklenir. Bir PlayStation kumandasında dokunmatik yüzey tıklamaları ve dokunmatik yüzeydeki en fazla iki parmağın konumları iletilir. Diğer kumandalarda View (Back), dokunmatik yüzeye tam ortasından basar.

Klavye, fare, tekerlek ve kısayol tuşu bağlamaları `Action = Type:Value` biçimini kullanır. Eylem adlarında büyük/küçük harf önemsizdir. `#` veya `;` bir yorum başlatır. Bir eylem için ilk satır yerleşik bağlamalarının yerini alır; aynı eylem için sonraki satırlar alternatif girdiler ekler. Aynı eylem için yinelenen girdiler yok sayılır. Dosyada atlanan eylemler yerleşik bağlamalarını korur. BOM'lu ya da BOM'suz UTF-8 dosyaları desteklenir.

Desteklenen girdi kaynakları şunlardır:

- `KEY:Return`, `KEY:Space` veya SDL'nin kabul ettiği başka bir tuş adı.
- `MOUSE:Left`, `MOUSE:Middle`, `MOUSE:Right`, `MOUSE:X1` veya `MOUSE:X2`.
- Kumanda düğmeleri için `WHEEL:Up`, `WHEEL:Down`, `WHEEL:Left` veya `WHEEL:Right`. `Left` ve `Right` yatay kaydırma adımlarıdır, örneğin yana eğilen bir tekerleğin yan tıklamaları; hiçbir eylem bunları varsayılan olarak kullanmaz.
- Yalnızca emülatör eylemleri için `PAD:Button` veya `PAD:Button+Button+...`: hepsi aynı anda basılı tutulan kumanda düğmelerinden oluşan bir akor. Düğme adları `Cross`, `Circle`, `Triangle`, `Square`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `Options`, `Up`, `Right`, `Down`, `Left` ve `TouchPad`'dir. Akor, son düğmesi aşağı indiğinde bir kez tetiklenir ve düğmeler yine oyuna ulaşır. `PAD:`, `ToggleFullscreen`, `TogglePerformanceOverlay`, `Screenshot`, `ShowPadStatus`, `VolumeUp`, `VolumeDown`, `ToggleMute` ve `ReloadInputMapping` için kabul edilir; diğer eylemler onu reddeder. Örnek: `ToggleFullscreen = PAD:Options+R1`.

Bir oyun fareyi açmışken (`sceMouseOpen`) ve pencere odaktayken AnyPS5, ana bilgisayar imlecini gizler ve yakalar; böylece oyunun kendi imleci tek imleç olur; yakalama, pencere odağı kaybettiğinde ya da fare-çubuk geçişi (`ToggleMouse`) açık olduğunda biter. Oyun fareye sahipken fare tıklamaları ve tekerlek, kumanda bağlamaları yerine oyuna gider. `ToggleMouse`'u tuşuyla (varsayılan olarak F7) açmak, kapatılana kadar fareyi tamamen kumandaya devreder: hareket çubuğu oynatır, tıklamalar ve tekerlek kumanda bağlamalarına basar; oyunun faresi ise hiçbir hareket, tıklama ya da tekerlek dönüşü bildirmez. Fareyi açan ama oyun sırasında yok sayan bir oyun bu şekilde oynanabilir. Fare hareketi, pencerenin sunulan imajından oyunun ekran çözünürlüğüne ölçeklenerek oyuna ulaşır; böylece oyunun imleci ana bilgisayar imlecinin gideceği kadar hareket eder.

Pencere kısayol tuşları (`ToggleFullscreen`, `TogglePerformanceOverlay`, `Screenshot`, `ShowPadStatus`, `VolumeDown`, `VolumeUp` ve `ToggleMute`) klavye tuşlarını ve `PAD:` akorlarını kabul eder. Kısayol tetiklendiğinde akordaki düğmeler yine oyuna ulaşır. Bir kısayola ve başka bir eyleme bağlanmış bir tuş, diğer her eylemde olduğu gibi ikisini de tetikler; yukarıdaki eylemlerin yerleşik tuşları hiçbirini paylaşmaz.

Oyun kumandası bağlamaları otomatik olarak etkinleştirilir. Klavye ve fare geçersiz kılmaları, [kumanda düğmesi yeniden eşlemeden](#kumanda-düğmesi-yeniden-eşleme) bağımsız olarak uygulanır.

Desteklenen eylemler `Cross`, `Circle`, `Triangle`, `Square`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `Options`, `Up`, `Right`, `Down`, `Left`, `LeftStickLeft`, `LeftStickRight`, `LeftStickUp`, `LeftStickDown`, `RightStickLeft`, `RightStickRight`, `RightStickUp`, `RightStickDown`, `TouchLeft`, `TouchRight`, `StickModifier`, `Shake`, `ToggleMouse`, `ToggleFullscreen` (F11), `TogglePerformanceOverlay` (F10, [performans katmanı](USAGE.tr.md#performans-katmanı-overlay)), `Screenshot` (F12, bir sonraki görüntülenen kareyi [ekran görüntüleri](USAGE.tr.md#ekran-görüntüleri) bölümünde açıklandığı gibi kaydeder) ve `ShowPadStatus`'tür (F9, [kumanda durum panelini](USAGE.tr.md#kumanda-durum-paneli) birkaç saniye gösterir). `ToggleFullscreen`, `TogglePerformanceOverlay`, `Screenshot` ve `ShowPadStatus` ayrıca `PAD:` akorlarını kabul eder.

### SDL kumanda algılama ve düzeni

SDL bir joystick görüyor ama oyun `NO CONTROLLER` gösteriyorsa ya da yön tuşu veya düğmeler yanlış girdi bildiriyorsa, kumandanın GUID'si ve işletim sistemi için bir SDL2 eşlemesi sağlayın. USB ve Bluetooth farklı eşlemeler kullanabilir. [SDL GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB) sayfasından `gamecontrollerdb.txt` dosyasını indirin ya da SDL2'nin `controllermap` aracındaki eşleme satırını, `platform:` alanı dahil, düz bir metin dosyasına kaydedin.

`gamecontrollerdb.txt` dönüştürülmüş oyunun yanındayken oyunu o dizinden başlatın. Windows'ta ayar, aynı PowerShell oturumundan başlatılan programlara, oturum kapanana kadar uygulanır.

```powershell
$env:SDL_GAMECONTROLLERCONFIG_FILE = (Resolve-Path -LiteralPath '.\gamecontrollerdb.txt').Path
.\app.exe
```

Linux'ta değişkeni oyun işlemi için ayarlayın.

```sh
SDL_GAMECONTROLLERCONFIG_FILE="$PWD/gamecontrollerdb.txt" ./app.elf
```

[Kumanda durum panelini](USAGE.tr.md#kumanda-durum-paneli) denetlemek için F9'a basın. SDL dosyayı açılışta yükler. Eşleşen bir dosya girdisi SDL'nin yerleşik eşlemesinin yerini alır ve doğrudan `SDL_GAMECONTROLLERCONFIG` içinde ayarlanan bir eşleme dosyadan önceliklidir. `anyps5-input.ini` içindeki `SDLButton:` girdileri sonra ortaya çıkan SDL düğmelerini PlayStation düğmelerine atar.

### Kumanda düğmesi yeniden eşleme

Bir SDL kumanda düğmesini bir PlayStation düğmesine atamak için `SDLButton:button = Button` kullanın. Örneğin bu, Cross ile Circle'ı değiştirir, ardından Back düğmesini devre dışı bırakır.

```ini
SDLButton:a = Circle
SDLButton:b = Cross
SDLButton:back = None
```

Her SDL düğmesi dosyada bir kez görünebilir. Atlanan bir düğme varsayılanını ya da paylaşılan dosyadaki atamasını korur. Bu geçersiz kılmalar tüm kumanda yuvalarına uygulanır. SDL düğme adlarında büyük/küçük harf önemsizdir ve aygıta bağlı olarak `a`, `b`, `x`, `y`, `back`, `start`, `leftshoulder`, `rightshoulder`, `leftstick`, `rightstick`, `dpup`, `dpdown`, `dpleft`, `dpright`, `guide`, `misc1`, `paddle1` ile `paddle4` arası ve `touchpad` adlarını içerir. Hedef, `PAD:` akorlarıyla aynı düğme adlarını ya da `None` kabul eder. Bir düğmeyi `L2` veya `R2`'ye eşlemek, tam bir tetik basışı üretir. Çubuklar ve fiziksel analog tetikler olağan davranışlarını korur. `PAD:` kısayolları, ortaya çıkan PlayStation düğme atamalarını izler.

### Eşlemeyi yeniden yükleme

`ReloadInputMapping` (varsayılan olarak F1), oyun çalışmaya devam ederken `anyps5-input.ini` dosyasını ve oyunun profilini yeniden okur; böylece bir değişikliği oyunu yeniden başlatmadan deneyebilirsiniz. Yeni dosya, hiçbir şey değişmeden önce tamamen okunur: bir hatası varsa önceki eşleme etkin kalır ve günlük dosyayı, satırı ve nedeni adlandırır. Başarılı bir yeniden yükleme, tüm eşlemenin yerini bir kerede alır.

Dosya ya da oyunun profili değiştiğinde otomatik olarak yeniden yüklemek için `ANYPS5_INPUT_WATCH=1` ayarlayın. AnyPS5, değiştirilme zamanını ve boyutunu bir arka plan iş parçacığında yaklaşık saniyede bir denetler ve yalnızca bir değişiklik bir denetim boyunca aynı kaldıktan sonra yeniden yükler; böylece hâlâ kaydedilmekte olan bir dosya yarım okunmaz. Seçenek varsayılan olarak kapalıdır.

`ShowPadStatus`'e ve başka bir eyleme bağlanmış bir tuş, diğer her eylemde olduğu gibi ikisini de tetikler. F9, `ShowPadStatus`'ün yerleşik tuşudur: `Screenshot`'ı, `TogglePerformanceOverlay`'i ya da başka bir eylemi F9'a taşıdıysanız, `ShowPadStatus`'ü de kullanmadığınız bir tuşa bağlayın; aksi halde her basış kumanda durum panelini de gösterir. [Web yapılandırıcı](#web-yapılandırıcı), iki eylemin paylaştığı bir tuş için uyarır.

Üç eylem daha ana bilgisayarın çıkış ses düzeyini kontrol eder: `VolumeDown` (F5), `VolumeUp` (F6) ve `ToggleMute` (F8); [ana bilgisayar ses düzeyi ve sessize alma](USAGE.tr.md#ana-bilgisayar-ses-düzeyi-ve-sessize-alma) bölümünde açıklanmıştır. Bunlar da `PAD:` akorlarını kabul eder.

### Varsayılan bağlamalar

`anyps5-input.ini` olmadan ya da dosyanın anmadığı eylemler için bu bağlamalar geçerlidir:

| Eylem | Klavye ve fare |
|--------|--------------------|
| `Cross` | Return, Space |
| `Circle` | C |
| `Triangle` | I |
| `Square` | Sol fare düğmesi |
| `L1` | Q |
| `R1` | E, Sol Alt, Sağ Alt |
| `L2` | R, Fare düğmesi 4 (X1) |
| `R2` | Sağ fare düğmesi |
| `L3` | Sol Shift, Sağ Shift |
| `R3` | Sol Ctrl, Sağ Ctrl |
| `Options` | Escape |
| `Up`, `Right`, `Down`, `Left` | Ok tuşları; fare tekerleği de `Up` (tekerlek yukarı) ve `Down` (tekerlek aşağı) düğmelerine basar |
| `LeftStickLeft`, `LeftStickRight`, `LeftStickUp`, `LeftStickDown` | A, D, W, S |
| `RightStickLeft`, `RightStickRight`, `RightStickUp`, `RightStickDown` | F, H, T, G |
| `TouchLeft`, `TouchRight` | Backspace, Tab |
| `Shake` | X (basılı tutulduğu sürece sallanan bir ivme ve açısal hız bildirir) |
| `ToggleMouse` | Orta fare düğmesi, F7 |
| `ToggleFullscreen` | F11 |
| `TogglePerformanceOverlay` | F10 |
| `Screenshot` | F12 |
| `ShowPadStatus` | F9 |
| `VolumeDown` | F5 |
| `VolumeUp` | F6 |
| `ToggleMute` | F8 |
| `ReloadInputMapping` | F1 |

Örneğin bu, Cross'u F veya Space'e değiştirir, sol çubuğu IJKL'ye taşır, Square ve R2 için fare düğmelerini kullanır, orta fare düğmesini tek `ToggleMouse` girdisi yapar (satır F7'nin yerini alır) ve diğer tüm yerleşik bağlamaları korur:

```ini
Cross = KEY:F
Cross = KEY:Space
LeftStickLeft = KEY:J
LeftStickRight = KEY:L
LeftStickUp = KEY:I
LeftStickDown = KEY:K
Square = MOUSE:Left
R2 = MOUSE:Right
ToggleMouse = MOUSE:Middle
```

Aynı dosya, her biri en fazla bir kez ve `Setting = Value` biçiminde şu ayarları da kabul eder:

| Ayar | Değer | Varsayılan |
|---------|-------|---------|
| `LeftStickDeadzone`, `RightStickDeadzone` | Kumanda çubuğunun hareketinin, merkezde okunan, 0 ile 90 arasında tam sayı yüzdesi. Bunun dışında hareket yeniden ölçeklenir, böylece çubuk yine tam aralığına ulaşır. Klavye ve fare çubuk bağlamaları etkilenmez. | `0` |
| `MouseSensitivity` | 0'dan büyük ve en fazla 10 olan sayı. 1'de, fareyi saniyede 3000 sayım hareket ettirmek çubuğu tamamen yatırır; 1600 DPI'da saniyede yaklaşık 5 cm; daha büyük bir sayı daha az hareket gerektirir. | `1` |
| `MouseStickTarget` | `Left` veya `Right`. `ToggleMouse` etkinken hangi analog çubuğun fare hareketiyle kontrol edileceğini seçer. | `Right` |
| `MouseVerticalScale` | Dikey fare hızını çarpan, 0,1 ile 10 arasında bir sayı; dönmekten daha yavaş ya da daha hızlı eğilen (pitch) oyunlar için. | `1` |
| `MouseCurve` | 0,2 ile 5 arasında bir sayı, yanıt üssü. 1'in üstü düşük hızda daha ince kontrol verir; 1'in altı büyük eğimlere daha erken ulaşır. | `1` |
| `MouseDeadzoneOffset` | 0 ile 0,9 arasında bir sayı; herhangi bir hareketin ürettiği, tam eğimin kesri olarak en küçük eğim. Oyunun kendi ölü bölgesinin hemen üstüne ayarlayın: yavaş hareket kamerayı oynatmıyorsa artırın, küçük hareketler sıçrıyorsa azaltın. | `0.2` |
| `MouseMaxSpeed` | 0,1 ile 10 arasında bir sayı; izlenen en yüksek hız, tam eğim hızının katı olarak. 1'in üstünde, hızlı bir savuruş, fare durduktan sonra çubuğu biraz daha uzun süre tam eğimde tutar. | `1` |
| `MouseSmoothingMs` | 0 ile 500 arasında bir sayı; milisaniye cinsinden yumuşatma zaman sabiti. Saniyede 300'den az bildiren bir fare, örneğin 125 ya da 250 Hz'lik bir fare, en az üç rapor aralığı boyunca, en fazla 30 ms'ye kadar yumuşatılır; rapor hızı hareketin kendisinden ölçülür. 0 her fare raporunu doğrudan izler ve bunu da kapatır. | `10` |
| `MouseDecayMs` | 1 ile 1000 arasında bir sayı; hareketsiz geçen, ardından çubuğun ortalandığı milisaniye. | `30` |
| `StickModifierScale` | 0,05 ile 1 arasında bir sayı; bir `StickModifier` bağlaması basılıyken klavye çubuk bağlamalarının eğimi. | `0.5` |
| `KeyboardRampMs` | 0 ile 2000 arasında bir sayı; bir klavye çubuk bağlamasının çubuğu merkezden tam eğime taşıması için gereken milisaniye. 0 hemen yatırır. | `0` |
| `TriggerRampMs` | 0 ile 2000 arasında bir sayı; `L2` veya `R2`'ye bağlı bir tuşun ya da fare düğmesinin tetiği 0'dan tama basması için gereken milisaniye. 0 onu hemen tamamen basar. | `0` |
| `ToggleActions` | Virgülle ayrılmış kumanda düğmeleri (`Cross`, `Circle`, `Triangle`, `Square`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `Options`, `Up`, `Right`, `Down`, `Left`); her biri ona bağlı tuşlar ve fare düğmeleriyle açılıp kapatılır. Çubuk yönleri ve emülatör eylemleri reddedilir. Bkz. [Geçişli düğmeler](#geçişli-düğmeler). | yok |

```ini
LeftStickDeadzone = 12
RightStickDeadzone = 8
MouseSensitivity = 2.5
```

## Kumanda zaman çizelgeleri

[Test çalıştırmaları için kumanda girdisi](USAGE.tr.md#test-çalıştırmaları-için-kumanda-girdisi) bölümünde açıklandığı gibi `APS5_PAD_INPUT_FILE` ayarlayın, ardından uygulama kumandasını okumaya başladıktan sonra tam bir düğme dizisini kuyruğa alın. `menu.json` gibi bir JSON dosyası kaydedin:

```json
{"version":1,"events":[{"at_ms":200,"hold_ms":300,"buttons":["cross","r1"]},{"at_ms":1000,"hold_ms":300,"buttons":["down"]}]}
```

```sh
python3 tools/pad_timeline.py menu.json /tmp/anyps5-pad --repeat 3 --gap-ms 500
```

Olaylar, uygulamanın yeni komutu ilk gözlemlediği ana göre başlar. Düğme listeleri `pad_input.py` içindeki her adı destekler; eşzamanlı adlar birleştirilir. Boş bir liste ya da `["release"]` hiçbir düğme enjekte etmez. Bir olay ayrıca `left_stick` ve `right_stick` konumlarını 0 ile 255 arasında `[x, y]` olarak (128 ortadır) içerebilir; bir olayın adlandırdığı ama diğerinin dışarıda bıraktığı bir çubuk o olay için ortada kalır ve ikisini de içermeyen bir olay fiziksel çubukları korur; örneğin sol çubuğu sağa itmek için `{"at_ms":0,"hold_ms":400,"buttons":[],"left_stick":[255,128]}`. Olaylar sıralı ve çakışmayan olmalı, 1–5000 ms boyunca tutmalı ve bir saat içinde bitmelidir. Tekrar, son olaydan sonra belirtilen boşluğu ekler; tekrardan sonra en fazla 4096 olay kabul edilir ve ürettikleri komut dosyası, çalışma zamanının okuduğu 64 KiB'ı aşmamalıdır. Yardımcı araç, komut dosyasını atomik olarak değiştirmeden önce tüm diziyi doğrular.

Çalışma zamanı, zamanlanmış olayları, otomatik bırakmaları dahil, her kumanda okumasında değerlendirir. Yeni bir darbe ya da zaman çizelgesi kalan dizinin yerini alır. Fiziksel kumanda düğmeleri enjekte edilen düğmelere eklenmeye devam eder. Uygulamanın ilk kumanda okumasında var olan komutlar yok sayılır. Bu, duvar saati zamanlı girdiyi zamanlar: uygulama kumandasını okumuyorken olaylar kaçırılabilir ve kare zamanlamasını ya da oyun durumunu yeniden üretmez. Oyunun girdi döngüsüne yetecek kadar uzun boşluklar ve tutmalar kullanın.

## Kumanda girdisini kaydetme ve yeniden oynatma

Siz oynarken bir oyunun kumanda port 0'dan okuduğunu kaydetmek için `APS5_PAD_RECORD` değerini bir dosya yoluna ayarlayın: hangi aygıt üretmiş olursa olsun, düğmelerinin, çubuklarının ve analog tetiklerinin her değişikliği, oyunun ilk kumanda okumasından itibaren milisaniye olarak zamanlanmış. Aynı durumları, o çalıştırmanın ilk okumasından itibaren aynı zamanlarda, hiçbir aygıt olmadan geri beslemek için sonraki bir çalıştırmada `APS5_PAD_REPLAY` değerini o dosyaya ayarlayın. Yeniden oynatma, port 0'ın düğmelerinin, çubuklarının ve analog tetiklerinin yerini her okumada alır; son kaydedilen değişiklikten sonra son durumu korur. Yazılamayan bir kayıt yolu ya da okunamayan veya aşağıdaki biçimde olmayan bir yeniden oynatma dosyası, oyunu ilk kumanda okumasında değişkeni adlandıran bir hatayla durdurur.

```sh
APS5_PAD_RECORD=/tmp/fight.txt ./app.elf
APS5_PAD_REPLAY=/tmp/fight.txt APS5_NO_PAD_INPUT=1 ./app.elf
```

Dosya `APS5_PAD_RECORDING 1` satırıyla başlar, ardından değişiklik başına bir satır gelir: milisaniye cinsinden zaman, onaltılık düğmeler (`pad_input.py` değerleri), sol ve sağ çubuk x ile y ve L2 ile R2, her biri 0 ile 255 arası. Yeniden oynatma, oyunun karelerini değil, kaydedilen zamanları izler: daha yavaş yüklenen ya da başka bir kare hızında çalışan bir çalıştırma ya da farklı rastgele sayılar çeken bir oyun, kaydedilen oturumdan sapar; bu yüzden çalıştırmaları saniyeye göre değil, sahneye göre karşılaştırın.

## Kumanda titreşim gücü

Titreşim, girdi eşleme dosyasının parçası değildir. Bir kumandanın titreşiminin ana bilgisayar tarafındaki gücü, [Kumanda titreşim gücü](USAGE.tr.md#kumanda-titreşim-gücü) bölümünde açıklanan `ANYPS5_RUMBLE` ile ayarlanır; `0` onu bağlı her kumanda için kapatır. Titreşim ayrıca, `ANYPS5_RUMBLE_ON_FOCUS_LOSS=0` ayarlanmadıkça pencere odakta değilken duraklar; bkz. [Pencere odakta değilken titreşim](USAGE.tr.md#pencere-odakta-değilken-titreşim).

Oyuncuların kumandalarını birbirinden ayıran tanımlama darbesi, [Kumanda tanımlama darbesi](USAGE.tr.md#kumanda-tanımlama-darbesi) bölümünde açıklanan `ANYPS5_PAD_IDENTIFY` ile ayarlanır.

## Kararlı oyuncu ataması

Varsayılan olarak bağlı kumandalar, SDL'nin keşif sırasına göre ilk boş oyuncu yuvasını doldurur. Bir kumandayı yeniden bağlandıktan ya da yeniden başlattıktan sonra aynı oyuncuda tutmak için, `ANYPS5_PAD_PLAYER_1_SERIAL` ile `ANYPS5_PAD_PLAYER_4_SERIAL` arasındaki değişkenleri her oyuncu için tam SDL seri numarasına ayarlayın. Bunu yapılandırırken seri numaralarını bağlantı günlüğüne eklemek için `ANYPS5_PAD_LOG_CONTROLLER_SERIAL=1` ayarlayın; seri numarası günlüğü varsayılan olarak kapalıdır. SDL her kumanda ya da arka uç için bir seri numarası sunmayabilir. Bu kumandalar ve seri numarası yapılandırılmamış kumandalar, ayrılmamış ilk boş oyuncu yuvasını kullanır. Yapılandırılmış bir yuva, kumandasının bağlantısı kesilmişken de ayrılmış kalır; böylece diğer kumandalar o oyuncunun yerini almaz. Aynı seri numarasını birden fazla oyuncuya atamak, açılışı bir hatayla durdurur.

Örneğin iki kumandayı bağlamak ve diğer oyuncu yuvalarını otomatik bırakmak için:

```sh
ANYPS5_PAD_PLAYER_1_SERIAL=ABC123 ANYPS5_PAD_PLAYER_2_SERIAL=XYZ789 ./AnyPS5
```

Bir kumanda başka bir aktarım yoluyla bağlandığında ya da farklı bir SDL arka ucu tarafından sunulduğunda seri numaraları farklı olabilir. Geçerli ana bilgisayarın bildirdiği tam değer için bağlantı günlüğüne bakın.

## Analog klavye girdisi

`StickModifier`'ın yerleşik bir bağlaması yoktur. Ona bağlı bir tuş ya da fare düğmesi basılıyken, klavye çubuk bağlamaları çubuğu tamamen değil, yürüme ya da ince nişan için aralığının `StickModifierScale` kadarı eğer. Kumanda çubukları ve fare çubuğu etkilenmez ve zıt tuşlar yine birbirini iptal eder.

```ini
StickModifier = KEY:Z
StickModifierScale = 0.4
```

`KeyboardRampMs` 0'ın üzerindeyken, klavye çubuk bağlamaları çubuğu eğimlerine sıçramak yerine sabit bir hızla doğru hareket ettirir ve tuşlar bırakıldığında aynı şekilde merkeze geri getirir. Birlikte basılan zıt tuşlar çubuğu aynı hızla merkeze taşır.

`TriggerRampMs` 0'ın üzerindeyken, `L2` veya `R2`'ye bağlı bir tuş ya da fare düğmesi analog tetiğe kademeli olarak basar ve aynı şekilde bırakır; yarış oyunlarında gaz ve fren için. Basılı bir kumanda tetiği yavaşlatılmaz ve iki değerin büyüğü iletilir.

## Geçişli düğmeler

`ToggleActions` içinde listelenen bir kumanda düğmesi, basılı tutulmak yerine mandallanır: ona bağlı bir tuşun ya da fare düğmesinin ilk basışı düğmeye basar ve tuş ya da fare düğmesi bırakıldıktan sonra düğme basılı kalır. Sonraki basış onu bırakır. Aynı eyleme bağlı her tuş ve fare düğmesi bu tek durumu paylaşır, bu yüzden herhangi birine basmak onu değiştirir. Listelenmeyen düğmeler normal davranışı korur: tuş basılıyken basılı. Odağı kaybetmek her geçişli düğmeyi bırakır.

Fare tekerleği bağlamaları geçişli değildir; eskisi gibi düğmeye kısa bir an basmaya devam ederler.

```ini
ToggleActions = R2, L1
R2 = KEY:Z
```

Burada Z ilk basışta R2'ye basar, ikincide bırakır ve Q (varsayılan olarak L1'e bağlı) L1 için aynısını yapar. Bağlaması olmayan listelenmiş bir eylem yok sayılır. `ToggleActions` ayarlayan oyun başına bir profil, paylaşılan dosyadaki listenin yerini alır; boş bir değer ya da bilinmeyen bir eylem hatadır.

## Sallama (Shake)

`Shake` girdisi basılı tutulduğu sürece (varsayılan olarak X), kumanda ivmesini ve açısal hızını her 50 ms'de ileri geri sallanıyormuş gibi bildirir; oyuncudan kumandayı sallamasını isteyen oyunlar için. Aksi halde kumandanın kendi hareket sensörleri kullanılır.

## Oyun başına profiller

`anyps5-input.ini` yanında (ya da `ANYPS5_INPUT_CONFIG`'in adlandırdığı dosyanın yanında) `anyps5-input.<TITLEID>.ini` adlı bir dosya, çalışan oyunun o kimliği olduğunda, ondan sonra okunur; örneğin `anyps5-input.PPSA00001.ini`. Profil aynı biçime sahiptir ve yalnızca listelediği eylemleri, ayarları ve SDL kumanda düğmelerini geçersiz kılar. Diğer oyunlar onu okumaz. Bir oyun kimliği harf, rakam, `_` ve `-` içerebilir. Bir profildeki geçersiz satır, profilin dosyasını ve satır numarasını bildirir.

Örneğin bir oyuna daha düşük fare hassasiyeti ve F'de Cross veren bir profil:

```ini
MouseSensitivity = 0.6
Cross = KEY:F
```

## Fare analog çubuk olarak

`ToggleMouse` (varsayılan olarak orta fare düğmesi ya da F7; orta düğmesi olmayan bir fare F7'yi kullanabilir), fareyi yakalar ve `MouseStickTarget`'in seçtiği analog çubuğu (varsayılan olarak `Right`) farenin ne kadar hızlı hareket ettiğine göre sürer. Yakalama ham göreli girdi kullanır, bu yüzden işletim sisteminde ayarlanan imleç hızı ve hızlanma uygulanmaz. Fareyi bırakmak için yeniden basın. Pencere odağı kaybettiğinde de yakalama biter ve basılı her tuş bırakılır.

Çubuk, farenin hareket ettiği yöne ve ne kadar hızlı hareket ederse o kadar öteye eğilir. Hız, güncellemeler arasındaki süre boyunca saniyedeki sayım cinsinden ölçülür, bu yüzden aynı el hızı her kare hızında aynı eğimi verir. Herhangi bir hareket çubuğu en az `MouseDeadzoneOffset` kadar eğer, böylece yavaş hareket oyunun kendi ölü bölgesinde kaybolmaz. Fare durduğunda çubuk geri yumuşar ve son hareketten `MouseDecayMs` sonra ortalanır.

Sol analog çubuğu fare hareketiyle kontrol etmek için:

```ini
MouseStickTarget = Left
```

Örneğin küçük bir ölü bölgesi olan ve hızlı dönüşlere ihtiyaç duyan bir oyun için:

```ini
MouseSensitivity = 1.6
MouseDeadzoneOffset = 0.12
MouseCurve = 1.3
```

Önceki sürümler fareyi her 33 ms'de örnekliyor ve herhangi bir hareket için çubuğu en az yarıya kadar eğiyordu. `MouseDeadzoneOffset = 0.5`, `MouseSmoothingMs = 0` ve `MouseDecayMs = 33` bu davranışa yaklaşır.

Geçersiz bir satır, video çıkışı başladığında oyunu, dosyayı ve satır numarasını adlandıran bir hatayla durdurur. `ANYPS5_INPUT_CONFIG`'in adlandırdığı dosya yoksa ya da yapılandırma dosyası okunamıyorsa, oyun bir hatayla durur. `anyps5-input.ini` ve `ANYPS5_INPUT_CONFIG` olmadan yerleşik eşleme kullanılır.

## Web yapılandırıcı

[Girdi eşleyici](../../tools/input-mapper/index.html), klavye, fare, tekerlek ve kumanda akoru bağlamalarını tarayıcıda düzenler. Alternatif bağlamaları, fiziksel tuş yakalamayı, aramayı, INI içe aktarmayı, önizlemeyi, kopyalamayı ve indirmeyi destekler. Eylem ya da grup adına, `F7` gibi tuş adına ya da `MOUSE:Left` gibi tam girdiye göre arayın. Girdi aramaları, yapılandırılmış bağlamaları ve geçersiz kılması olmayan eylemler için yerleşik bağlamaları içerir. Geçersiz içe aktarmalar geçerli yapılandırmayı korur. Geçersiz bağlamalar dışa aktarmayı engeller; yinelenen ve paylaşılan girdiler, korunan yerleşik bağlamalar dahil, uyarı üretir. Bir eylemin tüm bağlamalarını kaldırmak yerleşik girdilerini geri yükler.

`ToggleFullscreen` gibi bir emülatör eylemi için bir bağlama ekleyin, girdi türü olarak `PAD`'i seçin ve bağlama değerinde bir ya da daha fazla kumanda düğmesi seçin. İçe aktarılmış bir `PAD:` bağlamasını düğmelerini düzenlemek için aynı şekilde açın.

Yapılandırma sayfasında, [oyun başına profili](#oyun-başına-profiller), örneğin `anyps5-input.PPSA00001.ini`, indirmek için oyunun tam kimliğini girin. Paylaşılan `anyps5-input.ini` dosyasını indirmek için alanı boş bırakın.

`MouseSensitivity` ya da `ToggleActions` gibi ayarları ve `SDLButton:` satırlarını elle ekleyin. Yapılandırıcı bunları içe aktarmada doğrular ve INI önizlemesinde, kopyalanan metinde ve indirilen dosyada korur. Geçersiz veya yinelenen ayarlar ve kumanda girdileri satır numaralarıyla bildirilir.

Depo kökünden, Node.js 22 veya daha yenisiyle:

```sh
npm --prefix tools/input-mapper start
```

`http://127.0.0.1:4173` adresini açın. Sunucuyu Ctrl+C ile durdurun. Paket kurulumu gerekmez. Başka bir port kullanmak için `PORT` ayarlayın. Testleri şununla çalıştırın:

```sh
npm --prefix tools/input-mapper test
```

Kontroller, Yapılandırma ve Yardım ayrı görünümlerdir; görünümleri değiştirmek bağlamaları korur. Arama tüm kontrol gruplarını kapsar. Grup seçici ok tuşlarını, Home ve End'i destekler. Girdi ve dil menüleri aramayı, ok tuşlarını ve Enter'ı destekler; Escape seçimi iptal eder. Menüler büyük ekranlarda alanlarının yanında, telefonlarda alt sayfa olarak açılır. Arayüz sistem yazı tiplerini kullanır ve azaltılmış hareket tercihlerine uyar.

Çeviri düğmesi, 22 arayüz ve SSS dilinden birini seçer. İlk ziyaretler İngilizce kullanır; kaydedilmiş bir dil seçimi önceliklidir. Mısır Arapçası dil listesinde ilk sırada görünür. Arapça, Urduca ve Farsça sağdan sola yerleşimleri kullanır. Yalnızca dil tercihi kalıcıdır; yeniden yüklemeden önce girdi değişikliklerini kopyalayın ya da indirin.

Klavye yakalama, fiziksel tarayıcı tuş kodlarını SDL scancode adlarına dönüştürür. Escape yakalamayı iptal eder; atamak için Escape'i elle seçin. Birleştirme (composition), belirsiz tuşlar ve bazı tarayıcı ya da sistem kısayolları elle seçim gerektirir. `#` veya `;` içeren adlar dışa aktarılamaz, çünkü bu karakterler INI yorumlarını başlatır. Tuş kataloğu SDL commit `4b69833bc54abf3dd3288d4aa7afbba527775e5b`'yi izler. Kaynak-sözleşme testleri eylemleri, kısıtlamaları ve varsayılanları geçerli yerel girdi koduyla karşılaştırır.

Statik barındırma için `index.html`, `styles.css`, `app.js`, `mapping.js`, `keys.js`, `capture.js`, `validation.js`, `i18n.js` ve `dropdown.js` dosyalarını tek bir dizinden sunun. Yapılandırıcı henüz projenin GitHub Pages sitesinde yayımlanmamıştır.

## Bir kumanda olmadan zaman çizelgesini denetleme

- `tools/preview_pad_timeline.py <timeline> [--repeat N] [--gap-ms MS] [--json]`, bir kumanda zaman çizelgesini doğrular ve bir kumanda komut dosyasına dokunmadan önizler. `--json` önizlemeyi JSON olarak yazdırır.
- `tools/preview_pad_input.py <path> <button> [--hold-ms MS] [--left-stick X Y] [--right-stick X Y] [--touch X Y ID ACTIVE]`, `<path>` konumundaki komut dosyasını değiştirmeden tek bir kumanda darbesini yazdırır.
