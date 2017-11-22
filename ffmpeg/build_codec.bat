@rem codec.dll
cl /Ox /LD codec.c /D WIN32 /I "D:\Programs\misc\Multimedia\ffmpeg-20170824-f0f4888-win64\include" /I "..\external\pthread-w32\include" /I "..\external\fmod\inc" /D HAVE_STRUCT_TIMESPEC /D ENABLE_FMOD /link /LIBPATH:"D:\Programs\misc\Multimedia\ffmpeg-20170824-f0f4888-win64\lib" avcodec.lib avformat.lib avutil.lib swscale.lib avfilter.lib /LIBPATH:"..\external\fmod\lib" fmod64_vc.lib /LIBPATH:"..\external\pthread-w32\lib\x64" pthreadVC2.lib

@rem codec.exe
csc /out:codec.exe /target:exe codec.cs
