# Triangle-Based Rendering
It is an implementation of an efficient method to render 3D scenes with many lights. This method reduces lighting calculation by culling lights dynamically. It is similar to tile-based rendering, but there are two main modifications: a new design to accumulate lights and use a triangle-based culling method.

This project is implemented via DirectX 12 graphics API, and it uses some new DirectX 12 features. To run the executable, it requires the graphics hardware that supports DirectX 12.
### Dependencies
* Visual Studio 2015 (https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx)
* Windows 10 SDK (https://dev.windows.com/en-us/downloads/windows-10-sdk)
* FBX SDK 2016.1.2 VS2015 (http://www.autodesk.com/products/fbx/overview)

### Interactions
Mouse/Keyboard:
- Mouse : Rotate camera (left click on press)
- W : Move camera front
- S : Move camera back
- A : Move camera left
- D : Move camera right
- 1 : Switch to **Sponz** model
- 2 : Switch to **conference** model
- 3 : Switch to **sibenik** model
- P : Visualization of the number of lights for each triangle
- L : Visualization of the positions of light sources
- R : Regenerate all light sources randomly
- Up Arrow : Add a new light source
- Down Arrow : Remove a light source
