# App Android de desarrollo

La app todavía **no es jugable**. Inicializa Vulkan y AAudio y permite comprobar
el arranque con controles PSP en pantalla. Los APK no se publican en Releases.

Prepara primero el EBOOT y los overlays según el perfil de PES. Para enlazar los
módulos iniciales y empaquetar en Windows:

```powershell
python profiles/pes2013/tools/translate_overlays.py --names bootset title memorycard enter defaultdataset select select1 formation game result league_prog
./profiles/pes2013/tools/build_apk.ps1
```

Requiere Android SDK, NDK `28.2.13676358`, CMake `3.30.5` y el JDK de Android
Studio. El script compila la biblioteca nativa y usa el wrapper Gradle para
generar `android/app/build/outputs/apk/debug/app-debug.apk`.

El paquete es `com.psprecomp.pes2013`, independiente de VCS y LCS. Los datos
propios deben estar en el directorio de la app:

`Android/data/com.psprecomp.pes2013/files/game/PSP_GAME/`

Incluye el ELF descifrado comprobado por el perfil en
`PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF` y conserva la estructura de USRDIR.
No se solicitan permisos de acceso a todos los archivos. La configuración y
los datos de esta app no alteran las instalaciones de VCS/LCS.

El corpus actual se compila sin optimización para diagnosticar el arranque;
sus tiempos no representan el rendimiento final del port.
