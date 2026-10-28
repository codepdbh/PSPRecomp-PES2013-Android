# PES 2013 para Android: investigación inicial

Perfil independiente para **Pro Evolution Soccer 2013 de PSP, ULUS10597**.
El objetivo es reutilizar el runtime Allegrex y los servicios portables del
proyecto, con un host propio que no instale parches de direcciones de GTA.

**Todavía no es una aplicación jugable ni produce un APK.** La prueba actual
ejecuta el arranque traducido y se detiene al pedir un servicio PSP sin implementar.
No incluye un renderizador, audio ni un planificador de hilos.

## Lo comprobado con la copia local

- ISO identificada por `PSP_GAME/PARAM.SFO`: `Pro Evolution Soccer 2013`, `ULUS10597`.
- Ejecutable descifrado obtenido mediante el PPSSPP local: ELF MIPS de 32 bits.
- SHA-256 esperado: `5ab84864354b3ee32cf4dbb1a7e0bb0fc914d63b0f25cddfa387af35e09a14db`.
- Entrada: `0x08A1B910`; GP: `0x08AE7D10`. El nombre interno del módulo es
  `pes2010_psp`; proviene del ejecutable de esta misma ISO de PES 2013.
- 300 importaciones PSP y 11.949 semillas de funciones en el análisis inicial.
- Generación genérica del EBOOT: 183 unidades C++, 475.419 instrucciones candidatas
  y 127.169 entradas de bloques. Estos números no prueban que todo sea código
  ejecutable: el análisis automático también puede alcanzar datos.
- El corpus del EBOOT compila y su prueba Windows llega a
  `SysMemUserForUser::0x358CA1BB`, donde se detiene intencionalmente.
- La misma prueba compila para Android ARM64 y se ejecutó en el S25 Ultra
  (`SM-S938B`): registra 127.183 entradas AOT y llega al mismo servicio pendiente
  en `0x08A85DFC`. Esto valida el comienzo de la ejecución nativa, no el menú.
  Ese NID corresponde a `sceKernelSetCompiledSdkVersion606`, según la
  [tabla de servicios de PPSSPP](https://github.com/hrydgard/ppsspp/blob/master/Core/HLE/sceKernelMemory.cpp).
- `OVER.cpk` contiene 69 módulos MWo3 `.ovl`: 65 comprimidos con CRILAYLA y
  cuatro sin esa compresión. Se extrajeron 12.652.032 bytes; tamaños, límites
  de código/datos y relleno de cada archivo pasan las comprobaciones del extractor.

## Diferencia importante respecto a los perfiles GTA

El código del EBOOT no contiene todos los modos del juego. Hay módulos separados
como `bootset.ovl`, `title.ovl`, `select.ovl`, `formation.ovl`, `game.ovl`,
`masterleague.ovl` y otros. Sus cabeceras indican regiones de carga compartidas:
varios módulos pueden ocupar las mismas direcciones en momentos distintos.

Por eso falta verificar el cargador MWo3, traducir los módulos y seleccionar
las funciones AOT según el módulo cargado. Registrar todos los módulos a la vez
por dirección mezclaría funciones distintas. El corpus actual solo corresponde
al EBOOT, y los overlays extraídos todavía no se han traducido.

La auditoría estática encuentra 182 importaciones con registro literal en el host
actual, 31 que requieren revisar alias o registros mediante bucles y 87 sin
registro encontrado. Esto no valida el comportamiento de los servicios existentes.
Entre las áreas pendientes están MP3, sincronización, memoria, utilidades y red.

## Preparar los archivos locales

Desde la raíz de PSPRecomp:

```powershell
./profiles/pes2013/tools/prepare.ps1
```

El script usa `../pes2013.iso` y el volcado local de PPSSPP
`../PPSSPP/memstick/PSP/SYSTEM/DUMP/ULUS10597_EBOOT.BIN` por defecto. Se pueden
pasar `-Iso`, `-DecryptedElf` y `-ToolDirectory`. Requiere Python 3, `psp_analyze`
y `vcs_recomp` ya compilados. El generador se ejecuta siempre con `--generic`
para desactivar los reemplazos específicos de VCS.

La opción de PPSSPP usada para producir el volcado es `DumpFileTypes=1` en su
configuración `[General]`. El script verifica la identidad de la ISO y el hash
del ELF antes de analizarlo o generar C++.

Para repetir únicamente el inventario y la extracción de overlays:

```powershell
python profiles/pes2013/tools/extract_overlays.py
python profiles/pes2013/tools/audit.py
```

Los datos, ELF, módulos descomprimidos y reportes viven en `game/` y `analysis/`,
ignorados por Git. El corpus experimental `generated/` también se genera
localmente y está ignorado. No se incluyen archivos comerciales en este perfil.

## Prueba de arranque en PC

```powershell
cmake -S . -B out/pes2013-probe -DPSPRECOMP_PROFILE=pes2013 -DPSPRECOMP_BUILD_TESTS=OFF -DPSPRECOMP_GENERATED_OPT_LEVEL=0 -DPSPRECOMP_MSVC_MP_JOBS=4
cmake --build out/pes2013-probe --config Release --target pes2013_probe --parallel 1
./out/pes2013-probe/profiles/pes2013/Release/pes2013_probe.exe profiles/pes2013/game
```

El código de salida `3` indica que terminó la prueba diagnóstica, **no** que el
juego haya arrancado correctamente. La prueba no sustituye las funciones PSP
pendientes por resultados de éxito ficticios. El nivel de optimización `0` es
para reducir el tiempo de compilación de esta prueba; no es una configuración
de rendimiento del futuro port.

El mismo perfil se puede compilar con el toolchain Android NDK para ARM64.
Sus resultados de compilación y ejecución se guardan localmente en `out/` y
`analysis/`.

```powershell
cmake -S . -B out/pes2013-android-arm64 -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$env:ANDROID_HOME/ndk/28.2.13676358/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release -DPSPRECOMP_PROFILE=pes2013 -DPSPRECOMP_BUILD_TESTS=OFF -DPSPRECOMP_GENERATED_OPT_LEVEL=0
cmake --build out/pes2013-android-arm64 --target pes2013_probe --parallel 2
```

Esta salida es un ejecutable de diagnóstico para ADB, no una aplicación Android.
En la prueba local se usó `/data/local/tmp/pes2013-probe/` para el binario y su
ELF de entrada. VCS y LCS no forman parte de esta prueba.

## Siguientes hitos

1. Confirmar el mapa y el cambio de overlays en el cargador del juego.
2. Traducir los overlays y enlazarlos con el registro AOT activo.
3. Integrar los servicios PSP compartidos sin aplicar parches de GTA.
4. Llegar al menú y a un partido con Vulkan, audio y controles en una app aparte.

Referencia de formato consultada para contrastar UTF/ITOC y CRILAYLA:
[CriPakTools](https://github.com/esperknight/CriPakTools/blob/master/CriPakTools/CPK.cs).
El extractor del perfil se implementó aquí; no se incorporó código de esa herramienta.
