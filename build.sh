g++ -std=c++14 -shared -fPIC src/hooks.cpp -I3rdparty/PFishHook/ -L3rdparty/PFishHook/build/ -lPFishHook 3rdparty/PFishHook/build/3rdparty/zydis/libZydis.a -ldl -g -o libRootless.so 
