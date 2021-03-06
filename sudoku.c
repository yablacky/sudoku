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
void set_cell_unchecked(int i, int j, int n);
int clear_cell(int i, int j);
int init_known(int count, const char** cells, int next_row);
bool can_set(int i, int j, int n);
bool advance_cell(int i, int j);
bool solve_sudoku(int found);
void init_bits();
int find_first_on(int n);
int bits_on(int n);
void print_matrix(void);
void print_best_matrix(void);
void print_separator(void);
void swap_rows(int a, int b);
void print_trace(char *info);

/* The Sudoku matrix itself. */
int matrix[9][9];

/* Which numbers were given as known in the problem. */
int known[9][9];

/* map current matrix rows and cols to the original puzzle matrix */
int row_order[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
int col_order[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };

#define SWAP(a,b) do { auto t = a; a = b; b = t; } while(0)

/* Where to start solving. */
int start_pos = 0;

/* If no solution, best_matrix has the most cells filled. */
int best_matrix[9][9];

/* Number of cell filled in best_matrix. */
int best_pos = 0;

/* Number of best_matrix of best_pos found. */
int best_pos_count = 0;

/* Just for information */
unsigned int total_moves = 0;
unsigned int best_pos_try_count = 0;

/* print matrix after each move */
int trace_moves = 0;

/* An array of nine integers, each of which representing a sub-square.
Each integer has its nth-bit on iff n belongs to the corresponding sub-square. */
int squares[9];

/* An array of nine integers, each of which representing a row.
Each integer has its nth-bit on iff n belongs to the corresponding row. */
int rows[9];

/* An array of nine integers, each of which representing a column.
Each integer has its nth-bit on iff n belongs to the corresponding column. */
int cols[9];

/* Return value where bit at position n is 1 */
#define BITAT(n) (1 << (n))

/* Return value with all bits from position 0 up to (including) position n are all 1 */
#define BITSTO(n) (BITAT((n) + 1) - 1)

/* position of the first (rightmost) zero bit. */
int first_0bit[BITSTO(9) + 1];

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
        "  --trace, -t    Trace; show matrix after each move.\n"
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

    // Initialize the matrix

    int next_row = 0;
    if (argc > 0 && !strcmp(argv[0], "-")) {
        argv++; argc--;

        // Read cell definitions from file

        char line[256], *tokctx;
        while (fgets(line, sizeof(line) - 1, stdin)) {
            if (line[0] == '#' && !(line[1] >= '0' && line[1] <= '9')) {
                // This comment applies to the complete line - with a digit
                // it could be a single commented out cell definition with
                // more (not commented out) cell definitions beind it.
                continue;
            }
            char *preset = strtok_s(line, " \t\r\n", &tokctx);
            while (preset) {
                next_row = init_known(1, &preset, next_row);
                if (next_row < 0) {
                    print_matrix();
                    exit(EXIT_FAILURE);
                }
                preset = strtok_s(NULL, " \t\r\n", &tokctx);
            }
        }
    }

    if (init_known(argc, argv, next_row) < 0) {
        print_matrix();
        exit(EXIT_FAILURE);
    }

    // Optimize solver.

	for (next_row = 3; next_row < 7; next_row += 3) {
		if (bits_on(rows[next_row + 0]) +
			bits_on(rows[next_row + 1]) +
			bits_on(rows[next_row + 2]) >
			bits_on(rows[0]) +
			bits_on(rows[1]) +
			bits_on(rows[2])
			)
			swap_rows(0, next_row);
	}
	for (next_row = 1; next_row < 3; next_row ++) {
		if (bits_on(rows[next_row]) >
			bits_on(rows[0]))
			swap_rows(0, next_row);
	}

	if (0)
    for (next_row = 0; next_row < 9; ++next_row) {
        // Prevent starting in a row with few or even no (worst case)
        // cells known because backtracking would try all the possibilities.
        if (bits_on(rows[next_row]) > bits_on(rows[start_pos / 9])) {
            start_pos = next_row * 9;
        }
    }

    // Find solutions

    int found = 0;
    while (found < find_max && solve_sudoku(found)) {
        found++;
        if (silent == 0) {
            printf("Solution #%d found (%u moves):\n", found, total_moves);
            print_matrix();
        }
        else if (silent == 1) {
            printf("Found %d (%u moves)\r", found, total_moves);
        }
    }

    // Print the result

    if (find_max < 1) {
        printf("No solution requested. The matrix is:\n");
        print_matrix();
    }
    else if (found >= find_max) {
        if (silent == 1) {
            printf("Solution #%d found (%u moves):\n", found, total_moves);
            print_matrix();
        }
        printf("Stopped after found %d (%u moves). More solutions may exist.\n", found, total_moves);
    }
    else if (found > 0) {
        if (silent == 1) {
            printf("Solution #%d found (%u moves):\n", found, total_moves);
            print_matrix();
        }
        printf("Found %d (%u moves). No more solutions exist.\n", found, total_moves);
    }
    else {
        if (best_pos_count > 0) {
            printf("No solution found (%u moves). Mostly filled matrix (%d times, 1st after %u moves):\n",
                                    total_moves, best_pos_count, best_pos_try_count);
            print_best_matrix();
        }
        else {
            printf("No solution found (%u moves):\n", total_moves);
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
            if (!strcmp(opt, "trace")) {
                trace_moves++;
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
            case 't':
                trace_moves++;
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
    if (!can_set(i, j, n)) {
        return false;
    }
    set_cell_unchecked(i, j, n);
    return true;
}

/* Set cell(i,j) to n. Caller must check rules. */
void set_cell_unchecked(int i, int j, int n)
{
    matrix[i][j] = n;
    rows[i] |= BITAT(n);
    cols[j] |= BITAT(n);
    squares[square(i, j)] |= BITAT(n);
}

/* Clears the cell (i, j) and turns off the corresponding bits in rows, cols,
and squares. Returns the number it contained. */
int clear_cell(int i, int j)
{
    int n = matrix[i][j];
    matrix[i][j] = 0;
    rows[i] &= ~BITAT(n);
    cols[j] &= ~BITAT(n);
    squares[square(i, j)] &= ~BITAT(n);
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
                    if (m != 0) {
                        fprintf(stderr, "token #%d \"%s\": overwrites %d at [%d,%d]\n", c + 1, cell, m, i + 1, j + 1);
                    }
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
                fprintf(stderr, "token #%d \"%s\": bad row %d\n", c + 1, cell, i);
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
                fprintf(stderr, "token #%d \"%s\": bad row %d\n", c + 1, cell, i);
                nerr++;
                continue;
            }
            if (j < 1 || j > 9) {
                fprintf(stderr, "token #%d \"%s\": bad column %d\n", c + 1, cell, j);
                nerr++;
                continue;
            }
            i--, j--;
            m = clear_cell(i, j);
            if (m != 0)
                fprintf(stderr, "token #%d \"%s\": overwrites %d at [%d,%d]\n", c + 1, cell, m, i + 1, j + 1);
            if (n != 0 && !set_cell(i, j, n)) {
                fprintf(stderr, "token #%d \"%s\": setting %d at [%d,%d] breaks rules\n", c + 1, cell, n, i + 1, j + 1);
                nerr++;
            }
            known[i][j] = n != 0;
        }
        else {
            fprintf(stderr, "token #%d: \"%s\": bad format\n", c + 1, cell);
            nerr++;
        }
    }
    return nerr != 0 ? -1 : i;
}

/* Can we put n in the cell (i, j)? */
bool can_set(int i, int j, int n)
{
    return (rows[i] & BITAT(n)) == 0 && (cols[j] & BITAT(n)) == 0 && (squares[square(i, j)] & BITAT(n)) == 0;
}

/* Tries to fill the cell (i, j) with the next available number.
Returns a flag to indicate if it succeeded. */
bool advance_cell(int i, int j)
{
    int n = clear_cell(i, j);
    int z = rows[i] | cols[j] | squares[square(i, j)];
    z |= BITSTO(n);        // add 1-bits for numbers we already tried and for 0
    if (z == BITSTO(9)) {
        return false;
    }
    // The position of 1st 0-bit in z is number to try next.
    // Instead of searching for the zero in loop right here,
    // we lookup the position in our pre-calculated table:
    n = first_0bit[z];
    set_cell_unchecked(i, j, n);
    return true;
}

/* The main function, a fairly generic backtracking algorithm. */

bool solve_sudoku(int found)
{
    int pos = start_pos, filled_pos = 0;    // note that start_pos must not change between calls.
    if (found > 0) {
        // Assume the board has all cells set; find next solution.
        pos = start_pos - 1;
        if (pos < 0) pos = 80;
		// The "correct" code would be:
        // while (known[pos / 9][pos % 9]) {
		// But this uses expensive divide and modulo operations and the result
		// is just the same as this:
        while (known[0][pos]) {
            if (--pos < 0) pos = 80;
            if (pos == start_pos) {    // all cells are known cells...
                return false;
            }
        }
    }
    while (1) {
        // while (known[pos / 9][pos % 9]) {	// see "correct" code comment above.
        while (known[0][pos]) {
            if (++pos >= 81) pos = 0;
            if (pos == start_pos) {
                return true;    // All cells set: solution found; may be there are more...
            }
        }
        total_moves++;
        if (advance_cell(pos/9, pos%9)) {
            if (trace_moves) {
                print_trace("forward");
            }

            ++filled_pos;
            if (filled_pos > best_pos) {
                memcpy(best_matrix, matrix, sizeof(matrix));
                best_pos = filled_pos;
                best_pos_try_count = total_moves;
                best_pos_count = 1;
            }
            else if (filled_pos == best_pos) {
                best_pos_count++;
            }

            if (++pos >= 81) pos = 0;
            if (pos == start_pos) {
                return true;    // All cells set: solution found; may be there are more...
            }
        } else {
            if (trace_moves) {
                print_trace("back");
            }

            do {
                if (pos == start_pos) {
                    return false; // No solution found
                }
                if (--pos < 0) pos = 80;
            } while (known[0][pos]);
            // } while (known[pos / 9][pos % 9]);	// see "correct" code comment above.
        --filled_pos;
        }
    }
}

/* Return number of on (1) bits in the value passed. */
int bits_on(int n)
{
    static const int nibble[] = {   0, 1, 1, 2,
                                    1, 2, 2, 3,
                                    1, 2, 2, 3,
                                    2, 3, 3, 4 };
    unsigned int un = (unsigned int)n;
    int m = nibble[un & 15];
    while (un >>= 4) {
        m += nibble[un & 15];
    }
    return m;
}

/* Print the puzzle given as parameter. Originally known numbers are wrapped in parenthesis. */
void print_mat(int mat[9][9])
{
    for (int i = 0; i < 9; ++i) {
		int ii = 0;
		while (row_order[ii] != i)
			ii++;
        if ((i % 3) == 0) {
            print_separator();
        }
        for (int j = 0; j < 9; j++) {
            int cell = mat[ii][j];
            if ((j % 3) == 0) {
                printf("|");
            }
            if (known[ii][j]) {
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

void print_trace(char *info)
{
    printf("Matrix after move %u (%s):\n", total_moves, info ? info : "");
    print_matrix();
    if (trace_moves >= 2) {
        printf("Press Enter to continue (h for help) ...");
        char line[8] = { 0 };
        for(;;) {
            fgets(line, sizeof(line) - 1, stdin);
            switch (line[0]) {
            case 'r': trace_moves = 1; return;
            case 'g': trace_moves = 0; return;
            case 'q': exit(EXIT_SUCCESS); return;
            case '\n': return;
            }
            printf("Enter r to continue trace without stop; enter g to continue without trace\n");
        }
    }
}

/* Utility to print lines and crosses, used by print_matrix. */
void print_separator(void)
{
    for (int i = 0; i < 3; ++i) {
        printf("+---------");
    }
    printf("+\n");
}

void swap_rows_unchecked(int a, int b)
{
	int t[9];
	memcpy(t, matrix[a], sizeof(t));
	memcpy(matrix[a], matrix[b], sizeof(t));
	memcpy(matrix[b], t, sizeof(t));

	memcpy(t, known[a], sizeof(t));
	memcpy(known[a], known[b], sizeof(t));
	memcpy(known[b], t, sizeof(t));

	SWAP(rows[a], rows[b]);
	SWAP(row_order[a], row_order[b]);
}

void swap_rows(int a, int b)
{
	if (a / 3 != b / 3) {
		// do not share the same squares. Must swap three rows
		a = (a / 3) * 3;
		b = (b / 3) * 3;
		swap_rows_unchecked(a + 0, b + 0);
		swap_rows_unchecked(a + 1, b + 1);
		swap_rows_unchecked(a + 2, b + 2);
		SWAP(squares[a + 0], squares[b + 0]);
		SWAP(squares[a + 1], squares[b + 1]);
		SWAP(squares[a + 2], squares[b + 2]);
		return;
	}
	swap_rows_unchecked(a, b);
}

/* Return position of first 1-bit (the rightmost, least significant 1-bit) or -1 for zero*/
int find_first_on(int n)
{
    for (int m = 0; m < sizeof(n) * 8; ++m)
        if (n & (1 << m))
            return m;
    return -1;
}

/* Initialize lookup tables. */
void init_bits()
{
    for (int n = 0; n < _countof(first_0bit); ++n)
        first_0bit[n] = find_first_on(~n);
}
