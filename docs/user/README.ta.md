[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md) | **தமிழ்**

# அறிமுகம்

இயங்கு கோப்புகளை (executables) Linux மற்றும் Windows-க்குத் தானாக மாற்றும் கருவி.

relinker-only build, Apple silicon உட்பட macOS-இலும் இயங்கும்.

[**Web converter**](https://boykopovar.github.io/AnyPS5/) - கோப்புகள் உங்கள் சாதனத்திலேயே இருக்கும்.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

இதில் இயங்கு கோப்பை இலக்கு கணினியின் சொந்த வடிவத்துக்கு மாற்றும் [relinker](../../core/relinker), dynamic linking-க்கு ஏற்ற [system prx libraries](../../core/libs/prx)-இன் செயலாக்கங்கள் ஆகியவை உள்ளன. Emulation இல்லை; தனி runtime process-உம் இல்லை.

[**Discord**](https://discord.gg/x9naWVzC6A), [பயன்பாடு](USAGE.md), [Build வழிமுறைகள்](../dev/BUILD.md), [கட்டமைப்பு](../dev/ARCHITECTURE.md), [திட்டத்தின் technical debt](../dev/TechnicalDebt.md), [code style விதிகள்](../dev/CONVENTIONS.md), [பங்களிப்பு](../../CONTRIBUTING.md)

## நிலை

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![RDNA2 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![gfx1013 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders-gfx1013.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[முன்னேற்ற வரலாறு](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* System libraries: திட்டத்துக்கு இதுவரை தெரிந்த செயல்பாடுகளில் ([core/libs/prx](../../core/libs/prx)-இல் அறிவிக்கப்பட்டவை) எத்தனை சதவீதம் என்பதே இது; PS5-இன் ஒவ்வொரு system function-இன் அடிப்படையில் அல்ல. மேலும் செயல்பாடுகள் அறிவிக்கப்படும்போது மொத்த எண்ணிக்கை அதிகரிக்கும்.</sub>

ஆதரிக்கப்படாத அல்லது எதிர்பாராத நிலைகள் C++ exception-ஐ, பொதுவாக `std::runtime_error`-ஐ, எறியும். எதுவும் பிடிக்காத exception தனது `what()` செய்தியை stderr-க்கு அச்சிட்டு process-ஐ முடித்துவிடும்.

[Shader recompiler](../../core/shader/recompiler/Recompiler.cpp) SPIR-V-ஐ வெற்றிகரமாக உருவாக்குகிறது (`ANYPS5_ENABLE_SPIRV_TOOLS` உடன் build செய்யும்போது [Spirv-Tools](../../3rdparty/SPIRV-Tools) மூலம் சரிபார்க்கப்படுகிறது).

## இணக்கத்தன்மை

ஒவ்வொரு game-இன் நிலையும் சோதனை அறிக்கைகளும் [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up) discussions-இல் உள்ளன; ஒரு title-க்கு ஒரு thread.

## உள்ளீட்டு இணைப்பு (Input mapping)

SDL-mapped game controllers ஆதரிக்கப்படுகின்றன; analog sticks மற்றும் triggers உட்பட. விசைப்பலகை மற்றும் சுட்டிக் கட்டுப்பாடுகளை `anyps5-input.ini` கோப்பு மூலம் அமைக்கலாம். ஆதரிக்கப்படும் சாதனங்கள் மற்றும் அமைப்பு வடிவம் பற்றி [input mapping](INPUT_MAPPING.md) பகுதியைப் பார்க்கவும்.

## மறுப்பு (Disclaimer)

இந்தத் திட்டம் interoperability, ஆராய்ச்சி, பாதுகாப்பு (preservation), இணக்கத்தன்மை ஆகிய நோக்கங்களுக்காக உருவாக்கப்பட்டது. பதிப்புரிமை பெற்ற மென்பொருள், firmware, குறியாக்க விசைகள் (cryptographic keys), தனியுரிம நூலகங்கள் ஆகியவற்றை இது உள்ளடக்கவோ, விநியோகிக்கவோ, தேவைப்படுத்தவோ இல்லை. இந்தத் திட்டத்துடன் பயன்படுத்தப்படும் binaries, பொருந்தக்கூடிய சட்டங்களுக்கும் அவற்றின் உரிம விதிமுறைகளுக்கும் இணங்கப் பெறப்பட்டு பயன்படுத்தப்படுவதை உறுதிசெய்வது பயனர்களின் பொறுப்பு.

AnyPS5 ஒரு சுயாதீனத் திட்டம்; Sony Interactive Entertainment-உடன் தொடர்புடையது அல்ல, அவர்களால் அங்கீகரிக்கப்படவோ ஆதரிக்கப்படவோ இல்லை. "PlayStation", "PS5" ஆகியவை Sony Interactive Entertainment Inc.-இன் வர்த்தக முத்திரைகள்.

## உரிமம்

இந்தத் திட்டம் GNU General Public License பதிப்பு 2 மட்டும் (version 2 only) என்ற உரிமத்தின் கீழ் வெளியிடப்பட்டுள்ளது.
