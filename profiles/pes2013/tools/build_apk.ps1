param(
    [switch]$SkipNative,
    [int]$OptLevel = 2,
    [int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$sdkRoot = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { Join-Path $env:LOCALAPPDATA 'Android/Sdk' }
$cmake = Join-Path $sdkRoot 'cmake/3.30.5/bin/cmake.exe'
$ndk = Join-Path $sdkRoot 'ndk/28.2.13676358'
$build = Join-Path $repo 'out/pes2013-android-arm64'
if (-not $env:JAVA_HOME) {
    $jbr = 'C:/Program Files/Android/Android Studio/jbr'
    if (Test-Path (Join-Path $jbr 'bin/java.exe')) { $env:JAVA_HOME = $jbr }
}
$env:ANDROID_HOME = $sdkRoot
if (-not $SkipNative) {
    & $cmake -S $repo -B $build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" `
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_static `
        -DCMAKE_BUILD_TYPE=Release -DPSPRECOMP_PROFILE=pes2013 -DPSPRECOMP_BUILD_TESTS=OFF "-DPSPRECOMP_GENERATED_OPT_LEVEL=$OptLevel"
    if ($LASTEXITCODE -ne 0) { throw 'Android CMake configuration failed' }
    & $cmake --build $build --target pes2013_android --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Android native build failed' }
}
$destination = Join-Path $repo 'android/app/src/main/jniLibs/arm64-v8a'
New-Item -ItemType Directory -Force $destination | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'profiles/pes2013/libpes2013_android.so') -Destination $destination
Push-Location (Join-Path $repo 'android')
try {
    & ./gradlew.bat assembleDebug --no-daemon
    if ($LASTEXITCODE -ne 0) { throw 'Android APK packaging failed' }
} finally { Pop-Location }
Write-Output (Join-Path $repo 'android/app/build/outputs/apk/debug/app-debug.apk')
