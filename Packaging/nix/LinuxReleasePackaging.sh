mkdir ./build/package
find build/_CPack_Packages/Linux/7Z/ -name 'middleearthmod' -exec cp "{}" ./build/devilutionx-me \;
cp ./build/devilutionx-me ./build/package/devilutionx-me
cp ./Packaging/resources/devilutionx-me.mpq ./build/package/devilutionx-me.mpq
cp ./build/devilutionx*.deb ./build/package/devilutionx-me.deb
cp ./build/devilutionx*.rpm ./build/package/devilutionx-me.rpm
cp ./Packaging/nix/README.txt ./build/package/README.txt
cp ./Packaging/resources/LICENSE.CC-BY.txt ./build/package/LICENSE.CC-BY.txt
cp ./Packaging/resources/LICENSE.OFL.txt ./build/package/LICENSE.OFL.txt
cd ./build/package/ && tar -cavf ../../devilutionx-me.tar.xz * && cd ../../
