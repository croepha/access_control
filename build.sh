

echo "ASDF"

source build/build_vars


mkdir -p build/
clang++ --std=gnu++2a -Wno-writable-strings -g -fsanitize=address \
 -Ddebugf=printf \
 -lsqlite3 \
 $(pwd)/test_database.cpp \
 database.cpp \
 -o build/test_database.exec
build/test_database.exec
