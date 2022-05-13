# DevilutionX
DevilutionX is a source port of Diablo and Hellfire that strives to make it simple to run the game while providing engine improvements, bugfixes, and some optional quality of life features.

# Links
Discord: https://discord.gg/YQKCAYQ
GitHub: https://github.com/diasurgical/devilutionX

Check out the manual for what features are available and how best to take advantage of them: https://github.com/diasurgical/devilutionX/wiki
For a full list of changes see our changelog: https://github.com/diasurgical/devilutionX/blob/master/docs/CHANGELOG.md

# How To Install:
 - Extract the files in the archive.
 - Install libsdl2
 - Copy DIABDAT.MPQ from the CD or GOG-installation (or extract it from the GoG installer) to the DevilutionX folder.
 - To run the Diablo: Hellfire expansion you will need to also copy hellfire.mpq, hfmonk.mpq, hfmusic.mpq, hfvoice.mpq.
 - For Chinese, Japanese, and Korean text support download https://github.com/diasurgical/devilutionx-assets/releases/download/v2/fonts.mpq and add it to the game folder.
 - For the Polish voice pack download https://github.com/diasurgical/devilutionx-assets/releases/download/v2/pl.mpq.
 - For the Russian voice pack download https://github.com/diasurgical/devilutionx-assets/releases/download/v2/ru.mpq.
 - Run ./devilutionx

# Raspberry Pi performance
 - This build is compiled for Raspbian Stretch
 - For the best experience set upscale=0 in diablo.ini and set the system resolution to 640x480
 - Alternately you can enable experimental GL-drivers via raspi-config for upscaling support

# Multiplayer
 - TCP/IP requires the host to expose port 6112.

All games are encrypted and password protected.

# Save Games and configurations
The configurations and save games are located in:
~/.local/share/diasurgical/devilution

# Credits
 - See list of contributors https://github.com/diasurgical/devilutionX/graphs/contributors

# Legal
This software is being released to the Public Domain. No assets of Diablo are being provided. You must own a copy of Diablo and have access to the assets beforehand in order to use this software.

Battle.net® - Copyright © 1996 Blizzard Entertainment, Inc. All rights reserved. Battle.net and Blizzard Entertainment are trademarks or registered trademarks of Blizzard Entertainment, Inc. in the U.S. and/or other countries.

Diablo® - Copyright © 1996 Blizzard Entertainment, Inc. All rights reserved. Diablo and Blizzard Entertainment are trademarks or registered trademarks of Blizzard Entertainment, Inc. in the U.S. and/or other countries.

This software is in no way associated with or endorsed by Blizzard Entertainment®.
