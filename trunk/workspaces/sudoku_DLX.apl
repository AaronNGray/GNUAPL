#!apl --script

      )WSID sudoku_DLX

      ⎕PW←400  ⍝ for displaying many constraints columns

SUDO_p5 ← ⊃"""
╔═══╤═══╤═══╦═══╤═══╤═══╦═══╤═══╤═══╗
║ 2 │   │   ║   │   │ 5 ║   │   │ 7 ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │ 4 │   ║ 6 │   │ 8 ║   │ 9 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║ 1 │   │ 9 ║   │   │   ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║   │ 8 │ 3 ║   │   │   ║ 7 │ 4 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║   │   │   ║   │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║ 1 │ 7 │ 9 ║   │   │   ║ 6 │ 5 │   ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║   │   │   ║ 9 │   │ 4 ║   │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │ 5 │   ║ 8 │   │ 3 ║   │ 1 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║ 4 │   │   ║   │   │   ║   │   │ 8 ║
╚═══╧═══╧═══╩═══╧═══╧═══╩═══╧═══╧═══╝
"""

SUDO_sdk_10 ← ⊃"""
╔═══╤═══╤═══╦═══╤═══╤═══╦═══╤═══╤═══╗
║   │ 9 │   ║   │   │ 8 ║ 1 │ 6 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │ 1 ║ 7 │   │   ║ 9 │ 2 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║ 2 │   │   ║   │   │   ║   │   │ 3 ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║ 3 │   │ 8 ║ 4 │   │ 1 ║ 5 │   │ 6 ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │ 4 │ 9 ║ 8 │   │   ║ 2 │ 3 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║ 6 │   │   ║   │   │ 7 ║ 8 │   │ 4 ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║ 8 │   │   ║ 6 │ 4 │   ║   │   │ 9 ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║   │ 7 │ 2 ║ 6 │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │ 6 │ 7 ║ 1 │ 8 │   ║   │ 5 │ 2 ║
╚═══╧═══╧═══╩═══╧═══╧═══╩═══╧═══╧═══╝
"""


SUDO_sdk_16 ← ⊃"""
╔═══╤═══╤═══╦═══╤═══╤═══╦═══╤═══╤═══╗
║ 2 │   │ 7 ║   │ 4 │   ║ 6 │   │ 8 ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║ 4 │ 3 │ 5 ║ 9 │ 6 │ 8 ║ 7 │ 1 │ 2 ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │ 6 │ 8 ║ 7 │   │   ║   │   │   ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║ 5 │ 4 │ 1 ║ 6 │ 3 │ 9 ║ 2 │ 8 │ 7 ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║ 6 │ 7 │ 2 ║   │ 8 │   ║   │   │ 3 ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║ 3 │ 8 │ 9 ║ 2 │ 7 │ 5 ║ 4 │ 6 │ 1 ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║   │   │   ║   │   │ 6 ║   │ 7 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║ 8 │ 5 │ 6 ║ 3 │   │ 7 ║ 1 │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║ 7 │   │   ║   │ 1 │   ║   │   │ 6 ║
╚═══╧═══╧═══╩═══╧═══╧═══╩═══╧═══╧═══╝
"""

SUDO_sdk_23 ← ⊃"""
╔═══╤═══╤═══╦═══╤═══╤═══╦═══╤═══╤═══╗
║ 5 │   │ 2 ║   │   │   ║ 4 │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║ 7 │ 1 │   ║   │   │ 3 ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║   │   │   ║   │   │   ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║   │   │   ║   │   │ 4 ║ 6 │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │ 7 │   ║ 2 │   │   ║   │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │ 1 │   ║   │   │   ║   │   │   ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║ 6 │   │   ║   │   │ 2 ║   │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║   │ 3 │   ║   │ 1 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║ 4 │   │   ║   │   │   ║   │   │   ║
╚═══╧═══╧═══╩═══╧═══╧═══╩═══╧═══╧═══╝
"""

      SUDO_5←⊃"""
╔═══╤═══╤═══╦═══╤═══╤═══╦═══╤═══╤═══╗
║   │   │ 9 ║ 5 │   │ 6 ║ 1 │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │ 6 │ 7 ║   │ 1 │   ║   │ 9 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║ 1 │   │ 8 ║   │   │   ║ 5 │ 4 │ 6 ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║ 7 │   │   ║ 4 │   │ 2 ║   │   │ 5 ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │ 5 │   ║   │ 8 │   ║   │ 7 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║ 4 │   │   ║ 7 │   │ 9 ║   │   │ 8 ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║ 2 │ 3 │ 4 ║   │   │   ║ 8 │   │ 1 ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │ 9 │   ║   │ 3 │   ║ 7 │ 2 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │ 1 ║ 2 │   │ 8 ║ 3 │   │   ║
╚═══╧═══╧═══╩═══╧═══╧═══╩═══╧═══╧═══╝
"""

      SUDO_Xtian←⊃"""
╔═══╤═══╤═══╦═══╤═══╤═══╦═══╤═══╤═══╗
║ 4 │   │ 5 ║   │   │ 2 ║   │ 7 │ 1 ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║   │   │   ║   │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │ 9 ║   │ 4 │   ║   │   │ 6 ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║   │   │   ║ 9 │   │   ║ 8 │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │ 3 │   ║   │   │ 6 ║   │ 4 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║   │   │   ║ 3 │   │   ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║   │ 1 │   ║   │ 6 │   ║   │ 8 │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │ 7 │   ║   │ 2 │ 1 ║   │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │ 8 ║   │   │   ║   │   │   ║
╚═══╧═══╧═══╩═══╧═══╧═══╩═══╧═══╧═══╝
"""

      SUDO_EMPTY←⊃"""
╔═══╤═══╤═══╦═══╤═══╤═══╦═══╤═══╤═══╗
║   │   │   ║   │   │   ║   │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║   │   │   ║   │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║   │   │   ║   │   │   ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║   │   │   ║   │   │   ║   │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║   │   │   ║   │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║   │   │   ║   │   │   ║
╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣
║   │   │   ║   │   │   ║   │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║   │   │   ║   │   │   ║
╟───┼───┼───╫───┼───┼───╫───┼───┼───╢
║   │   │   ║   │   │   ║   │   │   ║
╚═══╧═══╧═══╩═══╧═══╧═══╩═══╧═══╧═══╝
"""

∇Z←DECORATE SUDO;ROWS;COLS
 ⍝
 ⍝ A raw sudoko is a 9×9 numeric matrix with values 0 (for empty fields) and
 ⍝ 1-9 for placed digits.
 ⍝
 ⍝ A decorated sudoku is a 19×37 character matrix which also contains line
 ⍝ graphic characters between the 9×9 fields (like the sudokus shown above).
 ⍝
 ⍝ This function converts a raw sudoko into a decorated sudoku, and vice versa.
 ⍝
 DX←37⍴0 0 1 0 ◊ DY←19⍴0 1   ⍝ the relevant columns and rows
 →(9 9≡⍴SUDO)⍴RAW_TO_DECORATED ◊ Z←' 123456789'⍳DY⌿DX/SUDO ◊ →0
RAW_TO_DECORATED: Z←SUDO_EMPTY ◊ (DY⌿DX/Z)←⎕UCS 48+SUDO-16×SUDO=0
∇

∇Z←SETUP_CONSTRAINTS;ROW;COL;DIG;DC;I
 ⍝
 ⍝ Return the constraints matrix of the empty sudoku. This matrix uses
 ⍝ characters 1-9 and spaces instead of the "normal" integers 0 and 1 for
 ⍝ a constraints matrix.
 ⍝
 I←0 ◊ Z←729 324⍴' '
LOOP: (ROW COL DIG)←9 9 9⊤I ◊ DC←⎕UCS 49+DIG 
 Z[I;(9⊥ROW COL),(81+9⊥ROW DIG),(162+9⊥COL DIG),243+9⊥(3⊥2⍴3 3⊤ROW COL),DIG]←DC
 →(729>I←I+1)⍴LOOP
∇

∇A SHOW SOLUTION;SUDO_OUT;Q
 ⍝
 ⍝ Display the input sudoku A and its (decorated) solution
 ⍝
 SUDO_OUT←A
 ⊣{SUDO_OUT[R;C]←D+1 ⊣ (R C D)←9 9 9⊤⍵;R;C;D}¨⊃⍬⍴SOLUTION   ⍝ apply moves
 (DECORATE A) (DECORATE SUDO_OUT)
 →0
 ⍝ check result
 ⍝
 { Q[⍋Q←,SUDO_OUT[⍵;]] ≡ 1+⍳9 } ¨ ⍳9
 { Q[⍋Q←,SUDO_OUT[;⍵]] ≡ 1+⍳9 } ¨ ⍳9
 { Q[⍋Q←,SUDO_OUT[0 1 2 + ⍵[1];0 1 2 + ⍵[0]]] ≡ 1+⍳9 } ¨ ,3×⍳3 3
∇

∇SOLVE SUDO;⎕IO;RAW;CONSTRAINTS;WL;SOLUTIONS;FROM
 ⍝
 ⍝ solve (decorated) sudoku SUDO
 ⍝
 FROM←⎕TS

 ⍝ Step 1: setup the constraints matrix for an empty sudoku
 ⍝
 ⎕IO←0   ⍝ this simplifies ⊥ and ⊤
 CONSTRAINTS←SETUP_CONSTRAINTS ◊ RAW←DECORATE SUDO

 ⍝ Step 2: create a worklist WL of RCDs that shall be placed
 ⍝
 WL←9⊥⍉(WL≥0)⌿(9/⍳9), (81⍴⍳9), 81 1⍴+WL←,RAW-1   ⍝ map digits 1-9 to 0-8
 'Number of digits pre-placed:    ' (⍬⍴⍴WL)

 ⍝ Step 3: place every RCD in WL onto the sudoku. Placing an RCD causes some
 ⍝         some rows of the constraints matrix to be cleared and some columns
 ⍝         to be removed from the matrix.
 ⍝
 CONSTRAINTS← (¯4, WL) ⎕DLX CONSTRAINTS≠' '

 ⍝ Step 4: solve remaining constraints matrix CONSTRAINTS using ⎕DLX
 ⍝
 'Size of the constraints matrix:' (⍴CONSTRAINTS)
 'Number of non-empty rows:       ' (729-+/0=+/CONSTRAINTS)
 SOLUTIONS←¯1 ⎕DLX CONSTRAINTS
 '' ◊ 'Time:' (0.001×0 60 60 1000⊥¯4↑⎕TS - FROM) 'seconds'

 'Number of solutions:            ' (⍬⍴⍴SOLUTIONS)

 ⍝ Step 5: display the solution(s) (if any)
 ⍝
 →(0=⍴SOLUTIONS)⍴0
 '' ◊ ' first solution:' ◊ RAW SHOW  1↑SOLUTIONS ◊ →(1=⍴SOLUTIONS)⍴0
 '' ◊ ' last solution:'  ◊ RAW SHOW ¯1↑SOLUTIONS
∇

      SOLVE SUDO_p5
      SOLVE SUDO_sdk_10
      SOLVE SUDO_sdk_16
      SOLVE SUDO_sdk_23
      SOLVE SUDO_5
      SOLVE SUDO_Xtian

      )OFF
