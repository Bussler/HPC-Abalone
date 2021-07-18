# HPC-Abalone
Project for the TUM master's course on high performance computing in the summer term 2021. Implemented is the Abalone game and a parallelized strategy to efficiently search the game tree and compute the next best move.

The general implementation for the Abalone game was given to us by the chair of Computer Architecture and Parallel Systems. Our own implementation is the parallelized game-tree search strategy in `Abalone/ab/search-minimax.cpp`.


## Dependencies
- OpenMp (to use the parallelism)
- Access to a hpc system (can also be run locally, but the amount of used threads should then be changed to suit the local machine)


## How to use
A basic tutorial on how to use the program can be found in `Abalone/ab/README`.

The program `player` encapsulates the gameplay and search-strategy for each agent, while `referee` handles the sending of the current board to the players. The program can be compiled by using make.

In order to use the player program with our implemented parallel search strategy, one has to specify the flags `-s <strategynum> <maxSearchDepth>` when starting the player. Since we implemented the third search strategy, our `<strategynum>` is `2`. Example: `-s 2 5`.

When testing the implementation, the flags `-n -1` can be used to keep a constant evaluation function between runs and cancel the game after playing one move. Deciding the color of the player can be done with the `X` and `O` flags.

### Example
Open three shells on the project. Compile the program by typing in `make` into one shell.

On the first shell: Start the O player by typing in `./player O -s 2 5`.

On the second shell: Start the X player by typing in `./player X -s 2 5`.

On the third shell: Start the referee by typing in `./referee`.
