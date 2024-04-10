# /bin/bash
#
#
CURRENT_FOLDER="../"
./configure LDFLAGS="-L$CURRENT_FOLDER/minisat/build/release/lib/" --with-uclibc="$CURRENT_FOLDER/klee-uclibc"  --enable-posix-runtime
bear make -j `nproc` ENABLE_OPTIMIZED=1
