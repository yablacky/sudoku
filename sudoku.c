#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

int sqare_idx(int i, int j);
bool set_cell(int i, int j, int n);
int clear_cell(int i, int j);
void init_known(int count, const char** cells);
bool can_set(int i, int j, int n);
bool advance_cell(int i, int j);
void solve_sudoku();
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


int main(int argc, char** argv)
{
	if (argc >= 2 && (
		!strcmp(argv[1], "-h") ||
		!strcmp(argv[1], "--help") ||
		!strcmp(argv[1], "/?") ||
		!strcmp(argv[1], "-?"))) {
		printf(
			"usage: %s [-h|--help|-?|/?] [preset_fields...]\n"
			"    preset_fields understands these formats:\n"
			"    rcn         Three digits for row, column and number.\n"
			"                Each must be in range 1..9\n"
			"                Sets number n in cell [row,column].\n"
			"                Example: '123' sets 3 to cell[1,2].\n"
			"    rc:n        Same as rcn\n"
			"                Example: '12:3' sets 3 to cell[1,2].\n"
			"    r:nnnnnnnnn 9 digits to define the complete row given by r.\n"
			"                Each non-digit will leave a field empty.\n"
			"                Example: '4:...246..8' is the same as 442 454 466 498.\n"
			"    nnnnnnnnn   9 Digits defining the complete 'next' row.\n"
			, argv[0]);
		exit(EXIT_FAILURE);
	}

    init_bits();
    init_known(argc-1, argv+1);

    solve_sudoku();
    print_matrix();

    return EXIT_SUCCESS;
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
void init_known(int count, const char** cells)
{
	int nerr = 0, i = 0;
    for (int c = 0; c < count; ++c, ++i) {
        const char* cell = cells[c], *row = NULL;
		char r;
        int j, n, m;
		if (!strchr(cell, ':') && strlen(cell) == 9) {
			row = cell;
			if (i < 0 || i > 8) {
				fprintf(stderr, "token #%d \"%s\": too many rows, got %d\n", c + 1, cell, i + 1);
				nerr++;
				continue;
			}
		}
		else if (sscanf(cell, "%1d:%n%1c", &i, &n, &r) == 2) {
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
		else if (sscanf(cell, "%1d%1d%1d%1c", &i, &j, &n, &r) == 3 ||
				 sscanf(cell, "%1d%1d:%1d%1c", &i, &j, &n, &r) == 3) {
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
	if (nerr != 0)
		exit(EXIT_FAILURE);
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
void solve_sudoku(void)
{
	int pos = 0;
    while (1) {
        while (pos < 81 && known[pos/9][pos%9]) {
            ++pos;
        }
        if (pos >= 81) {
            break;	// all cells set: solution found; may be there are more...
        }
        if (advance_cell(pos/9, pos%9)) {
            ++pos;
        } else {
            do {
                --pos;
            } while (pos >= 0 && known[pos/9][pos%9]);
            if (pos < 0) {
                break; // no solution found
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
