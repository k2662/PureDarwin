add_library(libelf.host STATIC)
target_sources(libelf.host PRIVATE
    begin.c
    clscook.c
    clscook_ELF64.c
    cntl.c
    cook.c
    data.c
    end.c
    error.c
    gelf.c
    getdata.c
    getehdr.c
    getident.c
    getscn.c
    getshdr.c
    getshstrndx.c
    input.c
    kind.c
    ndxscn.c
    nextscn.c
    strptr.c
    xlate.c
    xlate64.c

    decl.h
    machelf.h
    msg.h

    ../include/gelf.h
)
set_property(TARGET libelf.host PROPERTY OUTPUT_NAME elf)
target_include_directories(libelf.host PRIVATE ../include ../include/sys)
