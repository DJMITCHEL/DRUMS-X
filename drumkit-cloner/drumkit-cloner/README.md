# Drumkit Cloner (Fase 1: sampler de 64 pads)

Plugin de bateria hecho con JUCE: VST3 + Standalone (+ AU en Mac).

**Incluye ahora:** 64 pads en 4 bancos (A-D), MIDI desde C1 (nota 36), arrastrar samples a los
pads, y por pad: Gain, Pan, Tune, Attack, Fade, HP, LP (12 dB/oct), Choke, Mute, Solo y salida
separada (Main + 8 salidas estereo). Todo automatizable desde el DAW.

**Proximas fases:** deteccion de golpes, clasificacion, agrupacion, aislamiento (Clean), IA,
patron MIDI, skins.

## Subirlo a GitHub

```bash
cd drumkit-cloner
git init
git add .
git commit -m "Fase 1: sampler de 64 pads"
git branch -M main
git remote add origin https://github.com/TU_USUARIO/drumkit-cloner.git
git push -u origin main
```

(Crea antes el repositorio vacio en github.com -> New repository, sin README.)

## Compilar en la nube

Al hacer `push`, GitHub Actions compila para Windows y Mac. Ve a la pestana **Actions**,
abre la ultima ejecucion y descarga el plugin en **Artifacts**.

## Compilar en tu PC

Necesitas Git, CMake y Visual Studio 2022 (Windows) o Xcode (Mac).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

El resultado queda en `build/DrumkitCloner_artefacts/Release/`.

## Instalar

- **Windows:** copia la carpeta `Drumkit Cloner.vst3` a `C:\Program Files\Common Files\VST3`.
- **Mac:** copia a `~/Library/Audio/Plug-Ins/VST3` (o `Components` para AU). Si macOS lo bloquea
  por no estar firmado: `xattr -cr "Drumkit Cloner.vst3"`.

Luego reescanea plugins en tu DAW. Tambien puedes abrir la app Standalone para probar sin DAW.

## Antes de vender nada

- Cambia `COMPANY_NAME`, `PRODUCT_NAME` y los codigos en `CMakeLists.txt`.
- Revisa la licencia de JUCE en juce.com (gratis con limites de ingresos, AGPL o de pago).

## Limitaciones de la Fase 1

- El proyecto guarda la **ruta** de cada sample, no el audio; si mueves los archivos, no cargan
  (se arregla en la Fase 10).
- Un pad es monofonico (volver a tocarlo reinicia el sonido) y maximo 30 s por sample.
- Los buffers de samples se liberan al cerrar el plugin, no al reemplazarlos.
