#!/bin/bash
scp -prBC $@ "pi@rpi3:ffmpeg/" &
scp -prBC $@ "pi@rpi0:ffmpeg/" &
scp -prBC $@ "pi@rpi:ffmpeg/" &
scp -prBC $@ "np@np:ffmpeg/" &
wait
