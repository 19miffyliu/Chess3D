# Description
- This is a 3D Chess game where players can connect with other players on the same network using IP address and port. It also allows multiple players to spectate the same game (the player must join before the game starts).
- This project is for educational use only. All BGMs, SFXs and image assets are from the internet.
- The font is provided by Squirrel Eiserloh, our professor.
- This project requires the Engine dll library to work, please see the instructions in "Engine" project before trying to open this project in Visual Studio.


# Build instructions (with solution files)
1. Open the "Chess3D.sln" solution in "Chess3D" folder using Visual Studio 2022.
2. Click "Build->Build Solution" on the top navigation bar.


# Run instructions
With exe file:
1. Double-click on the "Chess3D_Release_x64.exe" file.

With solution files:
1. Press "Control" + "F5" to run the solution without debugging.
3. Navigate to "Chess3D/Run" folder in file explorer.
4. Double-click on the "Chess3D_Release_x64.exe" file.


# Gameplay Description
Please see the "C34 SDST Chess Network Protocol.docx" file under Docs/ to see all available DevConsole commands.


# Gameplay Controls
Keyboard:
- Press "Space" or "N" in attract mode to start the game
- Press "ESC" in game to go back to attract mode/exit the program
- Move mouse to change the camera yaw and pitch
- Press "WASD" to move the camera horizontally
- Press "Q" or "E" to move the camera down or up
- Hold "Shift" to increase camera movement speed by 10x
- Press "F4" to toggle free-fly or auto camera
- Press "F1" to toggle debug render for Lights
- Press 0-9 or NUMPAD 0-9 to switch debug render for Chess Objects
- Press "R" to disable debug render for Chess Objects

# Debug Controls
Keyboard:
- Press ` to toggle Dev Console
- Press "P" to pause the game
- Press "O" to step single frame
- Hold "T" to enter slow motion mode


# Console Commands
- Type "clear" to clear all debug objects
- Type "toggle" to toggle visibility of debug objects

