`tone.mp3` is an original synthetic 997 Hz test tone, generated for these tests.
It contains no game audio. Generated with FFmpeg/libmp3lame:

```text
ffmpeg -f lavfi -i sine=frequency=997:sample_rate=44100:duration=0.25 -ac 2 -c:a libmp3lame -b:a 128k -map_metadata -1 -write_xing 0 -id3v2_version 0 tone.mp3
```

The fixture is dedicated to the public domain under CC0 1.0 Universal.
