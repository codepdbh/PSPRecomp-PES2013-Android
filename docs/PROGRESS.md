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
  iniciales de pantalla y GE; 37 importaciones distintas alcanzadas, 57 llamadas.
- Configuración del SDK 6.06, suspensión/restauración de interrupciones, copia y
  relleno de memoria, alias del identificador de hilo, y estado de relojes PSP.
- Dos suites locales de pruebas pasan: framework y servicios kernel/power.
- El ejecutable diagnóstico actual compila para Windows x64 y Android ARM64.
- 69 overlays extraídos y contrastados con las bases declaradas en el EBOOT:
  28 con texto no nulo y 41 con texto íntegramente cero.
- Traducción experimental de los 28 módulos a espacios de nombres separados:
  576 unidades de código y 28 registros; los 604 archivos pasan la comprobación
  de sintaxis con Clang del NDK para Android ARM64.

## Bloqueo observado

El arranque Windows termina de forma diagnóstica en
`sceUmdWaitDriveStatCB` (`sceUmdUser`, NID `0x4A9E5E29`, PC `0x08A86094`).
La función necesita un modelo de estado del disco, espera y entrega de callbacks.
No se añadió una respuesta de éxito ficticia para ocultar este bloqueo.

La revisión ARM64 actual está compilada, pero pendiente de repetir en el S25;
ADB no detectaba el teléfono durante esta comprobación. La prueba anterior del
arranque básico sí se ejecutó en ese equipo.

## Trabajo que sigue

1. Modelar la espera UMD con callbacks y validar lectura de los archivos reales.
2. Verificar cómo el cargador MWo3 coloca y cambia los overlays en ejecución.
3. Enlazar la traducción de los overlays con el registro AOT activo. Su sintaxis
   comprobada no valida aún su selección, enlace ni ejecución dentro de PES.
4. Revisar las 82 ocurrencias de instrucciones no soportadas que todavía informa
   el análisis del EBOOT; no se asume que todas sean instrucciones reales ejecutadas.
5. Llegar al menú y a un partido antes de evaluar gráficos, audio y rendimiento.

Los ejecutables, datos y registros de las pruebas son archivos locales ignorados
por Git. Los relojes implementados representan peticiones de la PSP; no cambian
la frecuencia física del teléfono ni garantizan temporización exacta.
