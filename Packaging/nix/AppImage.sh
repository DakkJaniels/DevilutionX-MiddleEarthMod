make install -Cbuild DESTDIR=AppDir
mv build/AppDir/usr/share/diasurgical/devilutionx/devilutionx-me.mpq build/AppDir/usr/bin/devilutionx-me.mpq
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -N
chmod +x linuxdeploy-x86_64.AppImage
./linuxdeploy-x86_64.AppImage --appimage-extract-and-run --appdir=build/AppDir --custom-apprun=Packaging/nix/AppRun -d Packaging/nix/middleearthmod.desktop -o appimage
mv middleearthmod*.AppImage middleearthmod.appimage
