g++ -c -Wall -fpic dllmain.cpp utils.cpp engine_dll/engine.cpp
g++ -shared -o libCSGO-AC.so dllmain.o utils.o engine.o
rm dllmain.o utils.o engine.o

# credits aimtux linux
csgo_pid=$(pidof csgo_linux64)
if [ -z "$csgo_pid" ]; then
    /bin/echo -e "\e[31mCSGO needs to be open before you can inject...\e[0m"
    exit 1
fi

#Credit: Aixxe @ aixxe.net
if grep -q libCSGO-AC.so /proc/$csgo_pid/maps; then
    /bin/echo -e "\e[33mCSGO-AC is already injected... Unloading...\e[0m"

    sudo gdb -n -q -batch \
        -ex "attach $(pidof csgo_linux64)" \
        -ex "set \$dlopen = (void*(*)(char*, int)) dlopen" \
        -ex "set \$dlclose = (int(*)(void*)) dlclose" \
        -ex "set \$library = \$dlopen(\"$(pwd)/libCSGO-AC.so\", 6)" \
        -ex "call \$dlclose(\$library)" \
        -ex "call \$dlclose(\$library)" \
        -ex "detach" \
        -ex "quit"

fi

input="$(
sudo gdb -n -q -batch \
  -ex "attach $csgo_pid" \
  -ex "set \$dlopen = (void*(*)(char*, int)) dlopen" \
  -ex "call \$dlopen(\"$(pwd)/libCSGO-AC.so\", 1)" \
  -ex "detach" \
  -ex "quit"
)"

last_line="${input##*$'\n'}"

if [ "$last_line" != "\$1 = (void *) 0x0" ]; then
    /bin/echo -e "\e[32mSuccessfully injected!\e[0m"
else
    /bin/echo -e "\e[31mInjection failed, make sure you've compiled...\e[0m"
fi
