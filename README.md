Proof of Bubbles
================

This is a clone of Bust-A-Move for the Sega Master System.
This is an early proof-of-concept, with the possibility of
developing into a fully fledged game in the future.

* Fonts, and the 10 level layouts come from Bust-A-Move 2 Arcade Edition
* Bubble sprites and grass come from Alex Kidd in Miracle World


## How to Play

Launch bubbles into the game board. Completing a group of 3 or more bubbles of
the same colour will cause them to pop. The goal is to clear the game board to
move on to the next level.

* Left/Right: Aims the bubble launcher towards the left or right
* Up: Aim the bubble launcher towards the centre
* Down: Aim the bubble launcher away from the centre
* 1: Launch a bubble
* 2: While held, directions will single-step the aim per press


## To-Dos

* Background music
* Original level designs
* Arrow for the bubble launcher
* Push-down, based on shots taken and colours remaining
* Auto-fire countdown
* Win/Lose screens after each round
* Soft-landing for the bubbles
* Popping animation
* Colourblind mode / sprites (eg. unique shapes for each colour bubble)
* Support for PAL timing
* Optimize code - Currently there are moments, such as when animating falling bubbles, where the code hasn't finished running by the time the active-area starts to draw. 
* Different themes / backgrounds for levels.
* Star-bubbles that pop all of a colour


## Tools & Libraries used
* [SDCC 4.3](https://sdcc.sourceforge.net/) <br /> Small Devices C Compiler
* [devkitSMS](https://github.com/sverx/devkitSMS) <br /> Tools & libraries for Sega Master System development
* [Sneptile](https://github.com/JoppyFurr/Sneptile) <br /> Convert .png images into VDP patterns
