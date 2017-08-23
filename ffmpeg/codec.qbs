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
        "pthread", "m",
    ]
    cpp.libraryPaths: ["/usr/local/lib"]
    cpp.rpaths: cpp.libraryPaths

    property bool enableFMOD: false

    Properties {
        condition: enableFMOD
        cpp.dynamicLibraries: outer.concat(["fmod"]);
        cpp.defines: outer.concat(["ENABLE_FMOD"]);
    }

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
