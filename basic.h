#ifndef BASIC_H
#define BASIC_H

/**
 * @brief The main entry point for the BASIC interpreter.
 *
 * This function takes over the primary input loop, presenting the user with
 * a BASIC environment. It will only return to the caller (the shell) when
 * the user types the 'EXIT' command within the interpreter.
 */
void basic_start();

#endif // BASIC_H