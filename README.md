This is a fairly simple Sudoku solver written in C. An exercise in bactracking I did back in 2005 to cheat in a contest of a newspaper. They published a daily Sudoku, and you had to send the solution to win something I have forgotten. Come on, who wants to solve a Sudoku when you can solve them all, uh?

The backtracking loop is compact, it is kind of generic to many backtracking problems I believe.

There is a matrix that stores the numbers, and there are redundant structures for rows, columns, and the nine sub-squares that maintain bit masks. They allow the program to check stuff using bitwise operators, which proved to be a fast approach compared to other options.

Other than that, the program is not very smart, just brute force backtracking with the obvious optimizations.

To compile _sudoku.c_ run

    gcc -O3 -std=c99 -o sudoku sudoku.c

The program receives the square as a series of numbers of three digits. Each argument represents one of the known numbers in the Sudoku and is entered as _row-col-number_, where _row_ and _col_ are 1-based.

This is for example the solution to the [Sudoku in the Wikipedia](http://en.wikipedia.org/wiki/Sudoku):

    $ time ./sudoku 115 123 157 216 241 259 265 329 338 386 418 456 493 514 548 563 591 617 652 696 726 772 788 844 851 869 895 958 987 999
    +---------+---------+---------+
    | 5  3  4 | 6  7  8 | 9  1  2 |
    | 6  7  2 | 1  9  5 | 3  4  8 |
    | 1  9  8 | 3  4  2 | 5  6  7 |
    +---------+---------+---------+
    | 8  5  9 | 7  6  1 | 4  2  3 |
    | 4  2  6 | 8  5  3 | 7  9  1 |
    | 7  1  3 | 9  2  4 | 8  5  6 |
    +---------+---------+---------+
    | 9  6  1 | 5  3  7 | 2  8  4 |
    | 2  8  7 | 4  1  9 | 6  3  5 |
    | 3  4  5 | 2  8  6 | 1  7  9 |
    +---------+---------+---------+

    real	0m0.003s
    user	0m0.001s
    sys		0m0.002s

This is the puzzle that a Finnish mathematician [claims to be the hardest one known for a human](http://www.efamol.com/efamol-news/news-item.php?id=43):

    $ time ./sudoku 118 233 246 327 359 372 425 467 554 565 577 641 683 731 786 798 838 845 881 929 974
    +---------+---------+---------+
    | 8  1  2 | 7  5  3 | 6  4  9 |
    | 9  4  3 | 6  8  2 | 1  7  5 |
    | 6  7  5 | 4  9  1 | 2  8  3 |
    +---------+---------+---------+
    | 1  5  4 | 2  3  7 | 8  9  6 |
    | 3  6  9 | 8  4  5 | 7  2  1 |
    | 2  8  7 | 1  6  9 | 5  3  4 |
    +---------+---------+---------+
    | 5  2  1 | 9  7  4 | 3  6  8 |
    | 4  3  8 | 5  2  6 | 9  1  7 |
    | 7  9  6 | 3  1  8 | 4  5  2 |
    +---------+---------+---------+

    real    0m0.006s
    user    0m0.004s
    sys     0m0.001s

The exact milliseconds vary a little between runs, but those are the magnitudes. Those examples ran in my MacBook Air 11'' mid-2012, with a dual core i7 (Ivy Bridge).

The validity of command line parameters is checked. In particular it is not possible to specify a puzzle that breaks the Sudoku rules.

The puzzle cells can now be specified in various ways:

    rcn         Three digits define a single cell in (above mentioned) row-column-number
                format ("234" puts 4 to row 2/column 3).
                r and c must be in range 1..9; n in range 0..9 where 0 is empty cell.
    rc:n        Same as rcn
    r:nnnnnnnnn Nine digits define the row given by r.
    nnnnnnnnn   Nine digits define the "next" row, starting at row 1.
                If reading a puzzle from stdin you typically enter 9 of such lines.
    81 digits   81 digits define the complete puzzle at once.
    #anything   A leading # marks a cell definition as comment.

A single dash (-) as 1st cell definition reads definitions from stdin, line by line and in one line separated by blanks. Lines starting with # and empty lines are ignored. The cell definitions on the command line are applied afterwards (and modify the puzzle from file before solving it).

If file _my.sudoku_ contains cell definitions like this:

    # This is a minimal 17-cell sudoku puzzle
    000000010
    400000000
    020000000
    000050407
    008000300
    001090000
    300400200
    050100000
    000806000

This will read and solve it:

    sudoku - <my.sudoku
    
This will read it, clears number 4 in 2nd row, replace number 1 on 1st row by 2 and then solve it:

    sudoku - 210 182 <my.sudoku

If a puzzle has more than one solution all of them can be found, see _--max_ option. By default the solver looks for a 2nd solution because it is an interesting information if a puzzle has exactly one solution.

If you are interested in number of solutions and not in solutions itself then the solver can run silent, not printing all the solutions. See _--silent_ option.

Finally there is a _--help_ option available.

