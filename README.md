NOTE: please fill in the first section with information about your game.

# *Game Title*

Cube Volleyball [*Design Document*](http://graphics.cs.cmu.edu/courses/15-466-f17/game2-designs/jmccann/) for game2 in 15-466-f17.

![Screenshot](cube_volleyball.png)

## Asset Pipeline

I used the export-meshes.py in the models folder that generates the scene.blob and meshes.blob files that are loaded into the game. I decided to do vertex colors after completing the physics simulation, if there is time available.

## Architecture

I started by exporting the meshes from blender and then went on to loading them into the scene. I followed a parenting logic in the code. The sub objects in the crane are parented to the lower links. I assigned 4 axes variables, one to each of the links and on key press, used glm::angleAxis to rotate each axis with a fixed speed. Since the top links of the crane are parented to bottom ones, it takes care of moving them along with it.

I used standard sin functions to move the baloons up and down the scene. 

## Reflection

Since this was my first time working with open gl in a 3d environment and blender, I struggled a bit on this assignment. It took me a while to understand the scene.cpp files and how mvp matrix works, the scene heirarchy and working with quarternions. I was able to get the rotation and balloon motion working well within the scene with angleAxis and sine/cosine functions. 

However I got stuck with implementing the collisions for popping the balloon. I initially tried to code a hack by calculating the distance between the balloon and the needle. However, the transform.position property of the needle never changed. I realised late that I needed to use the localPosition of the needle to compute the transforms.
 


# About Base2

This game is based on Base2, starter code for game2 in the 15-466-f17 course. It was developed by Jim McCann, and is released into the public domain.

## Requirements

 - modern C++ compiler
 - glm
 - libSDL2
 - libpng
 - blender (for mesh export script)

On Linux or OSX these requirements should be available from your package manager without too much hassle.

## Building

This code has been set up to be built with [FT jam](https://www.freetype.org/jam/).

### Getting Jam

For more information on Jam, see the [Jam Documentation](https://www.perforce.com/documentation/jam-documentation) page at Perforce, which includes both reference documentation and a getting started guide.

On unixish OSs, Jam is available from your package manager:
```
	brew install ftjam #on OSX
	apt get ftjam #on Debian-ish Linux
```

On Windows, you can get a binary [from sourceforge](https://sourceforge.net/projects/freetype/files/ftjam/2.5.2/ftjam-2.5.2-win32.zip/download),
and put it somewhere in your `%PATH%`.
(Possibly: also set the `JAM_TOOLSET` variable to `VISUALC`.)

### Bulding
Open a terminal (on windows, a Visual Studio Command Prompt), change to this directory, and type:
```
	jam
```

### Building (local libs)

Depending on your OSX, clone 
[kit-libs-linux](https://github.com/ixchow/kit-libs-linux),
[kit-libs-osx](https://github.com/ixchow/kit-libs-osx),
or [kit-libs-win](https://github.com/ixchow/kit-libs-win)
as a subdirectory of the current directory.

The Jamfile sets up library and header search paths such that local libraries will be preferred over system libraries.
