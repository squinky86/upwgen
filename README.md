# upwgen
Unspeakable Random Password Generator

When passwords don't need to be spoken, why not include characters that are difficult to communicate verbally? Increasing the keysize of passwords increases their security. The Unspeakable Password Generator will allow you to generate Passwords-that-shall-not-be-named!

Upwgen works by selecting unprintable characters (using latin tags), rare languages (considered "extinct"), diacritical marks that stack, and other obscure scripts like Braille to ensure the passwords are as "unspeakable" as possible.

Including these characters in the keyspace means that the passwords can only be communicated digitally, and it encourages users to change their passwords quickly.

## Usage
`upwgen [-scldnexgmoh] [password size] [password quantity]`

### Options
*   `-s`: Disable symbols
*   `-c`: Disable English capital letters
*   `-l`: Disable English lowercase letters
*   `-d`: Disable digits/numbers
*   `-n`: Disable invisible characters (Latin tags)
*   `-e`: Disable emojis
*   `-x`: Disable extinct language characters (Gothic, Ugaritic, Cuneiform, Hieroglyphics, Runic, Ogham, Linear B, etc.)
*   `-g`: Disable gamepieces (Chess, Mahjong, Dominoes, Cards)
*   `-m`: Disable diacritical marks (Combining marks that stack on characters)
*   `-o`: Disable other languages/scripts (Braille)
*   `-h`: Show help

Default size: 15
Default quantity: 1
