# Drumkit Cloner (v0.4)

Plugin de bateria hecho con JUCE: VST3 + Standalone (+ AU en Mac).

## Que hace

**Clonar kits** - Arrastra una cancion o un loop al panel SOURCE (o BROWSE). Detecta cada golpe, lo
clasifica (kick, snare, clap, hat, open hat, tom, perc, cymbal), agrupa los que suenan igual y aisla un
one-shot de cada grupo (mediana de todos los golpes). SENSITIVITY + RE-CLONE repiten el analisis.

**Editor visual del sample** - La onda gris es el original; la de color es como queda de verdad despues de
cortar, fades, COMP y ganancia. Arrastra las asas START / END para cortar. El umbral del compresor se
dibuja con lineas punteadas y los fades como rampas.

**Corrector de pitch** - DETECT, TUNE (a la nota que elijas), SNAP (a la nota mas cercana) y TUNE ALL.

**Patron MIDI** - EDIT abre la rejilla (32 pasos, 2 compases) con el nombre de cada instrumento a la izquierda.
- click: pone/quita nota - arrastrar: pinta - click derecho: cambia la fuerza - shift+click: roll (2, 3 o 4 golpes por paso)
- GENERATE crea un patron completo en el estilo elegido: Trap, Boom Bap, Hip Hop, Rap o Drill (INTENSITY = cuanta variacion).
- HI-HATS regenera solo los hi-hats (con rolls estilo trap y open hats) sin tocar kick ni snare.
- HUMANIZE: TIMING (hasta +-25 ms), VELO (+-30) y SWING.
- SAVE .MID guarda el MIDI con el nombre de cada instrumento dentro del archivo ("C1 = KICK"),
  y ARRASTRA MIDI AL DAW lo suelta en tu proyecto. Incluye swing y humanizacion.
- SAVE .WAV renderiza el loop de audio con todos los ajustes de cada pad. La cola se pliega al inicio para que el loop no corte.

**Estetica** - Pads que se iluminan al sonar (tambien desde el patron), colores por familia, knobs con brillo,
selector de color (Naranja, Cian, Verde, Magenta, Violeta, Amarillo).

## Subirlo a GitHub

Sube estos archivos sobrescribiendo los de tu repositorio y haz commit: GitHub Actions compila solo.
Despues baja el plugin de Actions > Artifacts.

## Compilar en tu PC

Necesitas Git, CMake y Visual Studio 2022 (Windows) o Xcode (Mac).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

El resultado queda en `build/DrumkitCloner_artefacts/Release/`.

## Instalar

- **Windows:** copia la carpeta `Drumkit Cloner.vst3` a `C:\Program Files\Common Files\VST3`.
- **Mac:** copia a `~/Library/Audio/Plug-Ins/VST3` (o `Components` para AU). Si macOS lo bloquea:
  `xattr -cr "Drumkit Cloner.vst3"`.

## Limitaciones

- La deteccion es solo por analisis de audio (sin IA): con voces o acordes marcados puede marcar golpes falsos.
- Los nombres de instrumentos se ven en la rejilla del plugin y quedan escritos como texto dentro del .mid.
  Que el piano roll de tu DAW los muestre depende del DAW (Ableton/FL no leen nombres de notas de un .mid).
- El patron tiene su propio PLAY y BPM; todavia no sigue el transporte del DAW.
- Antes de vender: cambia COMPANY_NAME y PRODUCT_NAME en `CMakeLists.txt` y revisa la licencia de JUCE.
