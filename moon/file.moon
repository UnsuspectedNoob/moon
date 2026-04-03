let prompt be "Welcome to the MOON console. How old are you? "

## 1. 'ask' is triggered (routed via core.h to __ask)
## 2. The string is captured
## 3. 'as' safely casts it to a Number (Hard Crashing if invalid)
let age be (ask prompt) as Number

show "In 10 years, you will be `age + 10`"
