# canard

**canard** is a (work in progress) simple C static library that implements an internal command-line "console" within an application, in the classic style of id Software’s Quake and later games, which can hold configuration variables and provide hooks to commands.

## Initial features
* Application-defined commands and variables
* Callback functions
* Namespaces
* Saving and loading variables to/from a configuration file
* Command-line parsing (long options get converted into console commands)

## Planned future features
* Built-in Telnet server interface
* C++ bindings
