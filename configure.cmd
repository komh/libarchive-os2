extproc sh

./configure. --disable-shared --enable-static \
             LIBXML2_PC_LIBS="$(pkg-config --static --libs libxml-2.0)" \
             "$@"
