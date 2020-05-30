import qbs

CppApplication {
    consoleApplication: true
    files: [
        "lib/threadseq.cpp",
        "lib/threadseq.h",
        "main.cpp",
    ]
    cpp.dynamicLibraries: [ "pthread" ]


    Group {     // Properties for the produced executable
        fileTagsFilter: "application"
        qbs.install: true
        qbs.installDir: "bin"
    }
}
