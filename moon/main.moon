
## main.moon - The Entry Script

## 1. The Module Imports
load "math.moon"
load "player.moon"

## THE SHIELD TEST: If we load this again, the VM should instantly skip it 
## without getting trapped in an infinite loop!
load "math.moon"

show "--- MODULE SYSTEM ONLINE ---"

## 2. Testing the Math Routing
let highest be max of 25 and 80
show "The highest number is: " + highest
show "12 squared is: " + square of 12
show ""

## 3. Testing the Cross-File Type Engine
let hero be Player { name: "Munachi", health: 15 }

show "Hero spawned! Current health: " + hero's health
heal hero by 40
take 10 damage on hero

show "Final health: " + hero's health
