# Estado del port: 29 de octubre de 2026

PES todavía no llega al menú y no hay APK jugable.

## Verificado

- Repositorio público independiente con comprobaciones automáticas en Windows y Linux.
- EBOOT `ULUS10597` traducido a 161 unidades C++.
- Analizador corregido para respetar secciones ejecutables dentro de segmentos
  RWX y descubrir destinos de tablas de saltos en secciones de datos.
- Regresión sintética del caso que faltaba en PES: destino `0x08804920`.
- Variante de arranque con servicios PSP compartidos y parches de GTA desactivados.
- Pruebas Windows y S25: hilos CRI de lectura/descompresión y audio iniciados;
  lectura de archivos CPK reales; 89 importaciones distintas alcanzadas.
- Configuración del SDK 6.06, suspensión/restauración de interrupciones, copia y
  relleno de memoria, alias del identificador de hilo, y estado de relojes PSP.
- Estado UMD para disco fijo: presencia, activación y entrega de notificaciones.
  La traza del juego confirma la ejecución del callback `0x0880B9B8` y el retorno
  al código que esperaba el disco en `0x0880B9A0`.
- Cinco suites locales pasan: framework/memoria, kernel/power/alarms, UMD,
  mutex ligeros y cambio de overlays con entradas sintéticas.
- Mutex ligeros con propietario, recursión, cola por prioridad y borrado;
  todavía falta la espera contendida con temporizador.
- Alarmas con entrega al código invitado y reprogramación según su resultado.
- Ajuste y consulta del idioma y botón de confirmación de la interfaz PSP;
  pruebas de estado independiente por ejecución y límites de buffers de salida.
- Prueba repetida en S25 Ultra (`SM-S938B`): ejecuta `bootset.ovl` y
  `memorycard.ovl`, con cabecera y texto verificados contra los inputs AOT.
- Descubrimiento de punteros construidos a través de bifurcaciones y sus slots;
  referencias cruzadas desde los overlays a 3.462 entradas del EBOOT.
- App independiente `com.psprecomp.pes2013` instalada en el S25; inicializa Vulkan
  y AAudio, presenta controles y el indicador de carga inicial. No alcanza el menú.
- El ejecutable diagnóstico actual compila para Windows x64 y Android ARM64.
- 69 overlays extraídos y contrastados con las bases declaradas en el EBOOT:
  28 con texto no nulo y 41 con texto íntegramente cero.
- Traducción experimental de los 28 módulos a espacios de nombres separados:
  576 unidades de código y 28 registros; los 604 archivos pasan la comprobación
  de sintaxis con Clang del NDK para Android ARM64.

## Bloqueo observado

Después de ejecutar `bootset` y `memorycard`, el juego accede al scratchpad PSP
en `0x00010000`. Esa región no estaba modelada. Se añadió su memoria separada de
16 KB con pruebas de alias, límites, accesos AOT y escritura sin efectos parciales.
La recompilación del corpus con este soporte y su repetición en el S25 están en curso.
La espera UMD anterior ya se supera con el disco extraído presente y su callback.
El modelo UMD todavía no reproduce cambios de disco ni esperas asíncronas con
temporizador: una condición que no puede satisfacerse detiene el diagnóstico.

La primera app reveló una pila nativa insuficiente. La revisión con hilo de 16 MB
y profundidad de encadenado acotada ya supera ese cierre y llega al scratchpad.
La pantalla y los controles de la app no prueban todavía funcionamiento en un partido
ni permiten evaluar FPS jugables. Los APK siguen siendo artefactos locales.

## Trabajo que sigue

1. Validar el scratchpad dentro del arranque real y resolver el siguiente bloqueo.
2. Llegar al menú con los overlays bootset, memorycard, enter, title y defaultdataset.
3. Enlazar los módulos de selección y partido, validando su cambio en ejecución.
4. Revisar las instrucciones que fallen durante ejecución; los recuentos estáticos
   también pueden contener datos interpretados como código.
5. Llegar al menú y a un partido antes de evaluar gráficos, audio y rendimiento.

Los ejecutables, datos y registros de las pruebas son archivos locales ignorados
por Git. Los relojes implementados representan peticiones de la PSP; no cambian
la frecuencia física del teléfono ni garantizan temporización exacta.
