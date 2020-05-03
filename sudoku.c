/**
 * Command line interface sudoku solver.
 * Based on code from "fxn" (https://github.com/fxn/sudoku)
 * Enhancements by "yablacky" (https://github.com/yablacky/sudoku)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

int parse_options(int argc, char **argv);
int square(int i, int j);
bool set_cell(int i, int j, int n);
int clear_cell(int i, int j);
int init_known(int count, const char** cells, int next_row);
bool can_set(int i, int j, int n);
bool advance_cell(int i, int j);
bool solve_sudoku(int found);
void init_bits(void);
void print_matrix(void);
void print_best_matrix(void);
void print_separator(void);

/* The Sudoku matrix itself. */
int matrix[9][9];

/* Which numbers were given as known in the problem. */
int known[9][9];

/* If no solution, best_matrix has the most cells filled. */
int best_matrix[9][9];

/* Number of cell filled in best_matrix. */
int best_pos = 0;

/* Number of best_matrix of best_pos found. */
int best_pos_count = 0;

/* An array of nine integers, each of which representing a sub-square.
Each integer has its nth-bit on iff n belongs to the corresponding sub-square. */
int squares[9];

/* An array of nine integers, each of which representing a row.
Each integer has its nth-bit on iff n belongs to the corresponding row. */
int rows[9];

/* An array of nine integers, each of which representing a column.
Each integer has its nth-bit on iff n belongs to the corresponding column. */
int cols[9];

/* An array with some powers of 2 to avoid shifting all the time. */
int bits[10];

/* Number of solutions to search for in case there are more than one */
int find_max = 2;

/* Prevent output while searching for multiple solutions */
int silent = 0;

const char *prog = "?";

void print_usage()
{
    printf(
        "Find sudoku solutions using a backtracking algorithm.\n"
        "usage: %s [--help] [--silent] [--max n] [-] [cell_definition...]\n"
        "  --max nnn, -m nnn   Where nnn is a number (default 2).\n"
        "                 Do not search more solutions than this.\n"
        "  --silent, -s   Do not print solution boards, count them only.\n"
        "                 Specify twice to be even more silent.\n"
        "  --help, -h, -? Print this help.\n"
        "  -           A single dash reads cell_definition from stdin,\n"
        "              line by line or in one line, separated by blanks.\n"
        "              Use io-redirection to read from a file.\n"
        "  cell_definitions on the command line are applied after and\n"
        "  override those read from file.\n"
        "\n"
        "  cell_definition can be given in these formats:\n"
        "\n"
        "  #anything   Preset option ignored if starting with #.\n"
        "  rcn         Three digits for row, column and number.\n"
        "              Sets number n in cell [row,column].\n"
        "              r and c must be in range 1..9. n in range 0..9.\n"
        "              Example: '123' sets 3 to cell[1,2].\n"
        "  rc:n        Same as rcn\n"
        "              Example: '12:3' sets 3 to cell[1,2].\n"
        "  r:nnnnnnnnn 9 digits to define the complete row given by r.\n"
        "              A 0 and each non-digit means empty cell.\n"
        "              Example: '4:2..000..8' is the same as 412 498.\n"
        "  nnnnnnnnn   9 Digits defining the complete 'next' row\n"
        "              starting with row 1. In a file you typically\n"
        "              use 9 lines like this to define all cells.\n"
        "  81 digits   Exactly 81 digits define all cells at once.\n"
        "\n"
        "Example of minimal sodoku with 17 preset cells:\n"
        "  181 214 322 455 474 497 538 573 631 659 713 744 772 825 841 948 966\n"
        , prog);
}

int main(int argc, char** argv)
{
    prog = *argv++; argc--;

    init_bits();

    int nn = parse_options(argc, argv);
    if (nn < 0) {
        exit(nn == -2 ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    argv += nn; argc -= nn;

    int next_row = 0;
    if (argc > 0 && !strcmp(argv[0], "-")) {
        argv++; argc--;
        char line[256], *tokctx;
        while (fgets(line, sizeof(line) - 1, stdin)) {
            char *preset = strtok_s(line, " \r\n", &tokctx);
            while (preset) {
                next_row = init_known(1, &preset, next_row);
                if (next_row < 0) {
                    print_matrix();
                    exit(EXIT_FAILURE);
                }
                preset = strtok_s(NULL, " \r\n", &tokctx);
            }
        }
    }

    if (init_known(argc, argv, next_row) < 0) {
        print_matrix();
        exit(EXIT_FAILURE);
    }

    int found = 0;
    while (found < find_max && solve_sudoku(found)) {
        found++;
        if (silent == 0) {
            printf("Solution #%d found:\n", found);
            print_matrix();
        }
        else if (silent == 1) {
            printf("Found %d\r", found);
        }
    }

    if (find_max < 1) {
        printf("No solution requested. The matrix is:\n");
        print_matrix();
    }
    else if (found >= find_max) {
        if (silent == 1) {
            printf("Solution #%d found:\n", found);
            print_matrix();
        }
        printf("Stopped after found %d. More solutions may exist.\n", found);
    }
    else if (found > 0) {
        if (silent == 1) {
            printf("Solution #%d found:\n", found);
            print_matrix();
        }
        printf("Found %d. No more solutions exist.\n", found);
    }
    else {
        if (best_pos_count > 0) {
            printf("No solution found. Mostly filled matrix (%d times):\n", best_pos_count);
            print_best_matrix();
        }
        else {
            printf("No solution found:\n");
            print_matrix();
        }
    }

    return EXIT_SUCCESS;
}

/* Parse command line options, setup global variables.
Return the number of args eaten. A value < 0 indicates error (-2 for usage). */
int parse_options(int argc, char **argv)
{
    int argc0 = argc;
    while (argc > 0 && argv[0][0] == '-' && argv[0][1]) {    // is not just a single '-'
        char *opt = 1 + *argv++; argc--;
        char *arg = NULL;
        if (*opt == '-') {    // this is potentially --opt[=arg]
            if (!*++opt)
                break;        // just "--" stops option parsing
            if ((arg = strchr(opt, '=')) != NULL) {
                *arg++ = 0;
            }
            if (!strcmp(opt, "help")) {
                print_usage();
                return -2;
            }
            if (!strcmp(opt, "silent")) {
                silent++;
                continue;
            }
            if (!strcmp(opt, "max")) {
                if (!arg) {
                    if (--argc < 0) {
                        fprintf(stderr, "%s: --max requires value", prog);
                        return -1;
                    }
                    arg = *argv++;
                }
                find_max = atoi(arg);
                continue;
            }
            fprintf(stderr, "%s: unknown option: --%s", prog, opt);
            return -1;
        }
        for (; *opt; opt++) {
            switch (*opt) {
            case 'h': case '?':
                print_usage();
                return -2;
            case 's':
                silent++;
                break;
            case 'm':
                if (!arg) {
                    if (--argc < 0) {
                        fprintf(stderr, "%s: --max requires value", prog);
                        return -1;
                    }
                    arg = *argv++;
                }
                find_max = atoi(arg);
                break;
            default:
                fprintf(stderr, "%s: unknown option: -%c", prog, *opt);
                return -1;
            }
        }
    }
    return argc0 - argc;
}

/* Returns the index of the square the cell (i, j) belongs to. */
int square(int i, int j)
{
    return (i/3)*3 + j/3;
}

/* Set cell(i, j) to number n if this does not break the rules.
Turn on the corresponding bits in rows, cols, and squares.
Return success status.
*/
bool set_cell(int i, int j, int n)
{
    if (! can_set(i, j, n)) {
        return false;
    }
    matrix[i][j] = n;
    rows[i] |= bits[n];
    cols[j] |= bits[n];
    squares[square(i, j)] |= bits[n];
    return true;
}

/* Clears the cell (i, j) and turns off the corresponding bits in rows, cols,
and squares. Returns the number it contained. */
int clear_cell(int i, int j)
{
    int n = matrix[i][j];
    matrix[i][j] = 0;
    rows[i] &= ~bits[n];
    cols[j] &= ~bits[n];
    squares[square(i, j)] &= ~bits[n];
    return n;
}

/* Processes the program arguments. Each argument is assumed to be a string
with three digits row-col-number, 1-based, representing the known cells in the
Sudoku. For example, "123" means there is a 3 in the cell (0, 1). */
int init_known(int count, const char** cells, int next_row)
{
    int nerr = 0, i = next_row;
    for (int c = 0; c < count; ++c, ++i) {
        const char* cell = cells[c], *row = NULL;
        char r;
        int j, n, m;
        if (cell[0] == '#')    // commented out - ignore this preset.
            continue;
        if (!strchr(cell, ':') && strlen(cell) == 81) {
            // preset the complete 81 cells of the matrix
            for (i = 0; i < 9; ++i) {
                for (j = 0; j < 9; ++j) {
                    m = clear_cell(i, j);
                    if (m != 0)
                        fprintf(stderr, "token #%d \"%s\": overwrites %d at [%d,%d]\n", c + 1, cell, m, i + 1, j + 1);
                }
            }
            for (m = 0; m < 81; m++) {
                i = m / 9; j = m % 9;
                n = cell[m] - '0';
                if (n < 0 || n > 9)        // treat everything except digits as 0
                    n = 0;
                if (n != 0 && !set_cell(i, j, n)) {
                    fprintf(stderr, "token #%d \"%s\": setting %d at [%d,%d] breaks rules\n", c + 1, cell, n, i + 1, j + 1);
                    nerr++;
                }
                known[i][j] = n != 0;
            }
            continue;
        }
        if (!strchr(cell, ':') && strlen(cell) == 9) {
            // preset the complete "next" row i
            row = cell;
            if (i < 0 || i > 8) {
                fprintf(stderr, "token #%d \"%s\": too many rows, got %d\n", c + 1, cell, i + 1);
                nerr++;
                continue;
            }
        }
        else if (sscanf_s(cell, "%1d:%n%1c", &i, &n, &r, 1) == 2) {
            // preset the complete specified row
            if (i < 1 || i > 9) {
                fprintf(stderr, "token #%d \"%s\": bad row %d\n", c+1, cell, i);
                nerr++;
                continue;
            }
            i--;
            row = cell + n;
        }
        if (row)
        {
            if (strlen(row) != 9) {
                fprintf(stderr, "token #%d \"%s\": need 9 column values, got %d\n", c + 1, cell, strlen(row));
                nerr++;
                continue;
            }
            for (j = 0; j < 9; j++) {
                n = row[j] - '0';
                if (n < 0 || n > 9)        // treat everything except digits as 0
                    n = 0;
                m = clear_cell(i, j);
                if (m != 0)
                    fprintf(stderr, "token #%d \"%s\": overwrites %d at [%d,%d]\n", c + 1, cell, m, i + 1, j + 1);
                if (n != 0 && !set_cell(i, j, n)) {
                    fprintf(stderr, "token #%d \"%s\": setting %d at [%d,%d] breaks rules\n", c + 1, cell, n, i + 1, j + 1);
                    nerr++;
                }
                known[i][j] = n != 0;
            }
        }
        else if (sscanf_s(cell, "%1d%1d%1d%1c", &i, &j, &n, &r, 1) == 3 ||
                 sscanf_s(cell, "%1d%1d:%1d%1c", &i, &j, &n, &r, 1) == 3) {
            // preset a given cell
            if (i < 1 || i > 9) {
                fprintf(stderr, "token #%d \"%s\": bad row %d\n", c+1, cell, i);
                nerr++;
                continue;
            }
            if (j < 1 || j > 9) {
                fprintf(stderr, "token #%d \"%s\": bad column %d\n", c+1, cell, j);
                nerr++;
                continue;
            }
            i--, j--;
            m = clear_cell(i, j);
            if (m != 0)
                fprintf(stderr, "token #%d \"%s\": overwrites %d at [%d,%d]\n", c+1, cell, m, i+1, j+1);
            if (n != 0 && !set_cell(i, j, n)) {
                fprintf(stderr, "token #%d \"%s\": setting %d at [%d,%d] breaks rules\n", c + 1, cell, n, i + 1, j + 1);
                nerr++;
            }
            known[i][j] = n != 0;
        } else {
            fprintf(stderr, "token #%d: \"%s\": bad format\n", c+1, cell);
            nerr++;
        }
    }
    return nerr != 0 ? -1 : i;
}

/* Can we put n in the cell (i, j)? */
bool can_set(int i, int j, int n)
{
    return (rows[i] & bits[n]) == 0 && (cols[j] & bits[n]) == 0 && (squares[square(i, j)] & bits[n]) == 0;
}

/* Tries to fill the cell (i, j) with the next available number.
Returns a flag to indicate if it succeeded. */
bool advance_cell(int i, int j)
{
    int n = clear_cell(i, j);
    while (++n <= 9) {
        if (set_cell(i, j, n)) {
            return true;
        }
    }
    return false;
}

/* The main function, a fairly generic backtracking algorithm. */
bool solve_sudoku(int found)
{
    int pos = 0;
    if (found > 0) {
        // Assume the board has all cells set; find next solution.
        pos = 80;
        while (pos >= 0 && known[pos/9][pos%9]) {
            --pos;
        }
        if (pos < 0) {    // all cells are known cells...
            return false;
        }
    }
    while (1) {
        while (pos < 81 && known[pos/9][pos%9]) {
            ++pos;
        }
        if (pos >= 81) {
            return true;    // All cells set: solution found; may be there are more...
        }
        if (advance_cell(pos/9, pos%9)) {
            ++pos;

            if (pos > best_pos) {
                memcpy(best_matrix, matrix, sizeof(matrix));
                best_pos = pos;
                best_pos_count = 1;
            }
            else if (pos == best_pos) {
                best_pos_count++;
            }

        } else {
            do {
                --pos;
            } while (pos >= 0 && known[pos/9][pos%9]);
            if (pos < 0) {
                return false; // No solution found
            }
        }
    }
}

/* Initializes the array with powers of 2. */
void init_bits(void)
{
    for (int n = 1; n < 10; n++) {
        bits[n] = 1 << n;
    }
}

/* Print the puzzle given as parameter. Originally known numbers are wrapped in parenthesis. */
void print_mat(int mat[9][9])
{
    for (int i = 0; i < 9; ++i) {
        if ((i % 3) == 0) {
            print_separator();
        }
        for (int j = 0; j < 9; j++) {
            int cell = mat[i][j];
            if ((j % 3) == 0) {
                printf("|");
            }
            if (known[i][j]) {
                printf("(%d)", cell);
            } else {
                printf(" %d ", cell);
            }
        }
        printf("|\n");
    }
    print_separator();
}

/* Prints the matrix. */
void print_matrix(void)
{
    print_mat(matrix);
}
void print_best_matrix(void)
{
    print_mat(best_matrix);
}

/* Utility to print lines and crosses, used by print_matrix. */
void print_separator(void)
{
    for (int i = 0; i < 3; ++i) {
        printf("+---------");
    }
    printf("+\n");
}
