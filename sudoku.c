/**
 * Command line interface sudoku solver.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

int parse_options(int argc, char **argv);
int sqare_idx(int i, int j);
bool set_cell(int i, int j, int n);
int clear_cell(int i, int j);
int init_known(int count, const char** cells, int next_row);
bool can_set(int i, int j, int n);
bool advance_cell(int i, int j);
bool solve_sudoku(int found);
void init_bits(void);
void print_matrix(void);
void print_separator(void);

/* The Sudoku matrix itself. */
int matrix[9][9];

/* Which numbers were given as known in the problem. */
int known[9][9];

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
		"usage: %s [--help] [--silent] [--max n] [-] [preset_fields...]\n"
		"    --max nnn, -m nnn   Where nnn is a number (default 2).\n"
		"                   Do not search more solutions than this.\n"
		"    --silent, -s   Do not print solution boards, count them only.\n"
		"                   Specify twice to be even more silent.\n"
		"    --help, -h, -? Print this help.\n"
		"    -           A single dash reads preset_fields from stdin, line\n"
		"                by line or in one line, separated by blanks.\n"
		"                Use io-redirection to read from a file.\n"
		"\n"
		"    preset_fields can be given in these formats:\n"
		"\n"
		"    #anything   Preset option ignored if starting with #.\n"
		"    rcn         Three digits for row, column and number.\n"
		"                Each must be in range 1..9\n"
		"                Sets number n in cell [row,column].\n"
		"                Example: '123' sets 3 to cell[1,2].\n"
		"    rc:n        Same as rcn\n"
		"                Example: '12:3' sets 3 to cell[1,2].\n"
		"    r:nnnnnnnnn 9 digits to define the complete row given by r.\n"
		"                A 0 and each non-digit means empty cell.\n"
		"                Example: '4:2..000..8' is the same as 412 498.\n"
		"    nnnnnnnnn   9 Digits defining the complete 'next' row.\n"
		"                Fills row 1 if used as first preset option.\n"
		"\n"
		"Example (a preset that usually is difficult to solve for humans):\n"
		"  118 233 246 327 359 372 425 467 554 565 577 641 683 731 786 798 838 845 881 929 975\n"
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
		char line[256], *pl[1] = { line }, *tokctx;
		while (fgets(line, sizeof(line) - 1, stdin)) {
			char *preset = strtok_s(line, " \r\n", &tokctx);
			while (preset) {
				next_row = init_known(1, pl, next_row);
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

	if (found >= find_max) {
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
		printf("No solution found:\n");
		print_matrix();
	}

    return EXIT_SUCCESS;
}

int parse_options(int argc, char **argv)
{
	int argc0 = argc;
	while (argc > 0 && argv[0][0] == '-' && argv[0][1]) {	// is not just a single '-'
		char *opt = 1 + *argv++; argc--;
		char *arg = NULL;
		if (*opt == '-') {	// this is potentially --opt[=arg]
			if (!*++opt)
				break;		// just "--" stops option parsing
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
int sqare_idx(int i, int j)
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
    squares[sqare_idx(i, j)] |= bits[n];
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
    squares[sqare_idx(i, j)] &= ~bits[n];
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
		if (cell[0] == '#')	// commented out - ignore this preset.
			continue;
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
				if (n < 0 || n > 9)		// treat everything except digits as 0
					n = 0;
				m = clear_cell(i, j);
				if (m != 0)
					fprintf(stderr, "token #%d \"%s\": overwrites %d at [%d,%d]\n", c + 1, cell, m, i + 1, j + 1);
				if (n != 0 && !set_cell(i, j, n)) {
					fprintf(stderr, "token #%d \"%s\": setting %d breaks rules\n", c + 1, cell, n);
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
				fprintf(stderr, "token #%d \"%s\": setting %d breaks rules\n", c+1, cell, n);
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
    return (rows[i] & bits[n]) == 0 && (cols[j] & bits[n]) == 0 && (squares[sqare_idx(i, j)] & bits[n]) == 0;
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
		if (pos < 0) {	// all cells are known cells...
			return false;
		}
	}
    while (1) {
        while (pos < 81 && known[pos/9][pos%9]) {
            ++pos;
        }
        if (pos >= 81) {
            return true;	// All cells set: solution found; may be there are more...
        }
        if (advance_cell(pos/9, pos%9)) {
            ++pos;
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

/* Prints the matrix using some ANSI escape sequences
to distinguish the originally known numbers. */
void print_matrix(void)
{
    for (int i = 0; i < 9; ++i) {
        if ((i % 3) == 0) {
            print_separator();
        }
        for (int j = 0; j < 9; j++) {
            int cell = matrix[i][j];
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

/* Utility to print lines and crosses, used by print_matrix. */
void print_separator(void)
{
    for (int i = 0; i < 3; ++i) {
        printf("+---------");
    }
    printf("+\n");
}
