
[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md) | [தமிழ்](README.ta.md)

# அறிமுகம்

Linux மற்றும் Windows இயங்குதளங்களுக்கான Executable கோப்புகளைத் தானாகவே மாற்றும் கருவி.

relinker-only build, Apple Silicon உட்பட macOS-லும் இயங்கும்.

[**Web Converter**](https://boykopovar.github.io/AnyPS5/) - உங்கள் கோப்புகள் உங்கள் சாதனத்தை விட்டு வெளியே செல்லாது.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

இந்தத் திட்டத்தில், Executable கோப்புகளை இலக்கு கணினியின் Native வடிவத்திற்கு மாற்றும் [relinker](../../core/relinker) மற்றும் Dynamic Linking-க்குத் தேவையான [System PRX Libraries](../../core/libs/prx)-இன் செயலாக்கங்கள் ஆகியவை இடம்பெற்றுள்ளன.

இது Emulation அல்லது தனியான Runtime Process இல்லாமல் நேரடியாகச் செயல்படுகிறது.

[**Discord**](https://discord.gg/x9naWVzC6A), [பயன்பாட்டு வழிகாட்டி](USAGE.md), [Build செய்வதற்கான வழிமுறைகள்](../dev/BUILD.md), [Architecture](../dev/ARCHITECTURE.md), [திட்டத்தின் Technical Debt](../dev/TechnicalDebt.md), [Code Style விதிமுறைகள்](../dev/CONVENTIONS.md), [பங்களிப்பு வழிகாட்டி](../../CONTRIBUTING.md)

## திட்டத்தின் தற்போதைய நிலை

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![RDNA2 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![gfx1013 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders-gfx1013.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[முன்னேற்றத்தின் வரலாறு](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* System Libraries முன்னேற்றம் என்பது, இதுவரை திட்டத்தில் அறிவிக்கப்பட்டுள்ள செயல்பாடுகளில் ([core/libs/prx](../../core/libs/prx)) எத்தனை சதவீதம் செயல்படுத்தப்பட்டுள்ளன என்பதைக் குறிக்கிறது. இது PS5-இன் அனைத்து System Functions-ஐயும் அடிப்படையாகக் கொண்ட கணக்கீடு அல்ல. புதிதாகச் செயல்பாடுகள் அறிவிக்கப்படும்போது மொத்த எண்ணிக்கையும் அதிகரிக்கும்.</sub>

ஆதரிக்கப்படாத அல்லது எதிர்பாராத நிலைகள் ஏற்பட்டால், C++ Exceptions (பொதுவாக `std::runtime_error`) எழுப்பப்படும். அவற்றைப் பிடிக்கக்கூடிய Exception Handler இல்லையெனில், அந்த Exception-இன் `what()` செய்தி stderr-இல் அச்சிடப்பட்டு, Program நிறுத்தப்படும்.

[Shader Recompiler](../../core/shader/recompiler/Recompiler.cpp), SPIR-V-ஐ வெற்றிகரமாக உருவாக்குகிறது. `ANYPS5_ENABLE_SPIRV_TOOLS` உடன் Build செய்யும்போது, [SPIRV-Tools](../../3rdparty/SPIRV-Tools) பயன்படுத்தி உருவாக்கப்பட்ட SPIR-V சரிபார்க்கப்படுகிறது.

## Compatibility (இணக்கத்தன்மை)

ஒவ்வொரு Game-இன் தற்போதைய நிலை மற்றும் Testing Reports ஆகியவற்றை [Game Bring-up Discussions](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up) பகுதியில் காணலாம்.

ஒவ்வொரு Game-க்கும் தனித்தனி Discussion Thread உள்ளது.

## Input Mapping (கட்டுப்பாடுகள் அமைப்பு)

SDL மூலம் அடையாளம் காணப்படும் Game Controllers ஆதரிக்கப்படுகின்றன. இதில் Analog Sticks மற்றும் Triggers ஆகியவையும் அடங்கும்.

Keyboard மற்றும் Mouse கட்டுப்பாடுகளை `anyps5-input.ini` கோப்பின் மூலம் தேவைக்கேற்ப அமைத்துக்கொள்ளலாம்.

ஆதரிக்கப்படும் சாதனங்கள் மற்றும் Configuration File-இன் வடிவம் பற்றிய கூடுதல் தகவல்களுக்கு [Input Mapping](INPUT_MAPPING.md) பகுதியைப் பார்க்கவும்.

## Disclaimer (பொறுப்புத் துறப்பு)

இந்தத் திட்டம் Interoperability, Research, Software Preservation மற்றும் Compatibility ஆகிய நோக்கங்களுக்காக உருவாக்கப்பட்டுள்ளது.

Copyright பாதுகாப்பு கொண்ட Software, Firmware, Cryptographic Keys அல்லது Proprietary Libraries ஆகியவற்றை இந்தத் திட்டம் உள்ளடக்கவோ, விநியோகிக்கவோ அல்லது அவற்றைச் சார்ந்திருக்கவோ இல்லை.

இந்தத் திட்டத்துடன் பயன்படுத்தப்படும் Binary Files, பொருந்தக்கூடிய சட்டங்கள் மற்றும் அவற்றின் உரிம விதிமுறைகளுக்கு இணங்கப் பெறப்பட்டு பயன்படுத்தப்படுவதை உறுதிசெய்வது பயனர்களின் பொறுப்பாகும்.

AnyPS5 என்பது ஒரு சுயாதீனமான திட்டமாகும். இது Sony Interactive Entertainment நிறுவனத்துடன் எந்தவிதத் தொடர்பும் கொண்டதல்ல. அந்நிறுவனத்தால் அங்கீகரிக்கப்படவோ அல்லது ஆதரிக்கப்படவோ இல்லை.

"PlayStation" மற்றும் "PS5" ஆகியவை Sony Interactive Entertainment Inc.-இன் வர்த்தக முத்திரைகளாகும்.

## License (உரிமம்)

இந்தத் திட்டம் GNU General Public License Version 2 Only (GPL-2.0-only) உரிமத்தின் கீழ் வெளியிடப்பட்டுள்ளது.
