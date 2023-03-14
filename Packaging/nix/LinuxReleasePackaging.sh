mkdir ./build/package
find build/_CPack_Packages/Linux/7Z/ -name 'middleearthmod' -exec cp "{}" ./build/middleearthmod \;
cp ./build/middleearthmod ./build/package/middleearthmod
cp ./Packaging/resources/middleearthmod.mpq ./build/package/middleearthmod.mpq
cp ./build/middleearthmod*.deb ./build/package/middleearthmod.deb
cp ./build/middleearthmod*.rpm ./build/package/middleearthmod.rpm
cp ./Packaging/nix/README.txt ./build/package/README.txt
cp ./Packaging/resources/LICENSE.CC-BY.txt ./build/package/LICENSE.CC-BY.txt
cp ./Packaging/resources/LICENSE.OFL.txt ./build/package/LICENSE.OFL.txt
cd ./build/package/ && tar -cavf ../../middleearthmod.tar.xz * && cd ../../
