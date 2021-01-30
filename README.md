# C++ Multiplayer Template in UE4
Content in this project may only be used in Unreal Engine projects as per the Unreal Engine EULA
Originally based on a Udemy course by Tom Looman located here: https://www.udemy.com/course/unrealengine-cpp/

Purpose: To provide a quick template when creating a new multiplayer game.

Current features: A functioning and replicating(mostly) health system, projectile/hitscan/melee(with combo) weapons and animations, horde mode multiplayer game mode.
Roadmap features: In no particular order, Deathmatch/Battle Royal/MOBA/Capture The flag game modes. Title screen with a server selector. P2P or Server hosting option.

Getting started:

Step 1: Right-click the .uproject file and click "Generate visual studio project files".
Step 2: Open the newly created .sln file.
Step 3. click "Local Windows Debugger" or hit f6 to run.

Adding a weapon:

Step 1: Rick-click an empty spot in the content browser. Select "Blueprint Class" search for "SWeapon" and select it and name it.
Step 2: Double-click the newly created weapon blueprint and in the "Class Defaults" tab search for "Weapon".
Step 3: Configure the default settings, such as Type Of Weapon, Impact Effect, Muzzle Effect, etc.
Note: Be sure that you attach a socket to the skeletal mesh for the weapon AND the muzzle effect if applicable. A great tutorial on sockets can be found here: https://www.youtube.com/watch?v=DyPq1-JGMKY
Also, if using a projectile type of weapon be sure to create a projectile and then select it in the "Weapon" settings.

Adding a projectile:
Step 1: Rick-click an empty spot in the content browser. Select "Blueprint Class" search for "Projectile" and select it and name it.
Step 2: Double-click the newly created weapon blueprint and in the "Class Defaults" tab search for "Projectile".
Step 3: Select the "Projectile Type" and radious if it's AOE. I.E. A grenade or rocket.

Adding a player:

Step 1: Rick-click an empty spot in the content browser. Search for "SCharacter" and select it and name it.
Step 2: Double-click the newly created weapon blueprint and in the "Class Defaults" tab search for "Player".
Step 3: Set default values here, including the weapon you want to spawn with.
Note: Set animations using the mesh component.