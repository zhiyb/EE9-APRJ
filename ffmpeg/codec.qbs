import qbs

DynamicLibrary {
    name: "codec"
    Depends {name: "cpp"}
    cpp.commonCompilerFlags: [
        "-Wno-deprecated-declarations",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
    ]
    cpp.dynamicLibraries: [
        "avformat", "avfilter", "avcodec", "avutil", "swscale",
        "fmod", "pthread", "m",
    ]
    cpp.libraryPaths: ["/usr/local/lib"]
    cpp.rpaths: cpp.libraryPaths

    Export {
        Depends {name: "cpp"}
        cpp.includePaths: ["."]
    }

    files: [
        "codec.c",
        "codec.cs",
        "codec.h",
    ]

    Group {     // Properties for the produced executable
        fileTagsFilter: product.type
        qbs.install: true
    }
}
