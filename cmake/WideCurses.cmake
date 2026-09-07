# Locates a curses library that provides everything the engine calls, and
# exposes it as the interface target petriterm::curses.
#
# Written as a compile probe rather than a library-name lookup, because the name
# answers none of the questions that matter. "ncursesw" is a separate library on
# Debian and a symlink to ncurses on Arch and Alpine; Fedora puts wide support in
# the plain ncurses.h; macOS ships an ncurses 5.7 that has the wide API but hides
# it behind _XOPEN_SOURCE_EXTENDED, and calls the header curses.h. On top of
# that, wide characters and the ncurses extensions (set_escdelay,
# use_default_colors, resizeterm) are independent build options, so a library can
# have either without the other. The only question with a reliable answer is
# whether the code compiles and links, so that is the question asked.
#
# On success this sets, in the including scope:
#   petriterm::curses             interface target carrying the include
#                                 directories, libraries and feature macros
#   PETRITERM_CURSES_HEADER_NAME  the header Curses.hpp.in is configured with
#   PETRITERM_CURSES_DESCRIPTION  a one-line summary for the configure output
#
# On failure it calls message(FATAL_ERROR) naming what was tried, because a
# missing curses is not something the build can proceed without.

# The probe body lives in its own source file next to this one. See the comment
# at the top of it for why it is a file and not a string.
file(READ ${CMAKE_CURRENT_LIST_DIR}/wide_curses_probe.cpp PETRITERM_CURSES_PROBE_BODY)

# Header spellings, most specific first. ncursesw/ncurses.h is tried ahead of
# ncurses.h so that on a distribution shipping both flavours the wide one is
# picked even where the bare header happens to satisfy the probe too.
set(PETRITERM_CURSES_HEADER_CANDIDATES ncursesw/ncurses.h ncurses.h curses.h)

# Feature macros to try, in order. The empty entry is first so that nothing is
# defined on the platforms that do not need it: _XOPEN_SOURCE_EXTENDED also
# narrows what other system headers declare, and asking for it where curses
# already exposes the wide API buys a portability risk for nothing. macOS is the
# platform that needs it, where the system curses is an ncurses 5.7 that keeps
# the wide API behind it.
set(PETRITERM_CURSES_MACRO_CANDIDATES "" "_XOPEN_SOURCE_EXTENDED")

# Tries one library-and-include-directory candidate against every header
# spelling and feature-macro combination, and reports the first that builds.
#
# Takes variable *names* for the two list arguments, because CMake would
# otherwise flatten them into the surrounding argument list and the function
# could not tell where one ended and the next began.
function(petriterm_probe_curses_candidate candidateLabel includeDirsVariable
         librariesVariable resultVariable)
    set(candidateIncludeDirs "${${includeDirsVariable}}")
    set(candidateLibraries "${${librariesVariable}}")
    set(probeIndex 0)

    foreach(headerName IN LISTS PETRITERM_CURSES_HEADER_CANDIDATES)
        foreach(featureMacro IN LISTS PETRITERM_CURSES_MACRO_CANDIDATES)
            math(EXPR probeIndex "${probeIndex} + 1")
            set(probeDirectory
                "${CMAKE_CURRENT_BINARY_DIR}/wide-curses-probes/${candidateLabel}-${probeIndex}")
            set(probeSource "${probeDirectory}/probe.cpp")
            file(WRITE "${probeSource}"
                 "#include <${headerName}>\n${PETRITERM_CURSES_PROBE_BODY}")

            set(probeDefinitions "")
            if(NOT featureMacro STREQUAL "")
                set(probeDefinitions "-D${featureMacro}")
            endif()

            # try_compile rather than check_cxx_source_compiles: the latter is a
            # macro that re-parses the source as CMake code. The CXX_STANDARD
            # here has to match the real build, since that is what decides
            # whether the curses headers see a C++20 compiler.
            try_compile(probeSucceeded "${probeDirectory}/build"
                        SOURCES "${probeSource}"
                        CMAKE_FLAGS
                            "-DINCLUDE_DIRECTORIES=${candidateIncludeDirs}"
                        COMPILE_DEFINITIONS ${probeDefinitions}
                        LINK_LIBRARIES ${candidateLibraries}
                        CXX_STANDARD 20
                        CXX_STANDARD_REQUIRED TRUE
                        OUTPUT_VARIABLE probeOutput)

            if(probeSucceeded)
                message(STATUS
                        "Curses probe: ${candidateLabel} with <${headerName}> - works")
                set(PETRITERM_CURSES_HEADER_NAME "${headerName}" PARENT_SCOPE)
                set(PETRITERM_CURSES_FOUND_INCLUDE_DIRS "${candidateIncludeDirs}"
                    PARENT_SCOPE)
                set(PETRITERM_CURSES_FOUND_LIBRARIES "${candidateLibraries}" PARENT_SCOPE)
                set(PETRITERM_CURSES_FOUND_MACRO "${featureMacro}" PARENT_SCOPE)
                set(${resultVariable} TRUE PARENT_SCOPE)
                return()
            endif()

            # Kept at DEBUG rather than dropped. A configure that ends in the
            # fatal error below is nearly always debugged by asking which of
            # these combinations got closest, and the compiler output is the
            # only thing that answers it.
            message(DEBUG "Curses probe: ${candidateLabel} with <${headerName}>"
                          " macro '${featureMacro}' failed:\n${probeOutput}")
        endforeach()
    endforeach()

    set(${resultVariable} FALSE PARENT_SCOPE)
endfunction()

# Homebrew's ncurses is keg-only, so it is not on any default search path and a
# macOS build silently gets the system ncurses 5.7 instead. Point the searches
# below at it when it is installed: 6.x needs no feature macros, reports colors
# honestly, and is the build a macOS user who installed it is expecting to get.
# The system curses stays as the fallback, so this is a preference and not a
# requirement.
if(APPLE)
    find_program(PETRITERM_BREW_EXECUTABLE brew)
    if(PETRITERM_BREW_EXECUTABLE)
        execute_process(
            COMMAND ${PETRITERM_BREW_EXECUTABLE} --prefix ncurses
            OUTPUT_VARIABLE PETRITERM_BREW_NCURSES_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(PETRITERM_BREW_NCURSES_PREFIX AND
           IS_DIRECTORY "${PETRITERM_BREW_NCURSES_PREFIX}")
            list(PREPEND CMAKE_PREFIX_PATH "${PETRITERM_BREW_NCURSES_PREFIX}")
            set(ENV{PKG_CONFIG_PATH}
                "${PETRITERM_BREW_NCURSES_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
            message(STATUS "Preferring Homebrew ncurses at "
                           "${PETRITERM_BREW_NCURSES_PREFIX}")
        endif()
    endif()
endif()

set(PETRITERM_CURSES_CANDIDATE_FOUND FALSE)
set(PETRITERM_CURSES_CANDIDATES_TRIED "")

# pkg-config first. It is the only source that knows the exact include directory
# and the full transitive link line for the flavour installed, which is what the
# ncursesw-versus-tinfo split needs: on a system where the terminfo functions
# live in a separate libtinfo, guessing the library list gets a link error that
# looks like a missing symbol rather than a missing dependency.
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    foreach(moduleName ncursesw ncurses)
        if(PETRITERM_CURSES_CANDIDATE_FOUND)
            break()
        endif()
        pkg_check_modules(PETRITERM_PKG_CURSES QUIET ${moduleName})
        if(PETRITERM_PKG_CURSES_FOUND)
            list(APPEND PETRITERM_CURSES_CANDIDATES_TRIED "pkg-config ${moduleName}")
            petriterm_probe_curses_candidate(
                "pkgconfig_${moduleName}"
                PETRITERM_PKG_CURSES_INCLUDE_DIRS
                PETRITERM_PKG_CURSES_LINK_LIBRARIES
                PETRITERM_CURSES_CANDIDATE_FOUND)
            if(PETRITERM_CURSES_CANDIDATE_FOUND)
                set(PETRITERM_CURSES_DESCRIPTION
                    "pkg-config ${moduleName} ${PETRITERM_PKG_CURSES_VERSION}")
            endif()
        endif()
    endforeach()
endif()

# CMake's own module next, for systems with no pkg-config files for curses.
# CURSES_NEED_WIDE only steers which library name it looks for; whether that
# library is actually usable is still decided by the probe.
if(NOT PETRITERM_CURSES_CANDIDATE_FOUND)
    set(CURSES_NEED_WIDE TRUE)
    find_package(Curses QUIET)
    if(CURSES_FOUND)
        list(APPEND PETRITERM_CURSES_CANDIDATES_TRIED "find_package(Curses)")
        petriterm_probe_curses_candidate(
            "find_package" CURSES_INCLUDE_DIRS CURSES_LIBRARIES
            PETRITERM_CURSES_CANDIDATE_FOUND)
        if(PETRITERM_CURSES_CANDIDATE_FOUND)
            set(PETRITERM_CURSES_DESCRIPTION "find_package(Curses)")
        endif()
    endif()
endif()

# Last resort: look the libraries up by name and let the probe sort out which
# combination works. The tinfo variants are here because a curses split that way
# with no pkg-config file has no other way of being discovered.
if(NOT PETRITERM_CURSES_CANDIDATE_FOUND)
    find_path(PETRITERM_CURSES_BARE_INCLUDE_DIR
              NAMES ncursesw/ncurses.h ncurses.h curses.h)
    find_library(PETRITERM_CURSES_BARE_TERMINFO_LIBRARY NAMES tinfow tinfo)

    foreach(libraryName ncursesw ncurses curses)
        if(PETRITERM_CURSES_CANDIDATE_FOUND)
            break()
        endif()
        find_library(PETRITERM_CURSES_BARE_LIBRARY_${libraryName} NAMES ${libraryName})
        if(NOT PETRITERM_CURSES_BARE_LIBRARY_${libraryName})
            continue()
        endif()
        list(APPEND PETRITERM_CURSES_CANDIDATES_TRIED "lib${libraryName}")

        set(PETRITERM_CURSES_BARE_INCLUDE_DIRS "")
        if(PETRITERM_CURSES_BARE_INCLUDE_DIR)
            set(PETRITERM_CURSES_BARE_INCLUDE_DIRS
                "${PETRITERM_CURSES_BARE_INCLUDE_DIR}")
        endif()

        set(PETRITERM_CURSES_BARE_LIBRARIES
            "${PETRITERM_CURSES_BARE_LIBRARY_${libraryName}}")
        petriterm_probe_curses_candidate(
            "bare_${libraryName}" PETRITERM_CURSES_BARE_INCLUDE_DIRS
            PETRITERM_CURSES_BARE_LIBRARIES PETRITERM_CURSES_CANDIDATE_FOUND)

        if(NOT PETRITERM_CURSES_CANDIDATE_FOUND AND
           PETRITERM_CURSES_BARE_TERMINFO_LIBRARY)
            list(APPEND PETRITERM_CURSES_BARE_LIBRARIES
                 "${PETRITERM_CURSES_BARE_TERMINFO_LIBRARY}")
            petriterm_probe_curses_candidate(
                "bare_${libraryName}_tinfo" PETRITERM_CURSES_BARE_INCLUDE_DIRS
                PETRITERM_CURSES_BARE_LIBRARIES PETRITERM_CURSES_CANDIDATE_FOUND)
        endif()

        if(PETRITERM_CURSES_CANDIDATE_FOUND)
            set(PETRITERM_CURSES_DESCRIPTION "lib${libraryName} found by name")
        endif()
    endforeach()
endif()

if(NOT PETRITERM_CURSES_CANDIDATE_FOUND)
    # The two failures need different advice and are easy to confuse: one is a
    # missing package, the other a package present but built without what the
    # game needs, and only the second is worth reading probe output over.
    if(PETRITERM_CURSES_CANDIDATES_TRIED)
        string(REPLACE ";" ", " PETRITERM_CURSES_TRIED_TEXT
               "${PETRITERM_CURSES_CANDIDATES_TRIED}")
        string(CONCAT PETRITERM_CURSES_FAILURE_DETAIL
               "Found and rejected: ${PETRITERM_CURSES_TRIED_TEXT}. Re-run with "
               "--log-level=DEBUG for each probe's compiler output.")
    else()
        string(CONCAT PETRITERM_CURSES_FAILURE_DETAIL
               "No curses library was found at all - not by pkg-config, not by "
               "find_package(Curses), and not by library name.")
    endif()
    message(FATAL_ERROR
        "No usable curses found. PetriTerm needs a curses with the wide-character "
        "API (get_wch, cchar_t) and the ncurses extensions set_escdelay, "
        "use_default_colors and resizeterm.\n"
        "${PETRITERM_CURSES_FAILURE_DETAIL}\n"
        "Install the wide ncurses development package - libncurses-dev on "
        "Debian and Ubuntu, ncurses-devel on Fedora, ncurses-dev on Alpine, "
        "ncurses on Arch, or 'brew install ncurses' on macOS - then configure "
        "again.")
endif()

add_library(petriterm::curses INTERFACE IMPORTED)
target_link_libraries(petriterm::curses
                      INTERFACE ${PETRITERM_CURSES_FOUND_LIBRARIES})
if(PETRITERM_CURSES_FOUND_INCLUDE_DIRS)
    # SYSTEM so the curses headers are exempt from -Wall -Wextra -Wpedantic
    # -Werror. They are not ours to keep clean, and several distributions ship
    # headers that do not survive -Wpedantic.
    target_include_directories(petriterm::curses SYSTEM
                               INTERFACE ${PETRITERM_CURSES_FOUND_INCLUDE_DIRS})
endif()
if(PETRITERM_CURSES_FOUND_MACRO)
    # On the compile line rather than in Curses.hpp: a feature macro has to be
    # defined before any system header is read, and a header cannot guarantee it
    # is the first one included in the translation unit.
    target_compile_definitions(petriterm::curses
                               INTERFACE ${PETRITERM_CURSES_FOUND_MACRO})
    set(PETRITERM_CURSES_DESCRIPTION
        "${PETRITERM_CURSES_DESCRIPTION}, -D${PETRITERM_CURSES_FOUND_MACRO}")
endif()

message(STATUS "Curses: ${PETRITERM_CURSES_DESCRIPTION}, "
               "header <${PETRITERM_CURSES_HEADER_NAME}>")
