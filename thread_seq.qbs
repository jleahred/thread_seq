import qbs

CppApplication {
    consoleApplication: true
    files: [
        "lib/thread_seq.cpp",
        "lib/thread_seq.h",
        "main.cpp",
    ]
    cpp.dynamicLibraries: [ "pthread" ]


    Group {     // Properties for the produced executable
        fileTagsFilter: "application"
        qbs.install: true
        qbs.installDir: "bin"
    }
}
