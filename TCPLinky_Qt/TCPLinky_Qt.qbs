import qbs

QtGuiApplication {
    consoleApplication: true
    cpp.cxxLanguageVersion: "c++11"
    Depends {name: "Qt.widgets"}
    Depends {name: "Qt.network"}

    files: [
        "*.qrc",
        "*.cpp",
        "*.h",
    ]
}
