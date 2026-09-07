// Configure-time probe for cmake/WideCurses.cmake. Not compiled into the game.
//
// This is the definition of what PetriTerm needs from curses. A candidate
// library is accepted only if all of it compiles and links, which is a stronger
// claim than the library's name supports: wide characters, the ncurses
// extensions, and resizeterm are independent build options, so a library called
// ncursesw may lack any of them and one called curses may have all three. When
// a source file starts calling something new from curses, add it here too -
// otherwise the first report of it missing arrives as a build failure on
// somebody else's distribution instead of a configure failure on ours.
//
// A real source file rather than a string inside the CMake module, because
// check_cxx_source_compiles is a macro: it re-parses its argument as CMake code,
// where a C++ escape sequence is a syntax error. WideCurses.cmake prepends the
// curses include line to this and hands the result to try_compile.
//
// Nothing here runs. try_compile links the program and stops, so there is no
// terminal for initscr to fail to find.

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    set_escdelay(25);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_WHITE, COLOR_BLACK);
    }

    wint_t keyValue = 0;
    const int readStatus = get_wch(&keyValue);
    const bool sawResize = readStatus == KEY_CODE_YES && keyValue == KEY_RESIZE;

    // Terminated with a plain 0 rather than a wide null escape, to keep this file
    // free of backslashes: the module prepends a line and rewrites it, and an
    // escape sequence has repeatedly been the thing that breaks that path.
    cchar_t glyphCell;
    const wchar_t glyphText[2] = {L'x', 0};
    setcchar(&glyphCell, glyphText, A_BOLD, 0, nullptr);
    wmove(stdscr, 0, 0);
    mvwadd_wch(stdscr, 0, 0, &glyphCell);
    wattr_set(stdscr, A_NORMAL, 0, nullptr);

    int rowCount = 0;
    int columnCount = 0;
    getmaxyx(stdscr, rowCount, columnCount);
    resizeterm(rowCount, columnCount);
    clearok(stdscr, TRUE);
    werase(stdscr);
    wnoutrefresh(stdscr);
    doupdate();
    endwin();

    return (sawResize && COLOR_PAIRS > 0) ? 1 : 0;
}
