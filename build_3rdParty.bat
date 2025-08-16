cmake -S 3rdParty/Sources/SDL -B 3rdParty/Sources/SDL/build -DSDL_STATIC=ON -DSDL_SHARED=OFF -DSDL_TESTS=OFF
cmake --build 3rdParty/Sources/SDL/build --config Debug
cmake --install 3rdParty/Sources/SDL/build --config Debug --prefix 3rdParty/SDL/Debug
cmake --build 3rdParty/Sources/SDL/build --config Release
cmake --install 3rdParty/Sources/SDL/build --config Release --prefix 3rdParty/SDL/Release
cmake --build 3rdParty/Sources/SDL/build --config RelWithDebInfo
cmake --install 3rdParty/Sources/SDL/build --config RelWithDebInfo --prefix 3rdParty/SDL/RelWithDebInfo
