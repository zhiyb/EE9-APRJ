import qbs

Project {
    references: ["codec.qbs"]

    CppApplication {
        consoleApplication: true
        Depends {name: "codec"}
        cpp.rpaths: qbs.installRoot
        cpp.cxxLanguageVersion: "c++11"

        files: [
            "audio.mp3",
            "fmod.cpp",
        ]

        FileTagger {
            patterns: "*.mp3"
            fileTags: "mp3"
        }

        Group {     // Properties for the produced executable
            fileTagsFilter: product.type.concat("mp3")
            qbs.install: true
        }
    }
}
