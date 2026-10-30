# Estado del port: 29 de octubre de 2026

## Rendimiento: 30 de octubre de 2026

- En el S25, la pantalla de creación de la tarjeta ya se ve con Vulkan. El
  teclado de Android aceptó un nombre de prueba y el juego avanzó hasta pedir
  nacionalidad y el editor de rostro. El módulo `edit.ovl` se ejecutó en el S25.
  Se detectó parpadeo con fotogramas negros en el editor; todavía no hay una
  prueba completa de partido.
- Android conserva el último fotograma de Vulkan sin volver a copiarlo a la
  ventana cuando no hay una imagen nueva. Los controles usan un bloqueo propio.
- La selección de superficies Vulkan cuenta los lotes por destino y evita
  compararlos todos entre sí. La resolución de overlays consulta primero los
  módulos activos, conservando la comprobación de su cabecera.
- Las siete pruebas portables pasan y la biblioteca Android ARM64 compila con
  optimización. La nueva versión aún no se ha medido en el S25 durante un partido.
- Vulkan sigue usando una lectura de la imagen de GPU a CPU para presentarla en
  Android. La presentación directa mediante swapchain sigue pendiente.

PES alcanza su pantalla de titulo en S25; aun no hay un partido jugable verificado.

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

## Estado del arranque actual

El scratchpad, las operaciones Allegrex MADD/MSUB, DMA, las esperas de mutex
con temporizador y las consultas de hilos/eventos ya permiten ejecutar también
defaultdataset y title. El logo KONAMI se verificó visualmente en el S25.
Se añadió decodificación MP3 con minimp3 (CC0), probada con un tono original
sin datos comerciales; las seis suites portables pasan en Windows.

La introducción quedaba detenida con el búfer MPEG lleno: el host despachaba
únicamente el submanejador VBlank 15 usado por VCS. PES registra otro número.
Ahora se despachan todos los manejadores VBlank habilitados en orden. La prueba
PC ya consume el búfer, llega al final del archivo y sale de la decodificación.
La revisión Android está instalada: muestra la pantalla de título de PES 2013
y al pulsar avanzar intenta ejecutar select1.ovl. El resolver detuvo ese
salto porque faltaba enlazarlo. Se están compilando los módulos de selección,
formación, partido y resultados. Todavía no se ha verificado un partido jugable.

Los APK siguen siendo artefactos locales. Las métricas del arranque no
representan el rendimiento de un partido.

## Trabajo que sigue

1. Verificar la salida de la introducción en PC y S25 y resolver el siguiente bloqueo.
2. Llegar al menú con los overlays bootset, memorycard, enter, title y defaultdataset.
3. Enlazar los módulos de selección y partido, validando su cambio en ejecución.
4. Revisar las instrucciones que fallen durante ejecución; los recuentos estáticos
   también pueden contener datos interpretados como código.
5. Llegar al menú y a un partido antes de evaluar gráficos, audio y rendimiento.

Los ejecutables, datos y registros de las pruebas son archivos locales ignorados
por Git. Los relojes implementados representan peticiones de la PSP; no cambian
la frecuencia física del teléfono ni garantizan temporización exacta.
