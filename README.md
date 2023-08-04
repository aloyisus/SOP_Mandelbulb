Houdini SOP for generating a Mandelbulb
=======================================

This is a really simple SOP built with the HDK for creating the Mandelbulb. I compiled against Houdini 19.0.702 on Windows, but it
should be possible to get it working using hcustom on linux without too much difficulty.

The Mandelbulb is a beautiful and fascinating 3D fractal object which is related to the Mandelbrot set. I cribbed the formula used
in this code from https://www.skytopia.com/project/fractal/2mandelbulb.html#formula.

The SOP generates a volume of size (64,64,64) by default which can be resized as required. It also supports transformation of the
co-ordinate system before it gets input to the Mandelbulb formula, which is a good way to zoom in on areas of interest without
dialing up the resolution to non-interactive levels.

I would recommend leaving the multi-threaded toggle ticked for better performance.

![screenshot](https://github.com/aloyisus/SOP_Mandelbulb/blob/master/Mandelbulb.jpg?raw=true)