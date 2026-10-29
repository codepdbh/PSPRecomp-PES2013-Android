# Estado del port: 29 de octubre de 2026

PES todavía no llega al menú y no hay APK jugable.

## Verificado

- Repositorio público independiente con comprobaciones automáticas en Windows y Linux.
- EBOOT `ULUS10597` traducido a 161 unidades C++.
- Analizador corregido para respetar secciones ejecutables dentro de segmentos
  RWX y descubrir destinos de tablas de saltos en secciones de datos.
- Regresión sintética del caso que faltaba en PES: destino `0x08804920`.
- Variante de arranque con servicios PSP compartidos y parches de GTA desactivados.
- Prueba Windows: primer hilo creado/arrancado, parámetros del sistema y servicios
  iniciales de pantalla y GE; 38 importaciones distintas alcanzadas, 58 llamadas.
- Configuración del SDK 6.06, suspensión/restauración de interrupciones, copia y
  relleno de memoria, alias del identificador de hilo, y estado de relojes PSP.
- Estado UMD para disco fijo: presencia, activación y entrega de notificaciones.
  La traza del juego confirma la ejecución del callback `0x0880B9B8` y el retorno
  al código que esperaba el disco en `0x0880B9A0`.
- Tres suites locales de pruebas pasan: framework, kernel/power y UMD.
- El ejecutable diagnóstico actual compila para Windows x64 y Android ARM64.
- 69 overlays extraídos y contrastados con las bases declaradas en el EBOOT:
  28 con texto no nulo y 41 con texto íntegramente cero.
- Traducción experimental de los 28 módulos a espacios de nombres separados:
  576 unidades de código y 28 registros; los 604 archivos pasan la comprobación
  de sintaxis con Clang del NDK para Android ARM64.

## Bloqueo observado

El arranque Windows termina de forma diagnóstica en
`sceImposeSetLanguageMode` (`sceImpose`, NID `0x36AA6E91`, PC `0x08A8643C`).
La espera UMD anterior ya se supera con el disco extraído presente y su callback.
El modelo UMD todavía no reproduce cambios de disco ni esperas asíncronas con
temporizador: una condición que no puede satisfacerse detiene el diagnóstico.

La base anterior compila para ARM64. Los cambios UMD de esta revisión se han
comprobado en PC; por indicación del usuario, las pruebas continúan allí.
La prueba anterior del arranque básico sí se ejecutó en el S25.

## Trabajo que sigue

1. Implementar los ajustes de sistema solicitados por PES y validar la lectura
   de los archivos reales después de la inicialización.
2. Verificar cómo el cargador MWo3 coloca y cambia los overlays en ejecución.
3. Enlazar la traducción de los overlays con el registro AOT activo. Su sintaxis
   comprobada no valida aún su selección, enlace ni ejecución dentro de PES.
4. Revisar las 82 ocurrencias de instrucciones no soportadas que todavía informa
   el análisis del EBOOT; no se asume que todas sean instrucciones reales ejecutadas.
5. Llegar al menú y a un partido antes de evaluar gráficos, audio y rendimiento.

Los ejecutables, datos y registros de las pruebas son archivos locales ignorados
por Git. Los relojes implementados representan peticiones de la PSP; no cambian
la frecuencia física del teléfono ni garantizan temporización exacta.
