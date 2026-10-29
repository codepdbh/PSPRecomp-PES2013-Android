# PES 2013 para Android: investigación inicial

Perfil independiente para **Pro Evolution Soccer 2013 de PSP, ULUS10597**.
El objetivo es reutilizar el runtime Allegrex y los servicios portables del
proyecto, con un host propio que no instale parches de direcciones de GTA.

**Todavía no es una aplicación jugable ni produce un APK.** Hay una prueba
básica del ELF (`pes2013_probe`) y otra con servicios PSP compartidos
(`pes2013_host_probe`). Esta última usa el planificador y el host existentes,
con los parches de direcciones de GTA desactivados para PES, sin ventana ni audio.

## Lo comprobado con la copia local

- ISO identificada por `PSP_GAME/PARAM.SFO`: `Pro Evolution Soccer 2013`, `ULUS10597`.
- Ejecutable descifrado obtenido mediante el PPSSPP local: ELF MIPS de 32 bits.
- SHA-256 esperado: `5ab84864354b3ee32cf4dbb1a7e0bb0fc914d63b0f25cddfa387af35e09a14db`.
- Entrada: `0x08A1B910`; GP: `0x08AE7D10`. El nombre interno del módulo es
  `pes2010_psp`; proviene del ejecutable de esta misma ISO de PES 2013.
- 300 importaciones PSP y 12.870 semillas de funciones tras corregir el análisis
  de tablas de saltos y secciones dentro de segmentos RWX.
- Generación genérica actual del EBOOT: 161 unidades C++, 473.415 instrucciones
  candidatas y 138.353 entradas de bloques. El análisis todavía informa 82
  ocurrencias de instrucciones no soportadas; requieren revisión y no se dan por ejecutadas.
- La prueba inicial del EBOOT, sin servicios compartidos, en Windows llega a
  `SysMemUserForUser::0x358CA1BB`, donde se detiene intencionalmente.
- La misma prueba compila para Android ARM64 y se ejecutó en el S25 Ultra
  (`SM-S938B`): registra 127.183 entradas AOT y llega al mismo servicio pendiente
  en `0x08A85DFC`. Esto valida el comienzo de la ejecución nativa, no el menú.
  Esta fue la compilación inicial, antes de la corrección de secciones. Ese NID
  corresponde a `sceKernelSetCompiledSdkVersion606`, según la
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
al EBOOT. Se tradujeron experimentalmente los 28 overlays con texto no vacío:
576 unidades C++ en espacios de nombres separados. Los 604 archivos resultantes,
incluidos sus registros, pasaron la comprobación de sintaxis ARM64. Todavía no
se han enlazado ni ejecutado como módulos del juego.

La auditoría estática encuentra 187 importaciones con registro literal en el host
actual, 33 que requieren revisar alias o registros mediante bucles y 80 sin
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

Para crear contenedores ELF de análisis y traducir los módulos con código:

```powershell
python profiles/pes2013/tools/prepare_overlay_elf.py
python profiles/pes2013/tools/translate_overlays.py --all
python profiles/pes2013/tools/check_overlay_syntax.py --ndk "$env:ANDROID_HOME/ndk/28.2.13676358" --jobs 2
```

La base de cada módulo se contrasta con la tabla de secciones del EBOOT.
Los contenedores mantienen separados cabecera MWo3, texto, datos y BSS.
Los resultados quedan en `analysis/overlay_elf` y `analysis/overlay_codegen`.
El modelo de carga sigue siendo experimental; no sustituye la validación del
cargador real ni activa automáticamente esos registros AOT durante el juego.

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

## Prueba con servicios PSP compartidos

En Android, el objetivo `pes2013_host_probe` enlaza el corpus del EBOOT y los
servicios compartidos, con `PSPRECOMP_TITLE_PES2013` y los parches GTA desactivados.
El proceso usa una configuración propia en `probe-state/`, sin ventana ni sonido,
y tiene un límite de 20 segundos para evitar que un bucle de arranque quede activo.

```powershell
cmake --build out/pes2013-android-arm64 --target pes2013_host_probe --parallel 4
```

También puede ejecutarse esa variante en Windows para continuar el diagnóstico
cuando el teléfono no esté conectado. Requiere un SDK local FFmpeg 7 con sus
cabeceras, bibliotecas de importación y DLL; esos binarios no se distribuyen aquí:

```powershell
cmake -S . -B out/pes2013-probe -DPSPRECOMP_PROFILE=pes2013 -DPSPRECOMP_PES2013_WINDOWS_HOST_PROBE=ON -DPSPRECOMP_PES2013_FFMPEG_ROOT=C:/ffmpeg7
cmake --build out/pes2013-probe --config Release --target pes2013_host_probe --parallel 1
./out/pes2013-probe/profiles/pes2013/Release/pes2013_host_probe.exe profiles/pes2013/game
```

Esta variante pasó la configuración del SDK, creó y arrancó el primer hilo,
registró el callback de salida y consultó parámetros del sistema. Se añadieron
operaciones reales de suspensión/restauración de interrupciones, copia y relleno
de memoria, y configuración/consulta de los relojes solicitados por el juego.
El modelo de relojes guarda y valida las peticiones; no controla la frecuencia
física del teléfono ni reproduce aún todos los tiempos del hardware PSP.

Las pruebas `pes2013_kernel_tests` comprueban suspensión anidada, restauración,
límites de memoria, alias de VRAM, y peticiones válidas e inválidas de frecuencia.
El framework también tiene una regresión sintética de tablas de saltos en `.rodata`
dentro de un segmento RWX, el caso que bloqueaba PES en `0x08804920`.

La prueba Windows con el EBOOT regenerado registra 138.368 entradas AOT y
alcanza 38 servicios distintos (58 llamadas), incluida la inicialización de
pantalla y GE. El estado UMD del disco montado permite superar la espera inicial:
la traza confirma que se ejecuta el callback del juego en `0x0880B9B8` y se
restaura su contexto para continuar en `0x0880B9A0`.
Su punto de detención actual es `sceImpose::sceImposeSetLanguageMode`
(`0x36AA6E91`, PC `0x08A8643C`). Todavía no se alcanza el menú.

`pes2013_umd_tests` comprueba presencia del medio, máscaras de espera,
notificaciones y cesión del contexto al callback. El modelo se limita a disco
fijo: no reproduce cambios de medio ni temporizadores de esperas pendientes.
Si una condición no se cumple, detiene el diagnóstico en vez de declarar éxito.
Estos cambios se comprobaron en PC; las pruebas en el S25 quedan pendientes.

Referencias de nombres y NIDs para las implementaciones independientes:
[Kernel/Interrupts](https://github.com/hrydgard/ppsspp/blob/master/Core/HLE/sceKernelInterrupt.cpp),
[Power](https://github.com/hrydgard/ppsspp/blob/master/Core/HLE/scePower.cpp),
[UMD API](https://github.com/pspdev/pspsdk/blob/master/src/umd/pspumd.h),
[UMD y callbacks](https://github.com/hrydgard/ppsspp/blob/master/Core/HLE/sceUmd.cpp).

## Siguientes hitos

1. Confirmar el mapa y el cambio de overlays en el cargador del juego.
2. Traducir los overlays y enlazarlos con el registro AOT activo.
3. Integrar los servicios PSP compartidos sin aplicar parches de GTA.
4. Llegar al menú y a un partido con Vulkan, audio y controles en una app aparte.

Referencia de formato consultada para contrastar UTF/ITOC y CRILAYLA:
[CriPakTools](https://github.com/esperknight/CriPakTools/blob/master/CriPakTools/CPK.cs).
El extractor del perfil se implementó aquí; no se incorporó código de esa herramienta.
